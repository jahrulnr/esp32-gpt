#ifndef SPEECH_TO_SPEECH_SERVICE_H
#define SPEECH_TO_SPEECH_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>
#include <FS.h>

typedef struct GPTStsModel {
	const char* id;
	const char* displayName;
} gpt_sts_t;

/**
 * ESP32 Speech-to-Speech Service for OpenAI Realtime API
 */
class GPTStsService {
public:
	// Callback type for STS responses (audio data)
	using AudioCallback = std::function<void(const String& filePath, const uint8_t* audioData, size_t audioSize)>;

	// Callback type for streaming STS responses (audio chunks)
	using StreamCallback = std::function<void(const String& filePath, const uint8_t* audioChunk, size_t chunkSize, bool isLastChunk)>;

	// Callback type for audio fill (provide audio data for streaming)
	using AudioFillCallback = std::function<size_t(uint8_t* buffer, size_t maxSize)>;

	// Callback type for audio response (receive streaming audio response)
	using AudioResponseCallback = std::function<void(const uint8_t* audioData, size_t audioSize, bool isLastChunk)>;

	GPTStsService();
	~GPTStsService();

	/**
	 * Initialize STS service
	 * @param apiKey The OpenAI API key
	 * @param fs Reference to the filesystem (SPIFFS, LittleFS, etc.)
	 * @return true if initialization successful
	 */
	bool init(const String& apiKey, fs::FS& fs);

	/**
	 * Check if service is initialized
	 * @return true if ready to use
	 */
	bool isInitialized() const { return _initialized; }

	/**
	 * Process audio file with speech-to-speech
	 * @param filePath Path to input audio file (WAV format)
	 * @param callback Audio data callback
	 */
	void speechToSpeech(const String& filePath, AudioCallback callback);

	/**
	 * Process audio file with speech-to-speech using specific model
	 * @param filePath Path to input audio file (WAV format)
	 * @param model Model to use
	 * @param callback Audio data callback
	 */
	void speechToSpeech(const String& filePath, const String& model, AudioCallback callback);

	/**
	 * Process audio file with streaming speech-to-speech
	 * @param filePath Path to input audio file (WAV format)
	 * @param callback Stream callback for audio chunks
	 */
	void speechToSpeechStream(const String& filePath, StreamCallback callback);

	/**
	 * Process audio file with streaming speech-to-speech using specific model
	 * @param filePath Path to input audio file (WAV format)
	 * @param model Model to use
	 * @param callback Stream callback for audio chunks
	 */
	void speechToSpeechStream(const String& filePath, const String& model, StreamCallback callback);

	/**
	 * Start continuous streaming speech-to-speech session
	 * @param audioFillCallback Callback to provide audio data when needed
	 * @param audioResponseCallback Callback to receive audio responses
	 * @return true if streaming started successfully
	 */
	bool start(AudioFillCallback audioFillCallback, AudioResponseCallback audioResponseCallback);

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

private:
	String _apiKey;
	String _model;
	String _voice;
	bool _initialized;
	fs::FS* _fs;

	// Streaming state
	bool _isStreaming;
	TaskHandle_t _streamingTask;
	AudioFillCallback _audioFillCallback;
	AudioResponseCallback _audioResponseCallback;

	// Process WebSocket streaming
	void performStsStreaming(const String& filePath, const String& model, StreamCallback callback);

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