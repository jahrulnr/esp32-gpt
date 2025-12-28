#include <WiFi.h>
#include <tts.h>

// Replace with your WiFi credentials
const char* ssid = "your-ssid";
const char* password = "your-password";

#define TTS_API_KEY "your-openai-api-key"

// Callback function to handle TTS audio responses
void ttsAudioCallback(const String& text, const uint8_t* audioData, size_t audioSize) {
    Serial.println("TTS Text: " + text);

    if (audioData && audioSize > 0) {
        Serial.printf("Audio data received: %d bytes\n", audioSize);
        // Here you would typically play the audio data
        // For example, send to speaker or save to file
    } else {
        Serial.println("TTS failed - no audio data");
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

    // Send a test text to speech
    aiTts.textToSpeech("Hello! This is a test of the OpenAI text to speech service.", ttsAudioCallback);
}

void loop() {
    // Your main code here
    delay(1000);
}