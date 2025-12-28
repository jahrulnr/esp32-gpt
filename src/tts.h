#ifndef TTS_SERVICE_H
#define TTS_SERVICE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <functional>
#include <vector>

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
	 * Get available TTS voices
	 * @return Vector of available voices
	 */
	static std::vector<gpt_tts_t> getAvailableVoices();

private:
	String _apiKey;
	String _model;
	String _voice;
	bool _initialized;

	// Process API response
	void processResponse(int httpCode, const String& response, const String& text, AudioCallback callback);

	// Build JSON request payload
	String buildJsonPayload(const String& text);
};

extern GPTTtsService aiTts;

#endif // TTS_SERVICE_H