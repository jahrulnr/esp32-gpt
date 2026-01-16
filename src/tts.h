#ifndef TTS_SERVICE_H
#define TTS_SERVICE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <functional>
#include <vector>
#include "core.h"

typedef struct GPTTtsVoice {
	const char* id;
	const char* displayName;
} gpt_tts_t;

/**
 * ESP32 TTS Service for OpenAI Text-to-Speech API
 */
class GPTTtsService {
public:
	// Callback type for TTS responses (audio data)
	using AudioCallback = std::function<void(const String& text, const uint8_t* audioData, size_t audioSize)>;
	
	// Callback type for streaming TTS responses (audio chunks)
	using StreamCallback = std::function<void(const String& text, const uint8_t* audioChunk, size_t chunkSize, bool isLastChunk)>;

	GPTTtsService();
	~GPTTtsService();

	/**
	 * Initialize TTS service
	 * @param apiKey The OpenAI API key
	 * @return true if initialization successful
	 */
	bool init(const String& apiKey);

	/**
	 * Check if service is initialized
	 * @return true if ready to use
	 */
	bool isInitialized() const { return _initialized; }

	/**
	 * Convert text to speech
	 * @param text Text to convert to speech
	 * @param callback Audio data callback
	 */
	void textToSpeech(const String& text, AudioCallback callback);

	/**
	 * Convert text to speech with specific voice
	 * @param text Text to convert to speech
	 * @param voice Voice to use
	 * @param callback Audio data callback
	 */
	void textToSpeech(const String& text, const String& voice, AudioCallback callback);

	/**
	 * Convert text to speech with streaming callback
	 * @param text Text to convert to speech
	 * @param callback Stream callback for audio chunks
	 */
	void textToSpeechStream(const String& text, StreamCallback callback);

	/**
	 * Convert text to speech with specific voice and streaming callback
	 * @param text Text to convert to speech
	 * @param voice Voice to use
	 * @param callback Stream callback for audio chunks
	 */
	void textToSpeechStream(const String& text, const String& voice, StreamCallback callback);

	/**
	 * Set TTS model
	 * @param model Model name
	 */
	void setModel(const String& model) { _model = model; }

	/**
	 * Set voice
	 * @param voice Voice name
	 */
	void setVoice(const String& voice) { _voice = voice; }

	/**
	 * Set audio format
	 * @param format Audio format enum
	 */
	void setFormat(GPTAudioFormat format);

	/**
	 * Get current audio format
	 * @return Current audio format enum
	 */
	GPTAudioFormat getFormat() const;

	/**
	 * Get available TTS voices
	 * @return Vector of available voices
	 */
	static std::vector<gpt_tts_t> getAvailableVoices();

private:
	String _apiKey;
	String _model;
	String _voice;
	GPTAudioFormat _format;
	bool _initialized;

	// Common HTTP request handler
	template<typename CallbackType>
	void performTtsRequest(const String& text, const String& voice, CallbackType callback, bool isStreaming);

	// Process API response
	void processResponse(int httpCode, const String& response, const String& text, AudioCallback callback);

	// Build JSON request payload
	String buildJsonPayload(const String& text);
};

extern GPTTtsService aiTts;

#endif // TTS_SERVICE_H