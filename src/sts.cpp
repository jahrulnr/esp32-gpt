#include "sts.h"
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <WebSocketsClient.h>
#include "core.h"

// Available STS models
static const GPTStsModel AVAILABLE_MODELS[] = {
	{"gpt-realtime", "GPT-4 Realtime"},
	{"gpt-realtime-mini", "GPT Realtime Mini"},
	{"gpt-4o-realtime-preview", "GPT-4o Realtime Preview"},
	{"gpt-4o-mini-realtime-preview", "GPT-4o Mini Realtime Preview"}
};

static const size_t NUM_MODELS = sizeof(AVAILABLE_MODELS) / sizeof(AVAILABLE_MODELS[0]);

GPTStsService::GPTStsService()
	: _model("gpt-4o-mini-realtime-preview")
	, _voice("shimmer")
	, _initialized(false)
	, _fs(nullptr)
	, _isStreaming(false)
	, _streamingTask(nullptr)
{
}

GPTStsService::~GPTStsService() {
	stop();
}

bool GPTStsService::init(const String& apiKey, fs::FS& fs) {
	if (apiKey.length() == 0) {
		ESP_LOGE("STS", "API key is empty");
		return false;
	}

	_apiKey = apiKey;
	_fs = &fs;
	_initialized = true;

	ESP_LOGI("STS", "Speech-to-speech service initialized with model: %s", _model.c_str());
	return true;
}

void GPTStsService::speechToSpeech(const String& filePath, AudioCallback callback) {
	speechToSpeech(filePath, _model, callback);
}

void GPTStsService::speechToSpeech(const String& filePath, const String& model, AudioCallback callback) {
	// For non-streaming, we can collect all chunks and call callback once
	speechToSpeechStream(filePath, model, [callback](const String& filePath, const uint8_t* audioChunk, size_t chunkSize, bool isLastChunk) {
		static std::vector<uint8_t> audioBuffer;
		if (!isLastChunk) {
			audioBuffer.insert(audioBuffer.end(), audioChunk, audioChunk + chunkSize);
		} else {
			// Call the callback with complete audio
			callback(filePath, audioBuffer.data(), audioBuffer.size());
			audioBuffer.clear();
		}
	});
}

void GPTStsService::speechToSpeechStream(const String& filePath, StreamCallback callback) {
	speechToSpeechStream(filePath, _model, callback);
}

void GPTStsService::speechToSpeechStream(const String& filePath, const String& model, StreamCallback callback) {
	if (!_initialized) {
		ESP_LOGE("STS", "STS service not initialized");
		callback(filePath, nullptr, 0, true);
		return;
	}

	if (!WiFi.isConnected()) {
		ESP_LOGE("STS", "No WiFi connection");
		callback(filePath, nullptr, 0, true);
		return;
	}

	// Check if file exists
	if (!_fs->exists(filePath)) {
		ESP_LOGE("STS", "Audio file does not exist: %s", filePath.c_str());
		callback(filePath, nullptr, 0, true);
		return;
	}

	// Create async task for WebSocket streaming
	xTaskCreatePinnedToCore([](void* param) {
		auto* params = static_cast<std::tuple<GPTStsService*, String, String, StreamCallback>*>(param);
		auto& [service, file, mdl, cb] = *params;

		service->performStsStreaming(file, mdl, cb);

		delete params;
		params = nullptr;
		vTaskDelete(NULL);
	}, "STS_Streaming", 16384, new std::tuple<GPTStsService*, String, String, StreamCallback>(this, filePath, model, callback), 10, NULL, 0);
}

