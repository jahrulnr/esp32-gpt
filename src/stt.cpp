#include "stt.h"
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <FS.h>
#include "core.h"

// Available transcription models
static const GPTSttModel AVAILABLE_MODELS[] = {
	{"gpt-4o-transcribe", "GPT-4o Transcribe"},
	{"gpt-4o-mini-transcribe", "GPT-4o Mini Transcribe"},
	{"whisper-1", "Whisper v1"}
};

static const size_t NUM_MODELS = sizeof(AVAILABLE_MODELS) / sizeof(AVAILABLE_MODELS[0]);

GPTSttService::GPTSttService()
	: _model("gpt-4o-transcribe")
	, _initialized(false)
	, _fs(nullptr)
{
}

GPTSttService::~GPTSttService() {
	// Clean up if needed
}

bool GPTSttService::init(const String& apiKey, fs::FS& fs) {
	if (apiKey.length() == 0) {
		ESP_LOGE("TRANSCRIPTION", "API key is empty");
		return false;
	}

	_apiKey = apiKey;
	_fs = &fs;
	_initialized = true;

	ESP_LOGI("TRANSCRIPTION", "Transcription service initialized with model: %s", _model.c_str());
	return true;
}

String GPTSttService::buildMultipartPayload(const String& filePath, const String& boundary) {
	String payload = "";

	// Add file part
	payload += "--" + boundary + "\r\n";
	payload += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filePath.substring(filePath.lastIndexOf('/') + 1) + "\"\r\n";
	payload += "Content-Type: audio/wav\r\n\r\n";

	// Read file content
	File file = _fs->open(filePath, "r");
	if (!file) {
		ESP_LOGE("TRANSCRIPTION", "Failed to open file: %s", filePath.c_str());
		return "";
	}

	int bufferSize = 1024 * 100;
	uint8_t* buffer = (uint8_t*) heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
	if (!buffer) {
		ESP_LOGE("TRANSCRIPTION", "Failed to allocate memory for file content");
		file.close();
		return "";
	}
	
	while (file.available()) {
		size_t bytes = file.read(buffer, bufferSize);
		payload += String((const char*) buffer, bytes);
	}
	delete[] buffer;
	file.close();

	payload += "\r\n";

	// Add model part
	payload += "--" + boundary + "\r\n";
	payload += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
	payload += _model + "\r\n";

	// End boundary
	payload += "--" + boundary + "--\r\n";

	return payload;
}

void GPTSttService::transcribeAudio(const String& filePath, TranscriptionCallback callback) {
	transcribeAudio(filePath, _model, callback);
}

void GPTSttService::transcribeAudio(const String& filePath, const String& model, TranscriptionCallback callback) {
	if (!_initialized) {
		ESP_LOGE("TRANSCRIPTION", "Transcription service not initialized");
		callback(filePath, "", "{}");
		return;
	}

	if (!WiFi.isConnected()) {
		ESP_LOGE("TRANSCRIPTION", "No WiFi connection");
		callback(filePath, "", "{}");
		return;
	}

	// Check if file exists
	if (!_fs->exists(filePath)) {
		ESP_LOGE("TRANSCRIPTION", "Audio file does not exist: %s", filePath.c_str());
		callback(filePath, "", "{}");
		return;
	}

	// Set model for this request
	String originalModel = _model;
	_model = model;

	// Generate boundary
	String boundary = "----ESP32FormBoundary" + String(random(1000000));

	// Build multipart payload
	String multipartPayload = buildMultipartPayload(filePath, boundary);
	if (multipartPayload.length() == 0) {
		ESP_LOGE("TRANSCRIPTION", "Failed to build multipart payload");
		_model = originalModel;
		callback(filePath, "", "{}");
		return;
	}

	// Restore original model
	_model = originalModel;

	// Create async task for HTTP request
	xTaskCreatePinnedToCore([](void* param) {
		auto* params = static_cast<std::tuple<GPTSttService*, String, String, String, TranscriptionCallback>*>(param);
		auto& [service, payload, file, bnd, cb] = *params;

		gptWifiClient.setInsecure(); // For HTTPS without certificate validation
		gptHttp.begin(gptWifiClient, "https://api.openai.com/v1/audio/transcriptions");
    gptHttp.setReuse(false);
		gptHttp.addHeader("Content-Type", "multipart/form-data; boundary=" + bnd);
		gptHttp.addHeader("Authorization", "Bearer " + service->_apiKey);
		gptHttp.setTimeout(30000); // 30 second timeout

		ESP_LOGI("TRANSCRIPTION", "Sending transcription request to OpenAI API...");
		ESP_LOGI("TRANSCRIPTION", "File: %s", file.c_str());
		ESP_LOGI("TRANSCRIPTION", "Model: %s", service->_model.c_str());

		int httpCode = gptHttp.POST(payload);

		if (httpCode == 200) {
			String response = gptHttp.getString();
			ESP_LOGI("TRANSCRIPTION", "Transcription successful");
			service->processResponse(httpCode, response, file, cb);
		} else {
			String response = gptHttp.getString();
			ESP_LOGE("TRANSCRIPTION", "API returned error code: %d", httpCode);
			service->processResponse(httpCode, response, file, cb);
		}

		gptHttp.end();
		delete params;
		params = nullptr;
		vTaskDelete(NULL);
	}, "Transcription_Request", 16384, new std::tuple<GPTSttService*, String, String, String, TranscriptionCallback>(this, multipartPayload, filePath, boundary, callback), 1, NULL, 0);
}

void GPTSttService::processResponse(int httpCode, const String& response, const String& filePath, TranscriptionCallback callback) {
	if (httpCode == 200) {
		JsonDocument doc;
		DeserializationError error = deserializeJson(doc, response);

		if (error) {
			ESP_LOGE("TRANSCRIPTION", "Failed to parse JSON response: %s", error.c_str());
			callback(filePath, "", "{}");
			return;
		}

		String transcription = doc["text"] | "";
		String usageJson;
		serializeJson(doc["usage"], usageJson);

		ESP_LOGI("TRANSCRIPTION", "Transcription: %s", transcription.c_str());
		callback(filePath, transcription, usageJson);
		usageJson.clear();
	} else {
		ESP_LOGE("TRANSCRIPTION", "Transcription failed with code: %d", httpCode);

		// Try to extract error message
		JsonDocument errorDoc;
		if (deserializeJson(errorDoc, response) == DeserializationError::Ok) {
			if (errorDoc["error"].is<JsonObject>()) {
				String errorMsg = errorDoc["error"]["message"] | "Unknown API error";
				ESP_LOGE("TRANSCRIPTION", "API Error: %s", errorMsg.c_str());
			}
		}

		callback(filePath, "", "{}");
	}
}

std::vector<gpt_transcription_t> GPTSttService::getAvailableModels() {
	return std::vector<gpt_transcription_t>(AVAILABLE_MODELS, AVAILABLE_MODELS + NUM_MODELS);
}

GPTSttService aiStt;