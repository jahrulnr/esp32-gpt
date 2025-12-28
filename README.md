# ESP32-GPT Library

An Arduino library for ESP32 microcontrollers to communicate with OpenAI GPT models and generate text-to-speech audio via HTTP API.

## Features

- Simple interface for sending prompts to GPT models
- Conversation context caching for multi-turn conversations
- Multiple model support (gpt-5-nano, gpt-4o-mini, etc.)
- Text-to-speech (TTS) functionality with multiple voice options
- Asynchronous HTTP requests with callback responses for both GPT and TTS
- Easy integration with Arduino projects

## Installation

1. Download or clone this repository
2. Copy the `ESP32-GPT` folder to your Arduino libraries directory
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

### Text-to-Speech Example

```cpp
#include <WiFi.h>
#include <tts.h>

// Replace with your credentials
const char* ssid = "your-ssid";
const char* password = "your-password";
#define TTS_API_KEY "your-openai-api-key"

// Callback for TTS audio responses
void ttsCallback(const String& text, const uint8_t* audioData, size_t audioSize) {
    Serial.println("TTS Text: " + text);
    if (audioData && audioSize > 0) {
        Serial.printf("Audio data received: %d bytes\n", audioSize);
        // Audio is in MP3 format, 24.00kHz sample rate, mono
        // You may need to decode MP3 and convert to PCM for I2S/analog output
        // Here you would play the audio data or save it
    } else {
        Serial.println("TTS failed - no audio data");
    }
}

void setup() {
    Serial.begin(115200);
    
    // Connect WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    
    // Initialize TTS
    aiTts.init(TTS_API_KEY);
    
    // Generate speech
    aiTts.textToSpeech("Hello, world!", ttsCallback);
}

void loop() {
    // Your code here
}
```

### Advanced TTS Usage

```cpp
// Set custom voice
aiTts.setVoice("alloy");

// Set custom model
aiTts.setModel("tts-1");

// Generate speech with specific voice
aiTts.textToSpeech("Hello!", "alloy", ttsCallback);

// Get available voices
auto voices = GPTTtsService::getAvailableVoices();
```

## GPT API Reference

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

## TTS API Reference

### TTS Initialization
```cpp
bool init(const String& apiKey)
bool isInitialized() const
```

### Generating Speech
```cpp
void textToSpeech(const String& text, AudioCallback callback)
void textToSpeech(const String& text, const String& voice, AudioCallback callback)
```

### TTS Configuration
```cpp
void setModel(const String& model)
void setVoice(const String& voice)
static std::vector<gpt_tts_t> getAvailableVoices()
```

- ESP32 board
- Arduino IDE or PlatformIO
- WiFi connection for API calls
- Valid OpenAI API key

## License

MIT License