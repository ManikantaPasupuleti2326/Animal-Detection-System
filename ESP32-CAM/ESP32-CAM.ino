#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// ========== CAMERA PINS (AI Thinker ESP32-CAM) ==========
#define PWDN_GPIO_NUM     -1
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

// ========== PIR & FLASH PINS ==========
#define PIR_PIN 13          // connect PIR output to GPIO13
#define FLASH_LED_PIN 4     // built-in flash LED

// Insert your network credentials
#define WIFI_SSID "SGKR"
#define WIFI_PASSWORD "4321043210"

// Firebase project details
#define API_KEY "AIzaSyA7ZRbT-pW2zBqFTdjwGmIw8hOeUoUk5KU"
#define STORAGE_BUCKET_ID "gs://image-base64.firebasestorage.app"  // Storage bucket name

// User credentials
#define USER_EMAIL "madhusatti2007@gmail.com"
#define USER_PASSWORD "12345678"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-CAM with PIR and SD card");

  pinMode(PIR_PIN, INPUT);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW); // flash OFF by default

  // Camera config
  camera_config_t cameraConfig;
  cameraConfig.ledc_channel = LEDC_CHANNEL_0;
  cameraConfig.ledc_timer = LEDC_TIMER_0;
  cameraConfig.pin_d0 = Y2_GPIO_NUM;
  cameraConfig.pin_d1 = Y3_GPIO_NUM;
  cameraConfig.pin_d2 = Y4_GPIO_NUM;
  cameraConfig.pin_d3 = Y5_GPIO_NUM;
  cameraConfig.pin_d4 = Y6_GPIO_NUM;
  cameraConfig.pin_d5 = Y7_GPIO_NUM;
  cameraConfig.pin_d6 = Y8_GPIO_NUM;
  cameraConfig.pin_d7 = Y9_GPIO_NUM;
  cameraConfig.pin_xclk = XCLK_GPIO_NUM;
  cameraConfig.pin_pclk = PCLK_GPIO_NUM;
  cameraConfig.pin_vsync = VSYNC_GPIO_NUM;
  cameraConfig.pin_href = HREF_GPIO_NUM;
  cameraConfig.pin_sscb_sda = SIOD_GPIO_NUM;
  cameraConfig.pin_sscb_scl = SIOC_GPIO_NUM;
  cameraConfig.pin_pwdn = PWDN_GPIO_NUM;
  cameraConfig.pin_reset = RESET_GPIO_NUM;
  cameraConfig.xclk_freq_hz = 20000000;
  cameraConfig.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    cameraConfig.frame_size = FRAMESIZE_VGA;  // 640x480
    cameraConfig.jpeg_quality = 10;           // better quality
    cameraConfig.fb_count = 2;
  } else {
    cameraConfig.frame_size = FRAMESIZE_CIF;  // 352x288
    cameraConfig.jpeg_quality = 12;
    cameraConfig.fb_count = 1;
  }
   // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected!");

  // Firebase setup
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Init camera
  esp_err_t err = esp_camera_init(&cameraConfig);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Init SD card
  if(!SD_MMC.begin("/sdcard", true)){
    Serial.println("SD Card Mount Failed");
    return;
  }
  if(SD_MMC.cardType() == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }


  Serial.println("Setup complete. Waiting for PIR motion...");
}

void loop() {
  if (digitalRead(PIR_PIN) == HIGH) {
    Serial.println("Motion detected!");

    // Turn ON flash
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(200); // give some light before capture

    // Capture frame
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
    } else {
      File file = SD_MMC.open("/photo1.jpg", FILE_WRITE);
      if (!file) {
        Serial.println("Failed to open file for writing");
      } else {
        file.write(fb->buf, fb->len);
        file.close();
        Serial.println("Image saved: /photo1.jpg");
      }
      esp_camera_fb_return(fb);
    }

    // Turn OFF flash
    digitalWrite(FLASH_LED_PIN, LOW);
     if (Firebase.ready()) {
    // Example: upload an image from SD card
    String filePath = "/sdcard/photo1.jpg";   // Local file
    String storagePath = "/images/photo1.jpg"; // Firebase Storage path

    Serial.println("Uploading image...");

    if (Firebase.Storage.upload(&fbdo,
                                STORAGE_BUCKET_ID,  
                                filePath.c_str(),
                                mem_storage_type_sd,
                                storagePath.c_str(),
                                "image/jpeg")) {
      Serial.println("Upload success!");
      Serial.println(fbdo.downloadURL()); // Get file download URL
    } else {
      Serial.print("Upload failed: ");
      Serial.println(fbdo.errorReason());
    }
  }
    // Small cooldown so PIR doesn’t trigger too fast
    delay(10000);
  }
}