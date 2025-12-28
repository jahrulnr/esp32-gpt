

#include <WiFi.h>
#include <gpt.h>

// Replace with your WiFi credentials
const char* ssid = "your-ssid";
const char* password = "your-password";

#define GPT_MODEL "gpt-5-nano"
#define GPT_API_KEY "your-api-key"

// Callback function to handle GPT responses
void gptResponseCallback(const String& payload, const String& response) {
    Serial.println("GPT Prompt: " + payload);
    Serial.println("GPT Response: " + response);
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

    // Initialize GPT service
    if (ai.init(GPT_API_KEY)) {
        Serial.println("GPT service initialized successfully");
    } else {
        Serial.println("Failed to initialize GPT service");
        return;
    }

    // Send a test prompt
    ai.sendPrompt("Hello! Can you tell me a short joke?", gptResponseCallback);
}

void loop() {
    // Your main code here
    delay(1000);
}