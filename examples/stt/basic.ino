/**
 * ESP32-GPT Transcription Example
 *
 * This example demonstrates how to use the ESP32-GPT library to transcribe
 * audio files using OpenAI's Whisper API.
 *
 * Requirements:
 * - ESP32 board with WiFi
 * - OpenAI API key
 * - Audio file in WAV format stored on LittleFS
 *
 * Note: Make sure to upload the audio file to LittleFS before running this example.
 */

#include <WiFi.h>
#include <gpt.h>
#include <stt.h>
#include <LittleFS.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// OpenAI API key
const char* apiKey = "YOUR_OPENAI_API_KEY";

// Audio file path on LittleFS
const char* audioFilePath = "/response.wav";

// Callback function for transcription results
void transcriptionCallback(const String& filePath, const String& transcription, const String& usageJson) {
  Serial.println("Transcription completed!");
  Serial.printf("File: %s\n", filePath.c_str());
  Serial.printf("Transcription: %s\n", transcription.c_str());
  Serial.printf("Usage: %s\n", usageJson.c_str());
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-GPT Transcription Example");

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

  // Initialize transcription service
  if (!aiStt.init(apiKey, LittleFS)) {
    Serial.println("Failed to initialize transcription service");
    return;
  }

  // Check if audio file exists
  if (!LittleFS.exists(audioFilePath)) {
    Serial.printf("Audio file %s not found on LittleFS\n", audioFilePath);
    Serial.println("Please upload an audio file to LittleFS first");
    return;
  }

  Serial.printf("Transcribing audio file: %s\n", audioFilePath);

  // Transcribe audio
  aiStt.transcribeAudio(audioFilePath, transcriptionCallback);
}

void loop() {
  // Nothing to do here
  delay(1000);
}