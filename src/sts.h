#ifndef SPEECH_TO_SPEECH_SERVICE_H
#define SPEECH_TO_SPEECH_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>
#include <FS.h>
#include <core.h>

typedef struct GPTStsModel {
	const char* id;
	const char* displayName;
} gpt_sts_t;

/**
 * ESP32 Speech-to-Speech Service for OpenAI Realtime API
 */
class GPTStsService {
public:

	// setup tool
	struct GPTTool {
		const char* description;
		const char* name;
		GPTSpiJsonDocument params;
	};

	// call the function
	struct GPTToolCall {
		const char* callId;
		const char* name;
		GPTSpiJsonDocument params;
	};

	// trigger ai model with output
	struct GPTToolCallback {
		const char* callId;
		const char* name;
		const char* output;
		const char* status;
	};
	
	// Callback type for audio fill (provide audio data for streaming)
	using AudioFillCallback = std::function<size_t(uint8_t* buffer, size_t maxSize)>;

	// Callback type for audio response (receive streaming audio response)
	using AudioResponseCallback = std::function<void(const uint8_t* audioData, size_t audioSize, bool isLastChunk)>;

	// event callbcak
	using EventConnectedCallback = std::function<void(void)>;
	using EventUpdatedCallback = std::function<void(const char*)>;
	using EventFunctionCallback = std::function<void(const GPTToolCall&)>;
	using EventDisconnectCallback = std::function<void(void)>;

	GPTStsService();
	~GPTStsService();

	/**
	 * Initialize STS service
	 * @param apiKey The OpenAI API key
	 * @param fs Reference to the filesystem (SPIFFS, LittleFS, etc.)
	 * @return true if initialization successful
	 */
	bool init(const String& apiKey);

	/**
	 * Check if service is initialized
	 * @return true if ready to use
	 */
	bool isInitialized() const { return _initialized; }

	/**
	 * Start continuous streaming speech-to-speech session
	 * @param audioFillCallback Callback to provide audio data when needed
	 * @param audioResponseCallback Callback to receive audio responses
	 * @return true if streaming started successfully
	 */
	bool start(
		AudioFillCallback audioFillCallback, 
		AudioResponseCallback audioResponseCallback,
		EventConnectedCallback eventConnectedCallback = nullptr,
		EventUpdatedCallback eventUpdatedCallback = nullptr,
		EventFunctionCallback eventFunctionCallback = nullptr,
		EventDisconnectCallback eventDisconnectCallback = nullptr
		);

	/**
	 * Stop the continuous streaming session
	 */
	void stop();

	/**
	 * Check if streaming is currently active
	 * @return true if streaming is active
	 */
	bool isStreaming() const { return _isStreaming; }

	/**
	 * Set STS model
	 * @param model Model name
	 */
	void setModel(const String& model) { _model = model; }

	/**
	 * Set voice for TTS response
	 * @param voice Voice name
	 */
	void setVoice(const String& voice) { _voice = voice; }

	/**
	 * Get available STS models
	 * @return Vector of available models
	 */
	static std::vector<gpt_sts_t> getAvailableModels();
	
	void addTool(const GPTTool& tool);
	bool sendTools();
	bool sendToolCallback(const GPTToolCallback& toolCallback);

	bool Speak() { return gptWebSocket->sendTXT("{\"type\":\"response.create\"}"); }

private:
	String _apiKey;
	String _model;
	String _voice;
	bool _initialized;

	// Streaming state
	bool _isStreaming;
	bool _isGPTSpeaking;
	TaskHandle_t _streamingTask;

	// callback
	AudioFillCallback _audioFillCallback;
	AudioResponseCallback _audioResponseCallback;
	EventConnectedCallback _eventConnectedCallback;
	EventUpdatedCallback _eventUpdatedCallback;
	EventFunctionCallback _eventFunctionCallback;
	EventDisconnectCallback _eventDisconnectCallback;
	std::vector<GPTTool> _tools;

	// Continuous streaming task
	void streamingTask();

	// Build session configuration JSON
	String buildSessionConfig();

	// Encode audio to base64 for WebSocket
	String base64Encode(const uint8_t* data, size_t length);

	// Decode base64 audio from WebSocket
	std::vector<uint8_t> base64Decode(const String& input);
};

extern GPTStsService aiSts;

#endif // SPEECH_TO_SPEECH_SERVICE_H