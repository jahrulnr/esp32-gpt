#include <WiFi.h>
#include <tts.h>

// Replace with your WiFi credentials
const char* ssid = "your-ssid";
const char* password = "your-password";

#define TTS_API_KEY "your-openai-api-key"

// Callback function to handle streaming TTS audio chunks
void ttsStreamCallback(const String& text, const uint8_t* audioChunk, size_t chunkSize, bool isLastChunk) {
    Serial.println("TTS Stream Text: " + text);

    if (audioChunk && chunkSize > 0) {
        Serial.printf("Audio chunk received: %d bytes, isLast: %s\n", chunkSize, isLastChunk ? "true" : "false");

        // Here you would typically play the audio data
        // For example, send to speaker or save to file

        if (isLastChunk) {
            Serial.println("TTS streaming completed!");
            // Finalize (e.g., reset state)
        }
    } else if (isLastChunk) {
        Serial.println("TTS streaming failed - no audio data");
    } else {
        Serial.println("Empty chunk received (possible timeout)");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Connect to WiFi
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected!");

    // Initialize TTS service
    if (aiTts.init(TTS_API_KEY)) {
        Serial.println("TTS service initialized successfully");
    } else {
        Serial.println("Failed to initialize TTS service");
        return;
    }

    // Send a test text to speech with streaming
    aiTts.textToSpeechStream("Hello! This is a test of the OpenAI streaming text to speech service. You can hear this audio in real-time as it's being generated.", ttsStreamCallback);
}

void loop() {
    // Your main code here
    delay(1000);
}