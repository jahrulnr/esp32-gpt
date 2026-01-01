#include "gpt.h"
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include "core.h"

// Affordable GPT models sorted by cost (cheapest first)
static const GPTModel AFFORDABLE_MODELS[] = {
	{"gpt-5-nano", "GPT-5 Nano"},
	{"gpt-4.1-nano", "GPT-4.1 Nano"},
	{"gpt-4o-mini"},
	{"gpt-5-mini", "GPT-5 Mini"},
	{"gpt-4.1-mini", "GPT-4.1 Mini"},
	{"o3-mini", "O3 Mini"},
	{"o1-mini", "O1 Mini"}
};

static const size_t NUM_AFFORDABLE_MODELS = sizeof(AFFORDABLE_MODELS) / sizeof(AFFORDABLE_MODELS[0]);

GPTService::GPTService()
	: _model("gpt-5-nano")
	, _systemMessage("Respond with thoughtful pauses (\"Hmm...\", \"Well...\") and be curious. Keep answers under 250 characters, playful, and supportive. Offer quick reflections, light humor, and gentle encouragement. Do not use any emoticons or emojis.")
	, _initialized(false)
	, _contextCache(10) // Keep last 10 messages
	, _previousResponseId("") // Initialize empty for first conversation
	, _storeResponse(true)
{
}

GPTService::~GPTService() {
	// Clean up if needed
}

bool GPTService::init(const String& apiKey) {
	if (apiKey.length() == 0) {
		ESP_LOGE("GPT", "API key is empty");
		return false;
	}

	_apiKey = apiKey;
	_initialized = true;

	ESP_LOGI("GPT", "GPT service initialized with model: %s", _model.c_str());
	return true;
}

String GPTService::buildJsonPayload(const String& userPrompt, const std::vector<std::pair<String, String>>& contextMessages) {
	JsonDocument doc;

	doc["model"] = _model;
	doc["input"] = userPrompt;
	doc["instructions"] = _systemMessage;

	JsonObject reasoning = doc["reasoning"].to<JsonObject>();
	reasoning["effort"] = "low";

	// Include previous response ID for conversation continuity
	if (_storeResponse && !_previousResponseId.isEmpty()) {
		doc["previous_response_id"] = _previousResponseId;
	}

	doc["store"] = _storeResponse; // store response to gpt

	String jsonString;
	serializeJson(doc, jsonString);
	return jsonString;
}

void GPTService::sendPrompt(const String& prompt, ResponseCallback callback) {
	sendPromptWithContext(prompt, {}, callback);
}

void GPTService::sendPrompt(const String& prompt, const String& additionalContext, ResponseCallback callback) {
	std::vector<std::pair<String, String>> contextMessages;
	if (additionalContext.length() > 0) {
		contextMessages.push_back({"system", additionalContext});
	}
	sendPromptWithContext(prompt, contextMessages, callback);
}

void GPTService::sendPromptWithContext(const String& prompt,
									  const std::vector<std::pair<String, String>>& contextMessages,
									  ResponseCallback callback) {
	if (!_initialized) {
		ESP_LOGE("GPT", "GPT service not initialized");
		callback(prompt, "Error: GPT service not initialized");
		return;
	}

	if (!WiFi.isConnected()) {
		ESP_LOGE("GPT", "No WiFi connection");
		callback(prompt, "Error: No internet connection");
		return;
	}

	// Add user message to context cache
	_contextCache.addMessage("user", prompt);

	// Build JSON payload
	String jsonPayload = buildJsonPayload(prompt, contextMessages);

	// Create async task for HTTP request (since GPT API calls are slow)
	xTaskCreatePinnedToCore([](void* param) {
		auto* params = static_cast<std::tuple<GPTService*, String, ResponseCallback>*>(param);
		auto& [service, payload, cb] = *params;

		gptWifiClient.setInsecure(); // For HTTPS without certificate validation
		gptHttp.begin(gptWifiClient, "https://api.openai.com/v1/responses");
		gptHttp.setReuse(false);
		gptHttp.addHeader("Content-Type", "application/json");
		gptHttp.addHeader("Authorization", "Bearer " + service->_apiKey);
		gptHttp.setTimeout(30000); // 30 second timeout

		ESP_LOGI("GPT", "Sending request to OpenAI API...");

		int httpCode = gptHttp.POST(payload);

		if (httpCode > 0) {
			String response = gptHttp.getString();
			ESP_LOGI("GPT", "API response received, code: %d", httpCode);
			service->processResponse(httpCode, response, payload, cb);
		} else {
			ESP_LOGE("GPT", "HTTP request failed, error: %d", httpCode);
			cb(payload, "Error: Failed to connect to GPT API");
		}

		gptHttp.end();
		delete params;
		params = nullptr;
		vTaskDelete(NULL);
	}, "GPT_Request", 8192, new std::tuple<GPTService*, String, ResponseCallback>(this, jsonPayload, callback), 1, NULL, 1);
}

