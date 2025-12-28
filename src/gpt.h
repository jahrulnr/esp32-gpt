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

/**
 * Simple context cache for conversation history
 */
class ContextCache {
public:
	struct Message {
		String role;
		String content;
		unsigned long timestamp;
	};

	ContextCache(size_t maxMessages = 10) : _maxMessages(maxMessages) {}

	void addMessage(const String& role, const String& content) {
		Message msg = {role, content, millis()};
		_messages.push_back(msg);

		// Keep only recent messages
		if (_messages.size() > _maxMessages) {
			_messages.erase(_messages.begin());
		}
	}

	std::vector<Message> getRecentMessages(size_t count = 5) const {
		if (_messages.size() <= count) {
			return _messages;
		}

		// Return most recent messages
		return std::vector<Message>(_messages.end() - count, _messages.end());
	}

	void clear() {
		_messages.clear();
	}

private:
	std::vector<Message> _messages;
	size_t _maxMessages;
};

class GPTService {
public:
	// Callback type for GPT responses
	using ResponseCallback = std::function<void(const String& payload, const String& response)>;

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

	/**
	 * Reset conversation state for new conversation
	 */
	void resetConversation();

private:
	String _apiKey;
	String _model;
	String _systemMessage;
	bool _initialized;
	ContextCache _contextCache;
	String _previousResponseId; // For conversation state

	// Process API response
	void processResponse(int httpCode, const String& response, const String& userPrompt, ResponseCallback callback);

	// Build JSON request payload
	String buildJsonPayload(const String& userPrompt, const std::vector<std::pair<String, String>>& messages = {});

	// Extract response from JSON
	String extractResponse(const String& jsonResponse);
};

extern GPTService ai;

#endif // GPT_SERVICE_H