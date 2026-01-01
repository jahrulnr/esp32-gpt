/**
 * ESP32-GPT Speech-to-Speech Streaming Example
 *
 * This example demonstrates how to use the ESP32-GPT library to perform
 * continuous speech-to-speech streaming using OpenAI's Realtime API.
 *
 * Requirements:
 * - ESP32 board with WiFi
 * - OpenAI API key
 * - Microphone input (for audioFillCallback)
 * - Speaker output (for audioResponseCallback)
 *
 * Note: This example shows the basic structure for streaming STS.
 * You need to implement actual audio input/output in the callbacks.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <gpt.h>
#include <stt.h>
#include <sts.h>
#include <LittleFS.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// OpenAI API key
const char* apiKey = "YOUR_OPENAI_API_KEY";

// Audio buffer for demo
uint8_t audioBuffer[1024];
size_t audioBufferSize = 0;

// Callback function for audio fill (provide audio data to send)
size_t audioFillCallback(uint8_t* buffer, size_t maxSize) {
  // In a real implementation, this would read from microphone
  // For demo, we'll simulate some audio data
  if (audioBufferSize < sizeof(audioBuffer)) {
    size_t toCopy = _min(maxSize, sizeof(audioBuffer) - audioBufferSize);
    memcpy(buffer, &audioBuffer[audioBufferSize], toCopy);
    audioBufferSize += toCopy;
    return toCopy;
  }
  return 0; // No more data
}

// Callback function for audio response (receive and play audio)
void audioResponseCallback(const uint8_t* audioData, size_t audioSize, bool isLastChunk) {
  if (audioData && audioSize > 0) {
    Serial.printf("Received audio chunk: %d bytes\n", audioSize);
    // In a real implementation, this would send to speaker
    // For demo, just log the data
  }

  if (isLastChunk) {
    Serial.println("Audio response complete");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-GPT Speech-to-Speech Streaming Example");

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Initialize STS service
  if (!aiSts.init(apiKey, LittleFS)) {
    Serial.println("Failed to initialize STS service");
    return;
  }

  // Fill demo audio buffer with some dummy data
  for (size_t i = 0; i < sizeof(audioBuffer); i++) {
    audioBuffer[i] = (uint8_t)(i % 256);
  }
  audioBufferSize = 0;

  Serial.println("Starting streaming STS session...");

  // Start streaming
  if (aiSts.start(audioFillCallback, audioResponseCallback)) {
    Serial.println("Streaming started successfully");
  } else {
    Serial.println("Failed to start streaming");
  }
}

void loop() {
  // Check streaming status
  if (aiSts.isStreaming()) {
    Serial.println("Streaming is active");
  } else {
    Serial.println("Streaming is not active");
  }

  delay(5000); // Check every 5 seconds

  // Example: Stop streaming after 30 seconds
  static unsigned long startTime = millis();
  if (millis() - startTime > 30000) { // 30 seconds
    Serial.println("Stopping streaming...");
    aiSts.stop();
  }
}