void GPTStsService::performStsStreaming(const String& filePath, const String& model, StreamCallback callback) {
	WebSocketsClient webSocket;
	WiFiClientSecure client;
	client.setInsecure();

	// WebSocket event handler
		webSocket.onEvent([this, &filePath, &callback, &webSocket](WStype_t type, uint8_t* payload, size_t length) {
		switch (type) {
			case WStype_CONNECTED:
				ESP_LOGI("STS", "WebSocket connected");
				// Send session configuration
				{
					String config = this->buildSessionConfig();
					webSocket.sendTXT(config);
				}
				break;
			case WStype_TEXT:
				{
					GPTSpiJsonDocument doc;
					DeserializationError error = deserializeJson(doc, payload, length);
					if (error) {
						ESP_LOGE("STS", "Failed to parse WebSocket message: %s", error.c_str());
						return;
					}

					String type = doc["type"] | "";
					if (type == "session.created") {
						ESP_LOGI("STS", "Session created");
						// Now send audio data
						// For simplicity, send the whole file at once
						// In real implementation, stream in chunks
						File file = this->_fs->open(filePath, "r");
						if (file) {
							size_t fileSize = file.size();
							uint8_t* buffer = (uint8_t*) heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
							if (buffer) {
								file.read(buffer, fileSize);
								file.close();

								// Send audio data as base64
								String audioData = this->base64Encode(buffer, fileSize);
								String audioMessage = "{\"type\":\"input_audio_buffer.append\",\"audio\":\"" + audioData + "\"}";
								webSocket.sendTXT(audioMessage);

								// Commit the audio buffer
								webSocket.sendTXT("{\"type\":\"input_audio_buffer.commit\"}");

								// Create response
								webSocket.sendTXT("{\"type\":\"response.create\"}");

								free(buffer);
							} else {
								file.close();
							}
						}
					} else if (type == "response.audio.delta") {
						// Received audio delta (base64 encoded)
						String audioBase64 = doc["delta"] | "";
						// Decode base64 to audio data
						std::vector<uint8_t> audioData = this->base64Decode(audioBase64);
						callback(filePath, audioData.data(), audioData.size(), false);
					} else if (type == "response.audio.done") {
						ESP_LOGI("STS", "Response audio done");
						callback(filePath, nullptr, 0, true);
						webSocket.disconnect();
					} else if (type == "error") {
						String errorMsg = doc["error"]["message"] | "Unknown error";
						ESP_LOGE("STS", "WebSocket error: %s", errorMsg.c_str());
						callback(filePath, nullptr, 0, true);
						webSocket.disconnect();
					}
				}
				break;
			case WStype_BIN:
				ESP_LOGW("STS", "Received binary message (%d bytes) - not handled", length);
				break;
			case WStype_ERROR:
				ESP_LOGE("STS", "WebSocket error occurred");
				callback(filePath, nullptr, 0, true);
				webSocket.disconnect();
				break;
			case WStype_FRAGMENT_TEXT_START:
			case WStype_FRAGMENT_BIN_START:
			case WStype_FRAGMENT:
			case WStype_FRAGMENT_FIN:
				ESP_LOGW("STS", "Received fragmented message - not handled");
				break;
			case WStype_PING:
				ESP_LOGD("STS", "Received PING");
				break;
			case WStype_PONG:
				ESP_LOGD("STS", "Received PONG");
				break;
			case WStype_DISCONNECTED:
				ESP_LOGI("STS", "WebSocket disconnected");
				break;
			default:
				ESP_LOGW("STS", "Unknown WebSocket event type: %d", type);
				break;
		}
	});

	// Connect to WebSocket
	String url = "/v1/realtime?model=" + model;
	String authHeader = "Authorization: Bearer " + _apiKey + "\r\n";
	webSocket.setExtraHeaders(authHeader.c_str());
	webSocket.beginSSL("api.openai.com", 443, url.c_str());
	webSocket.setReconnectInterval(5000);

	// Wait for connection and process
	unsigned long startTime = millis();
	while (webSocket.isConnected() && (millis() - startTime) < 30000) { // 30 second timeout
		webSocket.loop();
		delay(10);
	}

	if (!webSocket.isConnected()) {
		ESP_LOGE("STS", "WebSocket connection failed or timed out");
		callback(filePath, nullptr, 0, true);
	}

	webSocket.disconnect();
}

String GPTStsService::buildSessionConfig() {
	GPTSpiJsonDocument doc;
	doc["type"] = "session.update";
	doc["session"]["modalities"][0] = "text";
	doc["session"]["modalities"][1] = "audio";
	doc["session"]["instructions"] = "You are a helpful assistant.";
	doc["session"]["voice"] = _voice;
	doc["session"]["input_audio_format"] = "pcm16";
	doc["session"]["output_audio_format"] = "pcm16";
	doc["session"]["input_audio_transcription"]["model"] = "whisper-1";
	doc["session"]["turn_detection"]["type"] = "server_vad";
	doc["session"]["turn_detection"]["threshold"] = 0.5;
	doc["session"]["turn_detection"]["prefix_padding_ms"] = 300;
	doc["session"]["turn_detection"]["silence_duration_ms"] = 200;
	doc["session"]["temperature"] = 0.8;
	doc["session"]["max_response_output_tokens"] = 4096;

	String config;
	serializeJson(doc, config);
	return config;
}

