#define ENABLE_USER_AUTH
#define ENABLE_STORAGE
#define ENABLE_FS

#include <Arduino.h>
#include <FirebaseClient.h>
#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"

#define WIFI_SSID "SGKR"
#define WIFI_PASSWORD "4321043210"

#define API_KEY "AIzaSyA7ZRbT-pW2zBqFTdjwGmIw8hOeUoUk5KU"
#define USER_EMAIL "madhusatti2007@gmail.com"
#define USER_PASSWORD "12345678"

// Define the Firebase storage bucket ID e.g bucket-name.appspot.com */
#define STORAGE_BUCKET_ID "image-base64.firebasestorage.app"

// Photo path in filesystem and photo path in Firebase bucket
#define FILE_PHOTO_PATH "/photo.jpg"
#define BUCKET_PHOTO_PATH "/CAM1/photo.jpg"

// PIR Sensor Pin
#define PIR_PIN 12  // GPIO12 for PIR sensor output
#define LED_PIN 2   // CAM LED
#define FLASH_LED 4 // Built-in flash LED
#define WiFi_LED 14 // WiFi LED
#define BUZZ_PIN 15 // BUZZER 

// User functions
void processData(AsyncResult &aResult);
void file_operation_callback(File &file, const char *filename, file_operating_mode mode);

FileConfig media_file(FILE_PHOTO_PATH, file_operation_callback); // Can be set later with media_file.setFile("/image.png", file_operation_callback);

File myFile;

// Authentication
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 1000 /* expire period in seconds (<3600) */);

// Firebase components
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
Storage storage;

bool taskComplete = false;
bool takeNewPhoto = false;  // Changed to be controlled by PIR
bool motionDetected = false;
bool uploadComplete = false; // Added to track upload completion
unsigned long lastMotionTime = 0;
const unsigned long motionCooldown = 2000;  // 10 seconds cooldown between detections
unsigned long uploadCompleteTime = 0; // Added to track when upload finished
const unsigned long postUploadDelay = 5000; // 5 seconds delay after upload

AsyncResult storageResult;

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Interrupt Service Routine for PIR sensor
void IRAM_ATTR detectsMovement() {
  if (digitalRead(PIR_PIN) && (millis() - lastMotionTime > motionCooldown)) {
    motionDetected = true;
    lastMotionTime = millis();
    // digitalWrite(BUZZ_PIN,HIGH);
  }
//digitalWrite(BUZZ_PIN,LOW);
}

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS( void ) {
  // Turn on flash before capturing
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(FLASH_LED, HIGH);
  delay(200); // Give flash time to light up
  
  // Dispose first pictures because of bad quality
  camera_fb_t* fb = NULL;
  // Skip first 3 frames (increase/decrease number as needed).
  for (int i = 0; i < 10; i++) {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
  }
    
  // Take a new photo
  fb = NULL;  
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    digitalWrite(LED_PIN, LOW);
    digitalWrite(FLASH_LED,LOW);// Turn off flash
    return;
  }  

  // Photo file name
  Serial.printf("Picture file name: %s\n", FILE_PHOTO_PATH);
  File file = LittleFS.open(FILE_PHOTO_PATH, FILE_WRITE);

  // Insert the data in the photo file
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  }
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.print("The picture has been saved in ");
    Serial.print(FILE_PHOTO_PATH);
    Serial.print(" - Size: ");
    Serial.print(fb->len);
    Serial.println(" bytes");
  }
  // Close the file
  file.close();
  esp_camera_fb_return(fb);
  
  // Turn off flash
  digitalWrite(LED_PIN, LOW);
  digitalWrite(FLASH_LED, LOW);
  delay(100);
}

void initLittleFS(){
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("LittleFS mounted successfully");
  }
}

void initWiFi(){
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    digitalWrite(WiFi_LED, LOW);
  }
  Serial.println("Connected to WiFi");
  digitalWrite(WiFi_LED, HIGH);
}

void initCamera(){
 // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;  // Reduced from UXGA to SVGA (800x600)
    config.jpeg_quality = 12;            // Reduced quality (higher number = lower quality)
    config.fb_count = 1;                 // Only 1 frame buffer
  } else {
    config.frame_size = FRAMESIZE_VGA;   // VGA (640x480) if no PSRAM
    config.jpeg_quality = 15;            // Even lower quality
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
  Serial.println("Camera init success");
}