void GPTService::processResponse(int httpCode, const String& response, const String& userPrompt, ResponseCallback callback) {
	if (httpCode != 200) {
		ESP_LOGE("GPT", "API returned error code: %d", httpCode);

		// Try to extract error message from JSON
		JsonDocument errorDoc;
		if (deserializeJson(errorDoc, response) == DeserializationError::Ok) {
			if (errorDoc["error"].is<JsonObject>()) {
				String errorMsg = errorDoc["error"]["message"] | "Unknown API error";
				callback(userPrompt, "Error: " + errorMsg);
				return;
			}
		}

		callback(userPrompt, "Error: GPT API returned code " + String(httpCode));
		return;
	}

	// Parse successful response
	String gptResponse = extractResponse(response);
	if (gptResponse.length() > 0) {
		// Add assistant response to context cache
		_contextCache.addMessage("assistant", gptResponse);

		ESP_LOGI("GPT", "Response generated successfully (%d chars)", gptResponse.length());
		callback(userPrompt, gptResponse);
	} else {
		ESP_LOGE("GPT", "Failed to extract response from API reply");
		callback(userPrompt, "Error: Could not parse GPT response");
	}
}

String GPTService::extractResponse(const String& jsonResponse) {
	JsonDocument doc;

	DeserializationError error = deserializeJson(doc, jsonResponse);
	if (error) {
		ESP_LOGE("GPT", "JSON parse error: %s", error.c_str());
		return "";
	}

	// Extract and store response ID for conversation continuity
	if (_storeResponse && doc["id"].is<String>()) {
		_previousResponseId = doc["id"].as<String>();
	}

	// Navigate to the response content (Responses API format)
	if (!doc["output"].is<JsonArray>() || doc["output"].size() == 0) {
		ESP_LOGE("GPT", "No output in response. response: %s", doc.as<String>().c_str());
		return "";
	}

	JsonDocument outputItem;
	for(auto outputI : doc["output"].as<JsonArray>()){
		if(outputI["type"] == "message") {
			outputItem.set(outputI);
			ESP_LOGI("GPT", "Found message in output item. message: %s", outputI.as<String>().c_str());
			break;
		}
	}

	if (!outputItem["content"].is<JsonArray>() || outputItem["content"].size() == 0) {
		ESP_LOGE("GPT", "No content in output item. response: %s", doc.as<String>().c_str());
		return "";
	}

	JsonObject contentItem = outputItem["content"][0];
	if (!contentItem["text"].is<String>()) {
		ESP_LOGE("GPT", "No text in content item. response: %s", doc.as<String>().c_str());
		return "";
	}

	String content = contentItem["text"];
	content.trim(); // Remove any leading/trailing whitespace
	if (content.length() == 0) {
		ESP_LOGE("GPT", "Empty content in response. response: %s", doc.as<String>().c_str());
		return "";
	};

	return content;
}

std::vector<GPTModel> GPTService::getAvailableModels() {
	return std::vector<GPTModel>(AFFORDABLE_MODELS, AFFORDABLE_MODELS + NUM_AFFORDABLE_MODELS);
}

void GPTService::resetConversation() {
	_previousResponseId = "";
	_contextCache.clear();
	ESP_LOGI("GPT", "Conversation state reset");
}

GPTService ai;