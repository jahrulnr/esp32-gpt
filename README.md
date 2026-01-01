# ESP32-GPT Library

An Arduino library for ESP32 microcontrollers to communicate with OpenAI GPT models and generate text-to-speech audio via HTTP API.

## Features

- Simple interface for sending prompts to GPT models
- Conversation context caching for multi-turn conversations
- Multiple model support (gpt-5-nano, gpt-4o-mini, etc.)
- Text-to-speech (TTS) functionality with multiple voice options
- Streaming TTS for real-time audio generation with lower latency
- Audio transcription using OpenAI's
- Speech-to-speech (STS) functionality using OpenAI's Realtime API (untested)
- Continuous streaming STS for real-time voice conversations (untested)
- Asynchronous HTTP/WebSocket requests with callback responses for GPT, TTS, transcription, and STS
- Easy integration with Arduino projects

## Installation

1. Download or clone this repository
2. Copy the `ESP32-GPT` folder to your Arduino libraries directory
3. Restart Arduino IDE

For PlatformIO, add this library to your `lib` folder.

## Examples

The `examples` folder contains sample sketches demonstrating different features:

- `gpt/basic.ino` - Basic GPT conversation
- `tts/basic.ino` - Regular text-to-speech
- `tts/streaming.ino` - Streaming text-to-speech for lower latency
- `stt/basic.ino` - Audio transcription
- `sts/basic.ino` - Speech-to-speech conversion and streaming

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

### Streaming Text-to-Speech Example

```cpp
#include <WiFi.h>
#include <tts.h>

// Replace with your credentials
const char* ssid = "your-ssid";
const char* password = "your-password";
#define TTS_API_KEY "your-openai-api-key"

// Callback for streaming TTS audio chunks
void ttsStreamCallback(const String& text, const uint8_t* audioChunk, size_t chunkSize, bool isLastChunk) {
    Serial.println("TTS Stream Text: " + text);
    
    if (audioChunk && chunkSize > 0) {
        Serial.printf("Audio chunk received: %d bytes, isLast: %s\n", chunkSize, isLastChunk ? "true" : "false");
        // Process audio chunk immediately for real-time playback
        // Lower latency compared to waiting for full audio
        
        if (isLastChunk) {
            Serial.println("TTS streaming completed!");
        }
    } else if (isLastChunk) {
        Serial.println("TTS streaming failed - no audio data");
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
    
    // Generate speech with streaming for lower latency
    aiTts.textToSpeechStream("Hello! This streaming TTS allows for real-time audio playback.", ttsStreamCallback);
}

void loop() {
    // Your code here
}
```

### Audio Transcription Example

```cpp
#include <WiFi.h>
#include <SPIFFS.h>
#include <transcription.h>

// Replace with your credentials
const char* ssid = "your-ssid";
const char* password = "your-password";
#define TRANSCRIPTION_API_KEY "your-openai-api-key"

// Callback for transcription responses
void transcriptionCallback(const String& filePath, const String& transcription, const String& usageJson) {
    Serial.println("Transcription completed!");
    Serial.printf("File: %s\n", filePath.c_str());
    Serial.printf("Transcription: %s\n", transcription.c_str());
    Serial.printf("Usage: %s\n", usageJson.c_str());
}

void setup() {
    Serial.begin(115200);
    
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    
    // Connect WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    
    // Initialize transcription service
    aiStt.init(TRANSCRIPTION_API_KEY, LittleFS);
    
    // Transcribe audio file (must be WAV format)
    aiStt.transcribeAudio("/audio.wav", transcriptionCallback);
}

void loop() {
    // Your code here
}
```

### Speech-to-Speech Example

```cpp
#include <WiFi.h>
#include <SPIFFS.h>
#include <sts.h>

// Replace with your credentials
const char* ssid = "your-ssid";
const char* password = "your-password";
#define STS_API_KEY "your-openai-api-key"

// Callback for STS responses
void stsCallback(const String& filePath, const uint8_t* audioData, size_t audioSize) {
    Serial.println("Speech-to-speech completed!");
    Serial.printf("File: %s\n", filePath.c_str());
    Serial.printf("Audio size: %d bytes\n", audioSize);
    
    // Save or play the audio response
    // Your audio output code here
}

void setup() {
    Serial.begin(115200);
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    
    // Initialize STS service
    aiSts.init(STS_API_KEY, LittleFS);
    
    // Process audio file (must be WAV format)
    aiSts.speechToSpeech("/input.wav", stsCallback);
}

void loop() {
    // Your code here
}
```

### Streaming Speech-to-Speech Example

```cpp
#include <WiFi.h>
#include <sts.h>

// Replace with your credentials
const char* ssid = "your-ssid";
const char* password = "your-password";
#define STS_API_KEY "your-openai-api-key"

// Callback to provide audio data for streaming
size_t audioFillCallback(uint8_t* buffer, size_t maxSize) {
    // Read audio from microphone or buffer
    // Return number of bytes written to buffer
    return bytesRead;
}

// Callback to receive streaming audio responses
void audioResponseCallback(const uint8_t* audioData, size_t audioSize, bool isLastChunk) {
    // Play or process the received audio
    if (isLastChunk) {
        Serial.println("Response complete");
    }
}

void setup() {
    Serial.begin(115200);
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    
    // Initialize STS service
    aiSts.init(STS_API_KEY, LittleFS);
    
    // Start continuous streaming
    aiSts.start(audioFillCallback, audioResponseCallback);
}

void loop() {
    // Monitor streaming status
    if (aiSts.isStreaming()) {
        // Streaming is active
    }
    
    // Stop streaming when needed
    // aiSts.stop();
}
```

### Advanced Transcription Usage

```cpp
// Set custom model
aiStt.setModel("whisper-1");

// Transcribe with specific model
aiStt.transcribeAudio("/audio.wav", "gpt-4o-transcribe", transcriptionCallback);

// Get available models
auto models = GPTSttService::getAvailableModels();
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

### Streaming Speech Generation
```cpp
void textToSpeechStream(const String& text, StreamCallback callback)
void textToSpeechStream(const String& text, const String& voice, StreamCallback callback)
```

### TTS Configuration
```cpp
void setModel(const String& model)
void setVoice(const String& voice)
static std::vector<gpt_tts_t> getAvailableVoices()
```

## Transcription API Reference

### Transcription Initialization
```cpp
bool init(const String& apiKey, fs::FS& fs)
bool isInitialized() const
```

### Transcribing Audio
```cpp
void transcribeAudio(const String& filePath, TranscriptionCallback callback)
void transcribeAudio(const String& filePath, const String& model, TranscriptionCallback callback)
```

### Transcription Configuration
```cpp
void setModel(const String& model)
static std::vector<gpt_transcription_t> getAvailableModels()
```

- ESP32 board
- Arduino IDE or PlatformIO
- WiFi connection for API calls
- Valid OpenAI API key

## License

MIT License