void setup(){
    Serial.begin(115200);
    
    // Initialize PIR sensor
    pinMode(PIR_PIN, INPUT_PULLDOWN);
    pinMode(LED_PIN, OUTPUT);
    pinMode(WiFi_LED,OUTPUT);
    pinMode(BUZZ_PIN,OUTPUT);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(WiFi_LED, LOW);
    
    // Attach interrupt to PIR sensor
    attachInterrupt(digitalPinToInterrupt(PIR_PIN), detectsMovement, RISING);
    
    Serial.println("Motion detection activated");
    
    initWiFi();
    initCamera();

    Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

    initLittleFS();

    // Configure SSL client
    ssl_client.setInsecure();
    ssl_client.setConnectionTimeout(1000);
    ssl_client.setHandshakeTimeout(5);
    Serial.println("Initializing app...");
    initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");

    app.getApp<Storage>(storage);

    Serial.println("Listing files in LittleFS:");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
        Serial.println(file.name());
        file = root.openNextFile();
    }

    Serial.println("Setup complete. Waiting for motion...");
}

void loop(){
    // To maintain the authentication process.
    app.loop();

    // Check if upload is complete and we need to wait before resetting
    if (uploadComplete && (millis() - uploadCompleteTime >= postUploadDelay)) {
        uploadComplete = false;
        Serial.println("Upload complete. Waiting for motion detection...");
    }

    // Check for motion detection
    if (motionDetected && !uploadComplete) {
      motionDetected = false;
      Serial.println("Motion detected! Preparing to capture photo.");
      takeNewPhoto = true;
      taskComplete = false;
    }

    if (app.ready() && !taskComplete && takeNewPhoto){
        taskComplete = true;
        takeNewPhoto = false;

        Serial.println("Capturing photo...");
        capturePhotoSaveLittleFS();
        
        // Async call with callback function.
        Serial.println("Uploading to Firebase...");
        storage.upload(aClient, FirebaseStorage::Parent(STORAGE_BUCKET_ID, BUCKET_PHOTO_PATH), getFile(media_file), "image/jpg", processData, "⬆️  uploadTask");
    }
    
    // Small delay to prevent overwhelming the loop
    delay(100);
}

void processData(AsyncResult &aResult)
{
    // Exits when no result available when calling from the loop.
    if (!aResult.isResult())
        return;

    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.downloadProgress())
    {
        Firebase.printf("Downloaded, task: %s, %d%s (%d of %d)\n", aResult.uid().c_str(), aResult.downloadInfo().progress, "%", aResult.downloadInfo().downloaded, aResult.downloadInfo().total);
        if (aResult.downloadInfo().total == aResult.downloadInfo().downloaded)
        {
            Firebase.printf("Download task: %s, complete!\n", aResult.uid().c_str());
        }
    }

    if (aResult.uploadProgress())
    {
        Firebase.printf("Uploaded, task: %s, %d%s (%d of %d)\n", aResult.uid().c_str(), aResult.uploadInfo().progress, "%", aResult.uploadInfo().uploaded, aResult.uploadInfo().total);
        if (aResult.uploadInfo().total == aResult.uploadInfo().uploaded)
        {
            Firebase.printf("Upload task: %s, complete!\n", aResult.uid().c_str());
            Serial.print("Download URL: ");
            Serial.println(aResult.uploadInfo().downloadUrl);
            
            // Set upload complete flag and record the time
            uploadComplete = true;
            uploadCompleteTime = millis();
        }
    }
}

void file_operation_callback(File &file, const char *filename, file_operating_mode mode){
    // FILE_OPEN_MODE_READ, FILE_OPEN_MODE_WRITE and FILE_OPEN_MODE_APPEND are defined in this library
    // MY_FS is defined in this example
    switch (mode)    {
    case file_mode_open_read:
        myFile = LittleFS.open(filename, "r");
        if (!myFile || !myFile.available()) {
            Serial.println("[ERROR] Failed to open file for reading");
        }
        break;
    case file_mode_open_write:
        myFile = LittleFS.open(filename, "w");
        break;
    case file_mode_open_append:
        myFile = LittleFS.open(filename, "a");
        break;
    case file_mode_remove:
        LittleFS.remove(filename);
        break;
    default:
        break;
    }
    // Set the internal FS object with global File object.
    file = myFile;
}