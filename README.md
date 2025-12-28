# ESP32-GPT Library

An Arduino library for ESP32 microcontrollers to communicate with GPT AI models via HTTP API.

## Features

- Simple interface for sending prompts to GPT models
- Conversation context caching for multi-turn conversations
- Multiple model support (gpt-5-nano, gpt-4o-mini, etc.)
- Asynchronous HTTP requests with callback responses
- Easy integration with Arduino projects

## Installation

1. Download or clone this repository
2. Copy the `OpenAI` folder to your Arduino libraries directory
3. Restart Arduino IDE

For PlatformIO, add this library to your `lib` folder.

## Usage

### Basic Example

```cpp
#include <WiFi.h>
#include <gpt.h>

// Replace with your credentials
const char* ssid = "your-ssid";
const char* password = "your-password";
#define GPT_API_KEY "your-openai-api-key"

// Callback for responses
void gptCallback(const String& prompt, const String& response) {
    Serial.println("Response: " + response);
}

void setup() {
    Serial.begin(115200);
    
    // Connect WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    
    // Initialize GPT
    ai.init(GPT_API_KEY);
    
    // Send prompt
    ai.sendPrompt("Hello, tell me a joke!", gptCallback);
}

void loop() {
    // Your code here
}
```

### Advanced Usage

```cpp
// Set custom model
ai.setModel("gpt-4o-mini");

// Set system message
ai.setSystemMessage("You are a helpful assistant.");

// Send with context
ai.sendPrompt("What's the weather?", "Current location: New York", gptCallback);

// Reset conversation
ai.resetConversation();
```

## API Reference

### Initialization
```cpp
bool init(const String& apiKey)
bool isInitialized() const
```

### Sending Prompts
```cpp
void sendPrompt(const String& prompt, ResponseCallback callback)
void sendPrompt(const String& prompt, const String& additionalContext, ResponseCallback callback)
void sendPromptWithContext(const String& prompt,
                          const std::vector<std::pair<String, String>>& contextMessages,
                          ResponseCallback callback)
```

### Configuration
```cpp
void setModel(const String& model)
void setSystemMessage(const String& message)
void resetConversation()
static std::vector<GPTModel> getAvailableModels()
```

## Requirements

- ESP32 board
- Arduino IDE or PlatformIO
- WiFi connection for API calls
- Valid OpenAI API key

## License

MIT License