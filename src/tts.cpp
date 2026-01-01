#include "tts.h"
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <vector>
#include "core.h"

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
	doc["instructions"] = "Speak softly with warmth, like a small robot chatting with a close friend late in the afternoon. The tone is relaxed, caring, and familiar. Use gentle pauses and light conversational fillers, naturally.";

	String jsonString;
	serializeJson(doc, jsonString);
	return jsonString;
}

template<typename CallbackType>
void GPTTtsService::performTtsRequest(const String& text, const String& voice, CallbackType callback, bool isStreaming) {
	if (!_initialized) {
		ESP_LOGE("TTS", "TTS service not initialized");
		if constexpr (std::is_same_v<CallbackType, AudioCallback>) {
			callback(text, nullptr, 0);
		} else {
			callback(text, nullptr, 0, true);
		}
		return;
	}

	if (!WiFi.isConnected()) {
		ESP_LOGE("TTS", "No WiFi connection");
		if constexpr (std::is_same_v<CallbackType, AudioCallback>) {
			callback(text, nullptr, 0);
		} else {
			callback(text, nullptr, 0, true);
		}
		return;
	}

	if (text.length() == 0) {
		ESP_LOGE("TTS", "Text is empty");
		if constexpr (std::is_same_v<CallbackType, AudioCallback>) {
			callback(text, nullptr, 0);
		} else {
			callback(text, nullptr, 0, true);
		}
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
		auto* params = static_cast<std::tuple<GPTTtsService*, String, String, CallbackType, bool>*>(param);
		auto& [service, payload, txt, cb, streaming] = *params;

		HTTPClient http;
		wifiClient.setInsecure(); // For HTTPS without certificate validation

		http.begin(wifiClient, "https://api.openai.com/v1/audio/speech");
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

		if (streaming) {
			ESP_LOGI("TTS", "Sending streaming TTS request to OpenAI API...");
		} else {
			ESP_LOGI("TTS", "Sending TTS request to OpenAI API...");
		}

		int httpCode = http.POST(payload);

		if (httpCode == 200) {
			// Handle audio data based on streaming mode
			WiFiClient* stream = http.getStreamPtr();
			int contentLength = http.getSize();
			
			const size_t BUFFER_SIZE = 1024 * 500;
			uint8_t* buffer = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
			
			if (streaming) {
				ESP_LOGI("TTS", "Starting to stream audio data");
			} else {
				ESP_LOGI("TTS", "Starting to read audio data (Content-Length: %d)", contentLength);
			}
			
			unsigned long startTime = millis();
			const unsigned long INITIAL_TIMEOUT_MS = 10000;
			const unsigned long INTER_CHUNK_TIMEOUT_MS = 2000;
			unsigned long lastDataTime = 0;
			size_t consecutiveZeroReads = 0;
			const size_t MAX_ZERO_READS = 10;
			bool dataReceived = false;
			bool initialTimeoutActive = true;
			size_t totalBytesProcessed = 0;
			
			if constexpr (std::is_same_v<CallbackType, AudioCallback>) {
				// For non-streaming, accumulate all data
				std::vector<uint8_t> audioData;
				
				while (true) {
					unsigned long currentTime = millis();
					size_t bytesAvailable = stream->available();
					
					if (!dataReceived) {
						if (currentTime - startTime >= INITIAL_TIMEOUT_MS) {
							ESP_LOGW("TTS", "Initial timeout: no data received after %lu ms", INITIAL_TIMEOUT_MS);
							break;
						}
					} else {
						if (currentTime - lastDataTime >= INTER_CHUNK_TIMEOUT_MS) {
							ESP_LOGI("TTS", "Inter-chunk timeout: no data for %lu ms after receiving %d bytes", 
									INTER_CHUNK_TIMEOUT_MS, totalBytesProcessed);
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
						delay(100);
						continue;
					}
					
					consecutiveZeroReads = 0;
					dataReceived = true;
					lastDataTime = currentTime;
					initialTimeoutActive = false;
					
					size_t bytesToRead = min(bytesAvailable, BUFFER_SIZE);
					size_t bytesRead = stream->readBytes(buffer, bytesToRead);
					
					if (bytesRead > 0) {
						audioData.insert(audioData.end(), buffer, buffer + bytesRead);
						totalBytesProcessed += bytesRead;
						ESP_LOGD("TTS", "Read %d bytes, total: %d", bytesRead, totalBytesProcessed);
					} else {
						ESP_LOGI("TTS", "Read returned 0 bytes, ending read");
						break;
					}
					
					delay(1);
				}
				
				if (totalBytesProcessed > 0) {
					uint8_t* audioBuffer = (uint8_t*)heap_caps_malloc(totalBytesProcessed, MALLOC_CAP_SPIRAM);
					if (audioBuffer) {
						memcpy(audioBuffer, audioData.data(), totalBytesProcessed);
						cb(txt, audioBuffer, totalBytesProcessed);
						heap_caps_free(audioBuffer);
					} else {
						ESP_LOGE("TTS", "Failed to allocate memory for audio buffer");
						cb(txt, nullptr, 0);
					}
				} else {
					ESP_LOGE("TTS", "No audio data received");
					cb(txt, nullptr, 0);
				}
			} else {
				// For streaming, send data chunks immediately as received
				
				while (true) {
					unsigned long currentTime = millis();
					size_t bytesAvailable = stream->available();
					
					if (!dataReceived) {
						if (currentTime - startTime >= INITIAL_TIMEOUT_MS) {
							ESP_LOGW("TTS", "Initial timeout: no data received after %lu ms", INITIAL_TIMEOUT_MS);
							cb(txt, nullptr, 0, true);
							break;
						}
					} else {
						if (currentTime - lastDataTime >= INTER_CHUNK_TIMEOUT_MS) {
							cb(txt, nullptr, 0, true);
							break;
						}
					}
					
					if (bytesAvailable == 0) {
						consecutiveZeroReads++;
						if (consecutiveZeroReads >= MAX_ZERO_READS) {
							if (dataReceived) {
								// No remaining data to send since we send immediately
							} else {
								ESP_LOGW("TTS", "Stream ended: %d consecutive zero reads, no data received", consecutiveZeroReads);
							}
							cb(txt, nullptr, 0, true);
							break;
						}
						delay(100);
						continue;
					}
					
					consecutiveZeroReads = 0;
					dataReceived = true;
					lastDataTime = currentTime;
					initialTimeoutActive = false;
					
					size_t bytesToRead = min(bytesAvailable, BUFFER_SIZE);
					size_t bytesRead = stream->readBytes(buffer, bytesToRead);
					
					if (bytesRead > 0) {
						totalBytesProcessed += bytesRead;
						// Send the chunk immediately without accumulation
						cb(txt, buffer, bytesRead, false);
						ESP_LOGD("TTS", "Sent chunk (%d bytes)", bytesRead);
					} else {
						cb(txt, nullptr, 0, true);
						break;
					}
					
					// Removed delay(1) to prevent potential data corruption from task yielding during streaming
				}
			}
			
			heap_caps_free(buffer);
		} else {
			String response = http.getString();
			ESP_LOGE("TTS", "API returned error code: %d", httpCode);

			JsonDocument errorDoc;
			if (deserializeJson(errorDoc, response) == DeserializationError::Ok) {
				if (errorDoc["error"].is<JsonObject>()) {
					String errorMsg = errorDoc["error"]["message"] | "Unknown API error";
					ESP_LOGE("TTS", "API Error: %s", errorMsg.c_str());
				}
			}

			if constexpr (std::is_same_v<CallbackType, AudioCallback>) {
				cb(txt, nullptr, 0);
			} else {
				cb(txt, nullptr, 0, true);
			}
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
	}, isStreaming ? "TTS_Stream_Request" : "TTS_Request", 16384, new std::tuple<GPTTtsService*, String, String, CallbackType, bool>(this, jsonPayload, text, callback, isStreaming), 15, NULL, 0);
}

void GPTTtsService::textToSpeech(const String& text, AudioCallback callback) {
	textToSpeech(text, _voice, callback);
}

void GPTTtsService::textToSpeech(const String& text, const String& voice, AudioCallback callback) {
	performTtsRequest(text, voice, callback, false);
}

void GPTTtsService::textToSpeechStream(const String& text, StreamCallback callback) {
	textToSpeechStream(text, _voice, callback);
}

void GPTTtsService::textToSpeechStream(const String& text, const String& voice, StreamCallback callback) {
	performTtsRequest(text, voice, callback, true);
}

std::vector<gpt_tts_t> GPTTtsService::getAvailableVoices() {
	return std::vector<gpt_tts_t>(AVAILABLE_VOICES, AVAILABLE_VOICES + NUM_VOICES);
}

GPTTtsService aiTts;