String GPTStsService::base64Encode(const uint8_t* data, size_t length) {
	// Simple base64 encoding implementation
	static const char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	String encoded;
	size_t i = 0;
	uint32_t buffer = 0;
	int bits = 0;

	while (i < length || bits > 0) {
		if (i < length) {
			buffer = (buffer << 8) | data[i++];
			bits += 8;
		} else {
			buffer <<= 8;
			bits += 8;
		}

		while (bits >= 6) {
			bits -= 6;
			encoded += base64Chars[(buffer >> bits) & 0x3F];
		}
	}

	// Add padding
	while (encoded.length() % 4 != 0) {
		encoded += '=';
	}

	return encoded;
}

std::vector<uint8_t> GPTStsService::base64Decode(const String& input) {
	std::vector<uint8_t> decoded;
	static const int base64Index[256] = {
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
		52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
		-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
		15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
		-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
		41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
	};

	size_t length = input.length();
	uint32_t buffer = 0;
	int bits = 0;

	for (size_t i = 0; i < length; ++i) {
		char c = input[i];
		if (c == '=') break; // Padding

		int value = base64Index[(unsigned char)c];
		if (value == -1) continue;

		buffer = (buffer << 6) | value;
		bits += 6;

		if (bits >= 8) {
			bits -= 8;
			decoded.push_back((buffer >> bits) & 0xFF);
		}
	}

	return decoded;
}

std::vector<gpt_sts_t> GPTStsService::getAvailableModels() {
	return std::vector<gpt_sts_t>(AVAILABLE_MODELS, AVAILABLE_MODELS + NUM_MODELS);
}

bool GPTStsService::start(AudioFillCallback audioFillCallback, AudioResponseCallback audioResponseCallback) {
	if (!_initialized) {
		ESP_LOGE("STS", "STS service not initialized");
		return false;
	}

	if (_isStreaming) {
		ESP_LOGW("STS", "Streaming already active");
		return true;
	}

	if (!WiFi.isConnected()) {
		ESP_LOGE("STS", "No WiFi connection");
		return false;
	}

	_audioFillCallback = audioFillCallback;
	_audioResponseCallback = audioResponseCallback;
	_isStreaming = true;

	// Create streaming task
	xTaskCreatePinnedToCore([](void* param) {
		GPTStsService* service = static_cast<GPTStsService*>(param);
		service->streamingTask();
		vTaskDelete(service->_streamingTask);
	}, "STS_Streaming", 16384, this, 6, &_streamingTask, 1);

	ESP_LOGI("STS", "Streaming started");
	return true;
}

void GPTStsService::stop() {
	if (!_isStreaming) {
		return;
	}

	_isStreaming = false;

	if (_streamingTask != nullptr) {
		vTaskDelete(_streamingTask);
		_streamingTask = nullptr;
	}

	ESP_LOGI("STS", "Streaming stopped");
}

