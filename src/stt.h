#ifndef TRANSCRIPTION_SERVICE_H
#define TRANSCRIPTION_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <functional>
#include <vector>
#include <FS.h>

typedef struct GPTSttModel {
	const char* id;
	const char* displayName;
} gpt_transcription_t;

/**
 * ESP32 Transcription Service for OpenAI Audio Transcription API
 */
class GPTSttService {
public:
	// Callback type for transcription responses
	using TranscriptionCallback = std::function<void(const String& filePath, const String& transcription, const String& usageJson)>;

	GPTSttService();
	~GPTSttService();

	/**
	 * Initialize transcription service
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
	 * Transcribe audio file
	 * @param filePath Path to audio file (WAV format)
	 * @param callback Transcription callback
	 */
	void transcribeAudio(const String& filePath, TranscriptionCallback callback);

	/**
	 * Transcribe audio file with specific model
	 * @param filePath Path to audio file (WAV format)
	 * @param model Model to use
	 * @param callback Transcription callback
	 */
	void transcribeAudio(const String& filePath, const String& model, TranscriptionCallback callback);

	/**
	 * Set transcription model
	 * @param model Model name
	 */
	void setModel(const String& model) { _model = model; }

	/**
	 * Get available transcription models
	 * @return Vector of available models
	 */
	static std::vector<gpt_transcription_t> getAvailableModels();

private:
	String _apiKey;
	String _model;
	bool _initialized;
	fs::FS* _fs;

	// Process API response
	void processResponse(int httpCode, const String& response, const String& filePath, TranscriptionCallback callback);

	// Build multipart form data
	String buildMultipartPayload(const String& filePath, const String& boundary);
};

extern GPTSttService aiStt;

#endif // TRANSCRIPTION_SERVICE_H