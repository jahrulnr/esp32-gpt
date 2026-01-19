#ifndef GPT_SERVICE_H
#define GPT_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <functional>
#include <vector>

struct GPTModel {
	const char* id;
	const char* displayName;
};

class GPTService {
public:
	// Callback type for GPT responses
	using ResponseCallback = std::function<void(const String& payload, const String& response)>;
	using FuncCallback = std::function<void(const String& payload, const String& funcCall)>;

	GPTService();
	~GPTService();

	/**
	 * Initialize GPT service
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
	 * Send a prompt to GPT
	 * @param prompt User prompt
	 * @param callback Response callback
	 */
	void sendPrompt(const String& prompt, ResponseCallback callback);

	/**
	 * Send prompt with additional context
	 * @param prompt User prompt
	 * @param additionalContext Extra context information
	 * @param callback Response callback
	 */
	void sendPrompt(const String& prompt, const String& additionalContext, ResponseCallback callback);

	/**
	 * Send prompt with structured context messages
	 * @param prompt User prompt
	 * @param contextMessages Vector of role-content pairs
	 * @param callback Response callback
	 */
	void sendPromptWithContext(const String& prompt,
							  const std::vector<std::pair<String, String>>& contextMessages,
							  ResponseCallback callback);

	/**
	 * Set GPT model
	 * @param model Model name
	 */
	void setModel(const String& model) { _model = model; }

	/**
	 * Set system message
	 * @param message System prompt
	 */
	void setSystemMessage(const String& message) { _systemMessage = message; }

	/**
	 * Get available GPT models (sorted by cost)
	 * @return Vector of available models
	 */
	static std::vector<GPTModel> getAvailableModels();

private:
	String _apiKey;
	String _model;
	String _systemMessage;
	bool _initialized;

	// Process API response
	void processResponse(int httpCode, const String& response, const String& userPrompt, ResponseCallback callback);

	// Build JSON request payload
	String buildJsonPayload(const String& userPrompt, const std::vector<std::pair<String, String>>& messages = {});

	// Extract response from JSON
	String extractResponse(const String& jsonResponse);
	String extractFuncCall(const String& jsonResponse);
};

extern GPTService ai;

#endif // GPT_SERVICE_H