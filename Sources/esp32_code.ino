#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"

#define IN1 12
#define IN2 13
#define IN3 15
#define IN4 14
#define enA 3
#define enB 1

WebServer sensorServer(81);
WebServer streamServer(80);

int watch = 0;
bool flashlight = false;

void handleExit(){
  watch = 0;
  sensorServer.send(200, "text/plain", "Streaming ended");
}

void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backwards() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void left() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void right() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void halt() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void handleSubmit(){
  String rx = sensorServer.arg("x");
  String ry = sensorServer.arg("y");
  int x = rx.toInt();
  int y = ry.toInt();

  if (x == 0 && y == 0) {
    halt();
  } else {
    if (abs(y) <= 1) {
      if (x < 0) {
        left();
      } else {
        right();
      }
    } else {
      if (y < 0) {
        forward();
      } else {
        backwards();
      }
    }
  }
  
  int sx = map(abs(x), 0, 6, 0, 255);
  int sy = map(abs(y), 0, 6, 0, 255);
  int sh = max(sx, sy);
  int sl = min(sx, sy);
  if (x < 0) {
    analogWrite(enA, sh - sl); 
    analogWrite(enB, sh);
  } else {
    analogWrite(enA, sh); 
    analogWrite(enB, sh - sl);
  }

  sensorServer.send(200, "text/plain", "Motors changed");
}

void flash(){
  flashlight = not flashlight;
  analogWrite(4, (flashlight == true) ? 200 : 0);
  
  sensorServer.send(200, "text/plain", "Flash changed");
}

void handleStream(){
  watch = 1;
  String response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  streamServer.sendContent(response);
  while (watch == 1) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }
    String header = 
      "--frame\r\n"
      "Content-Type: image/jpeg\r\n"
      "Content-Length: " + String(fb->len) + "\r\n\r\n";
    streamServer.sendContent(header);
    streamServer.sendContent((const char*)fb->buf, fb->len);
    streamServer.sendContent("\r\n");
    esp_camera_fb_return(fb);
  }
  streamServer.sendContent("--frame--\r\n");
}

void setup() {
  Serial.begin(115200);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(enA, OUTPUT);
  pinMode(enB, OUTPUT);

  // Set up Wi-Fi access point
  WiFi.mode(WIFI_AP);
  WiFi.softAP("MyESP32AP", "password");
  Serial.println("Wi-Fi access point started");

  // Initialize the camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  Serial.println("Camera initialized");

  // Set up handlers
  sensorServer.on("/exit", HTTP_GET, handleExit);
  
  sensorServer.on("/set", HTTP_POST, handleSubmit);

  sensorServer.on("/flash", HTTP_GET, flash);

  streamServer.on("/", handleStream);

  sensorServer.begin();
  streamServer.begin();

  // Thread for sensorServer
  xTaskCreatePinnedToCore(
    sensorTaskFunction,
    "Sensor Task",
    5000,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {
  streamServer.handleClient();
}

void sensorTaskFunction(void * parameter) {
  while (true) {
    sensorServer.handleClient();
  }
}
