#include "tts.h"
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <vector>

// Available TTS voices
static const GPTTtsVoice AVAILABLE_VOICES[] = {
	{"alloy", "Alloy"},
	{"echo", "Echo"},
	{"fable", "Fable"},
	{"onyx", "Onyx"},
	{"nova", "Nova"},
	{"shimmer", "Shimmer"}
};

static const size_t NUM_VOICES = sizeof(AVAILABLE_VOICES) / sizeof(AVAILABLE_VOICES[0]);

GPTTtsService::GPTTtsService()
	: _model("gpt-4o-mini-tts")
	, _voice("shimmer")
	, _initialized(false)
{
}

GPTTtsService::~GPTTtsService() {
	// Clean up if needed
}

bool GPTTtsService::init(const String& apiKey) {
	if (apiKey.length() == 0) {
		ESP_LOGE("TTS", "API key is empty");
		return false;
	}

	_apiKey = apiKey;
	_initialized = true;

	ESP_LOGI("TTS", "TTS service initialized with model: %s, voice: %s", _model.c_str(), _voice.c_str());
	return true;
}

String GPTTtsService::buildJsonPayload(const String& text) {
	JsonDocument doc;

	doc["model"] = _model;
	doc["input"] = text;
	doc["voice"] = _voice;

	String jsonString;
	serializeJson(doc, jsonString);
	return jsonString;
}

void GPTTtsService::textToSpeech(const String& text, AudioCallback callback) {
	textToSpeech(text, _voice, callback);
}