void GPTStsService::streamingTask() {
	WebSocketsClient webSocket;
	WiFiClientSecure client;
	client.setInsecure();

	bool sessionCreated = false;
	unsigned long wsLastLoop = 0;

	// WebSocket event handler
	webSocket.onEvent([this, &sessionCreated](WStype_t type, uint8_t* payload, size_t length) {
		switch (type) {
			case WStype_CONNECTED:
				ESP_LOGI("STS", "WebSocket connected for streaming");
				sessionCreated = false;
				break;
			case WStype_TEXT:
				{
					GPTSpiJsonDocument doc;
					DeserializationError error = deserializeJson(doc, payload);
					if (error) {
						ESP_LOGE("STS", "Failed to parse WebSocket message: %s", error.c_str());
						return;
					}

					String type = doc["type"] | "";
					if (type == "session.created") {
						sessionCreated = true;
						ESP_LOGI("STS", "Session created for streaming");
						ESP_LOGI("STS", "%s", (char*)payload);
					} else if (type == "response.audio.delta" && sessionCreated) {
						// Received audio delta (base64 encoded)
						String audioBase64 = doc["delta"] | "";
						// Decode base64 to audio data
						std::vector<uint8_t> audioData = this->base64Decode(audioBase64);
						if (_audioResponseCallback) {
							_audioResponseCallback(audioData.data(), audioData.size(), false);
						}
					} else if (type == "response.text.delta" && sessionCreated) {
						String textDelta = doc["delta"] | "";
						ESP_LOGI("STS", "Received text delta: %s", textDelta.c_str());
					} else if (type == "response.created" && sessionCreated) {
						ESP_LOGI("STS", "Response created");
					} else if (type == "response.output_item.added" && sessionCreated) {
						ESP_LOGI("STS", "Response output item added");
					} else if (type == "response.output_item.done" && sessionCreated) {
						ESP_LOGI("STS", "Response output item done");
					} else if (type == "response.done" && sessionCreated) {
						ESP_LOGI("STS", "Response completed");
					} else if (type == "error") {
						String errorMsg = doc["error"]["message"] | "Unknown error";
						ESP_LOGE("STS", "WebSocket error: %s", errorMsg.c_str());
					} else if (type == "input_audio_buffer.speech_started") {
						ESP_LOGI("STS", "Speech started");
					} else if (type == "input_audio_buffer.speech_stopped") {
						ESP_LOGI("STS", "Speech stopped - server will create response");
					} else {
						ESP_LOGW("STS", "Unknown Response type: %s", type.c_str());
						ESP_LOGW("STS", "%s", (char*)payload);
					}
				}
				break;
			case WStype_BIN:
				ESP_LOGW("STS", "Received binary message (%d bytes) - not handled", length);
				break;
			case WStype_ERROR:
				ESP_LOGE("STS", "WebSocket error occurred");
				_isStreaming = false; // Stop streaming on error
				break;
			case WStype_FRAGMENT_TEXT_START:
			case WStype_FRAGMENT_BIN_START:
			case WStype_FRAGMENT:
			case WStype_FRAGMENT_FIN:
				ESP_LOGW("STS", "Received fragmented message - not handled");
				break;
			case WStype_PING:
				ESP_LOGD("STS", "Received PING");
				break;
			case WStype_PONG:
				ESP_LOGD("STS", "Received PONG");
				break;
			case WStype_DISCONNECTED:
				ESP_LOGI("STS", "WebSocket disconnected");
				sessionCreated = false;
				_isStreaming = false; // Stop streaming on disconnect
				break;
			default:
				ESP_LOGW("STS", "Unknown WebSocket event type: %d", type);
				break;
		}
	});

	// Connect to WebSocket
	String url = "/v1/realtime?model=" + _model;
	String authHeader = "Bearer " + _apiKey;
	webSocket.beginSSL("api.openai.com", 443, url.c_str());
	webSocket.setAuthorization(authHeader.c_str());
	webSocket.setReconnectInterval(5000);

	// Send session configuration
	String config = this->buildSessionConfig();
	webSocket.sendTXT(config);

	// Main streaming loop
	while (_isStreaming) {
		if (millis() - wsLastLoop > 10){
			webSocket.loop();
			wsLastLoop = millis();
		}

		// Continuously send audio data if available
		if (sessionCreated && _audioFillCallback) {
			const size_t bufferSize = 1536; 
			uint8_t* buffer = (uint8_t*) heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
			size_t bytesRead = _audioFillCallback(buffer, bufferSize);

			if (bytesRead > 0) {
				ESP_LOGD("STS", "Sending %d bytes of audio data", bytesRead);
				// Encode audio to base64 and send
				String audioData = this->base64Encode(buffer, bytesRead);
				String audioMessage = "{\"type\":\"input_audio_buffer.append\",\"audio\":\"" + audioData + "\"}";
				webSocket.sendTXT(audioMessage);
			}

			heap_caps_free(buffer);
		}

		// vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent busy loop
		taskYIELD();
	}

	webSocket.disconnect();
	ESP_LOGI("STS", "Streaming task ended");
}

GPTStsService aiSts;