void GPTTtsService::textToSpeech(const String& text, const String& voice, AudioCallback callback) {
	if (!_initialized) {
		ESP_LOGE("TTS", "TTS service not initialized");
		callback(text, nullptr, 0);
		return;
	}

	if (!WiFi.isConnected()) {
		ESP_LOGE("TTS", "No WiFi connection");
		callback(text, nullptr, 0);
		return;
	}

	if (text.length() == 0) {
		ESP_LOGE("TTS", "Text is empty");
		callback(text, nullptr, 0);
		return;
	}

	// Set voice for this request
	String originalVoice = _voice;
	_voice = voice;

	// Build JSON payload
	String jsonPayload = buildJsonPayload(text);

	// Restore original voice
	_voice = originalVoice;

	// Create async task for HTTP request
	xTaskCreatePinnedToCore([](void* param) {
		auto* params = static_cast<std::tuple<GPTTtsService*, String, String, AudioCallback>*>(param);
		auto& [service, payload, txt, cb] = *params;

		HTTPClient http;
		WiFiClientSecure client;
		client.setInsecure(); // For HTTPS without certificate validation

		http.begin(client, "https://api.openai.com/v1/audio/speech");
		http.setReuse(false);
		http.addHeader("Content-Type", "application/json");
		http.addHeader("Accept", "*/*");
		http.addHeader("Authorization", "Bearer " + service->_apiKey);
		http.setTimeout(30000); // 30 second timeout

		// Collect response headers
		const char* headerKeys[] = {
			"Content-Type", 
			"Content-Length", 
			"Transfer-Encoding", 
			"Connection", 
		};
		http.collectHeaders(headerKeys, sizeof(headerKeys)/sizeof(headerKeys[0]));

		// Print all headers being sent
		ESP_LOGI("TTS", "=== TTS Request Headers ===");
		ESP_LOGI("TTS", "Content-Type: application/json");
		ESP_LOGI("TTS", "Accept: */*");
		ESP_LOGI("TTS", "Authorization: Bearer [REDACTED]");
		ESP_LOGI("TTS", "URL: https://api.openai.com/v1/audio/speech");
		ESP_LOGI("TTS", "Payload: %s", payload.c_str());
		ESP_LOGI("TTS", "==========================");

		ESP_LOGI("TTS", "Sending TTS request to OpenAI API...");

		int httpCode = http.POST(payload);

		if (httpCode == 200) {
			// Get audio data - handle both Content-Length and chunked transfer encoding
			WiFiClient* stream = http.getStreamPtr();
			int contentLength = http.getSize();
			
			// For chunked responses, contentLength will be -1
			// We'll read data until stream ends
			const size_t BUFFER_SIZE = 4096;
			uint8_t buffer[BUFFER_SIZE];
			size_t totalBytesRead = 0;
			std::vector<uint8_t> audioData;
			
			ESP_LOGI("TTS", "Starting to read audio data (Content-Length: %d)", contentLength);
			
			unsigned long startTime = millis();
			const unsigned long INITIAL_TIMEOUT_MS = 10000; // 10 seconds to receive first data
			const unsigned long INTER_CHUNK_TIMEOUT_MS = 2000; // 2 seconds between chunks
			unsigned long lastDataTime = 0;
			size_t consecutiveZeroReads = 0;
			const size_t MAX_ZERO_READS = 10;
			bool dataReceived = false;
			bool initialTimeoutActive = true;
			
			while (true) {
				unsigned long currentTime = millis();
				size_t bytesAvailable = stream->available();
				
				// Check timeout conditions
				if (!dataReceived) {
					// Still waiting for first data
					if (currentTime - startTime >= INITIAL_TIMEOUT_MS) {
						ESP_LOGW("TTS", "Initial timeout: no data received after %lu ms", INITIAL_TIMEOUT_MS);
						break;
					}
				} else {
					// Data received, use inter-chunk timeout
					if (currentTime - lastDataTime >= INTER_CHUNK_TIMEOUT_MS) {
						ESP_LOGI("TTS", "Inter-chunk timeout: no data for %lu ms after receiving %d bytes", 
								INTER_CHUNK_TIMEOUT_MS, totalBytesRead);
						break;
					}
				}
				
				if (bytesAvailable == 0) {
					consecutiveZeroReads++;
					if (consecutiveZeroReads >= MAX_ZERO_READS) {
						if (dataReceived) {
							ESP_LOGI("TTS", "Stream ended: %d consecutive zero reads after receiving data", consecutiveZeroReads);
						} else {
							ESP_LOGW("TTS", "Stream ended: %d consecutive zero reads, no data received", consecutiveZeroReads);
						}
						break;
					}
					// Wait a bit for more data
					delay(100);
					continue;
				}
				
				// Data is available!
				consecutiveZeroReads = 0;
				dataReceived = true;
				lastDataTime = currentTime;
				initialTimeoutActive = false;
				
				size_t bytesToRead = min(bytesAvailable, BUFFER_SIZE);
				size_t bytesRead = stream->readBytes(buffer, bytesToRead);
				
				if (bytesRead > 0) {
					audioData.insert(audioData.end(), buffer, buffer + bytesRead);
					totalBytesRead += bytesRead;
					ESP_LOGD("TTS", "Read %d bytes, total: %d", bytesRead, totalBytesRead);
				} else {
					ESP_LOGI("TTS", "Read returned 0 bytes, ending read");
					break;
				}
				
				// Small yield to prevent watchdog
				delay(1);
			}
			
			ESP_LOGI("TTS", "Finished reading audio data: %d bytes in %lu ms", 
					totalBytesRead, millis() - startTime);
			
			if (totalBytesRead > 0) {
				uint8_t* audioBuffer = (uint8_t*)heap_caps_malloc(totalBytesRead, MALLOC_CAP_SPIRAM);
				if (audioBuffer) {
					memcpy(audioBuffer, audioData.data(), totalBytesRead);
					ESP_LOGI("TTS", "Audio data received successfully (%d bytes)", totalBytesRead);
					cb(txt, audioBuffer, totalBytesRead);
					free(audioBuffer);
				} else {
					ESP_LOGE("TTS", "Failed to allocate memory for audio buffer");
					cb(txt, nullptr, 0);
				}
			} else {
				ESP_LOGE("TTS", "No audio data received");
				cb(txt, nullptr, 0);
			}
		} else {
			String response = http.getString();
			ESP_LOGE("TTS", "API returned error code: %d", httpCode);

			// Try to extract error message from JSON
			JsonDocument errorDoc;
			if (deserializeJson(errorDoc, response) == DeserializationError::Ok) {
				if (errorDoc["error"].is<JsonObject>()) {
					String errorMsg = errorDoc["error"]["message"] | "Unknown API error";
					ESP_LOGE("TTS", "API Error: %s", errorMsg.c_str());
				}
			}

			cb(txt, nullptr, 0);
		}
		
		// Get all collected headers
		for(int i = 0; i < http.headers(); i++) {
			String headerName = http.headerName(i);
			String headerValue = http.header(i);
			ESP_LOGI("TTS", "%s: %s", headerName.c_str(), headerValue.c_str());
		}

		http.end();
		delete params;
		params = nullptr;
		vTaskDelete(NULL);
	}, "TTS_Request", 16384, new std::tuple<GPTTtsService*, String, String, AudioCallback>(this, jsonPayload, text, callback), 1, NULL, 1);
}

std::vector<gpt_tts_t> GPTTtsService::getAvailableVoices() {
	return std::vector<gpt_tts_t>(AVAILABLE_VOICES, AVAILABLE_VOICES + NUM_VOICES);
}

GPTTtsService aiTts;