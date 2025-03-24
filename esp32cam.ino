#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// WiFi credentials
const char* ssid = "ESP32CAM_Stream";
const char* password = "12345678";

// Camera pins for AI Thinker ESP32-CAM
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

// Flash LED pin
#define FLASH_LED_PIN 4

// HTTP server and camera stream variables
httpd_handle_t camera_httpd = NULL;
bool flashState = false;

// HTML for the web page with flash button and fullscreen functionality
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>ESP32-CAM Stream</title>
  <style>
    body {margin:0; padding:0; background:#000; color:white; font-family:Arial, sans-serif;}
    .container {position:relative; width:100vw; height:100vh; overflow:hidden;}
    .video-container {width:100%; height:100%;}
    #stream {width:100%; height:100%; object-fit:contain;}
    .controls {position:fixed; bottom:20px; left:0; right:0; display:flex; justify-content:center; gap:10px;}
    button {background:rgba(0,0,0,0.5); color:white; border:2px solid white; padding:12px 20px;
      border-radius:30px; font-size:16px; cursor:pointer; transition:all 0.3s; display:flex; align-items:center;}
    button:hover {background:rgba(255,255,255,0.2);}
    .icon {width:20px; height:20px; margin-right:8px; display:inline-block;}
    .fullscreen-on .controls {opacity:0; transition:opacity 0.5s;}
    .fullscreen-on:hover .controls {opacity:1;}
  </style>
</head>
<body>
  <div class='container'>
    <div class='video-container'>
      <img src='/stream' id='stream' alt='Live Camera Stream'>
    </div>
    <div class='controls'>
      <button id='flashBtn' onclick='toggleFlash()'><span class='icon'>⚪</span>Turn Flash ON</button>
      <button id='fullscreenBtn' onclick='toggleFullscreen()'><span class='icon'>⊞</span>Fullscreen</button>
    </div>
  </div>
  <script>
    let isFullscreen = false;
    
    // Flash button handler
    function toggleFlash() {
      fetch('/flash')
        .then(response => response.text())
        .then(data => {
          checkFlashStatus();
        })
        .catch(error => console.error('Error:', error));
    }
    
    function checkFlashStatus() {
      fetch('/flash/status')
        .then(response => response.text())
        .then(data => {
          const flashBtn = document.getElementById('flashBtn');
          if(data === 'on') {
            flashBtn.innerHTML = '<span class="icon">💡</span>Turn Flash OFF';
            flashBtn.style.background = 'rgba(255,200,0,0.5)';
          } else {
            flashBtn.innerHTML = '<span class="icon">⚪</span>Turn Flash ON';
            flashBtn.style.background = 'rgba(0,0,0,0.5)';
          }
        })
        .catch(error => console.error('Error:', error));
    }
    
    // Fullscreen button handler
    function toggleFullscreen() {
      const container = document.querySelector('.container');
      if (!isFullscreen) {
        if (container.requestFullscreen) {
          container.requestFullscreen();
        } else if (container.webkitRequestFullscreen) {
          container.webkitRequestFullscreen();
        } else if (container.msRequestFullscreen) {
          container.msRequestFullscreen();
        }
        container.classList.add('fullscreen-on');
        document.getElementById('fullscreenBtn').innerHTML = '<span class="icon">⊠</span>Exit Fullscreen';
      } else {
        if (document.exitFullscreen) {
          document.exitFullscreen();
        } else if (document.webkitExitFullscreen) {
          document.webkitExitFullscreen();
        } else if (document.msExitFullscreen) {
          document.msExitFullscreen();
        }
        container.classList.remove('fullscreen-on');
        document.getElementById('fullscreenBtn').innerHTML = '<span class="icon">⊞</span>Fullscreen';
      }
      isFullscreen = !isFullscreen;
    }
    
    // Handle fullscreen change events
    document.addEventListener('fullscreenchange', handleFullscreenChange);
    document.addEventListener('webkitfullscreenchange', handleFullscreenChange);
    document.addEventListener('mozfullscreenchange', handleFullscreenChange);
    document.addEventListener('MSFullscreenChange', handleFullscreenChange);
    
    function handleFullscreenChange() {
      isFullscreen = !!document.fullscreenElement;
      const container = document.querySelector('.container');
      if (isFullscreen) {
        container.classList.add('fullscreen-on');
        document.getElementById('fullscreenBtn').innerHTML = '<span class="icon">⊠</span>Exit Fullscreen';
      } else {
        container.classList.remove('fullscreen-on');
        document.getElementById('fullscreenBtn').innerHTML = '<span class="icon">⊞</span>Fullscreen';
      }
    }
    
    // Check flash status on page load
    checkFlashStatus();
  </script>
</body>
</html>
)rawliteral";

// Stream JPEG frames from the camera
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];

  static int64_t last_frame = 0;
  if (!last_frame) {
    last_frame = esp_timer_get_time();
  }

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          Serial.println("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, "\r\n--frame\r\n", 12);
    }
    
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    
    if (res != ESP_OK) {
      break;
    }
    
    int64_t fr_end = esp_timer_get_time();
    int64_t frame_time = fr_end - last_frame;
    last_frame = fr_end;
    frame_time /= 1000;
    
    // Limit frame rate to reduce load
    if (frame_time < 50) {
      vTaskDelay(pdMS_TO_TICKS(50 - frame_time));
    }
  }
  
  return res;
}

// Handle root URL request
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, index_html, strlen(index_html));
}

// Handle flash toggle request
static esp_err_t flash_handler(httpd_req_t *req) {
  flashState = !flashState;
  digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
  Serial.printf("Flash turned %s\n", flashState ? "ON" : "OFF");
  
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, flashState ? "on" : "off");
  return ESP_OK;
}

// Handle flash status request
static esp_err_t flash_status_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, flashState ? "on" : "off");
  return ESP_OK;
}

// Start HTTP server
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  
  httpd_uri_t flash_uri = {
    .uri       = "/flash",
    .method    = HTTP_GET,
    .handler   = flash_handler,
    .user_ctx  = NULL
  };
  
  httpd_uri_t flash_status_uri = {
    .uri       = "/flash/status",
    .method    = HTTP_GET,
    .handler   = flash_status_handler,
    .user_ctx  = NULL
  };

  Serial.println("Starting web server on port: 80");
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &flash_uri);
    httpd_register_uri_handler(camera_httpd, &flash_status_uri);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-CAM Initializing...");
  
  // Flash LED pin setup
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  
  // Test flash to confirm it works
  Serial.println("Testing flash LED...");
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(500);
  digitalWrite(FLASH_LED_PIN, LOW);
  
  // Initialize camera
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Higher resolution
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;
  
  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    ESP.restart();
    return;
  }
  Serial.println("Camera initialized successfully.");
  
  // Start WiFi hotspot
  WiFi.softAP(ssid, password);
  Serial.print("WiFi Hotspot Started. SSID: ");
  Serial.println(ssid);
  Serial.print("Access the stream at: http://");
  Serial.println(WiFi.softAPIP());
  
  // Start HTTP server
  startCameraServer();
  Serial.println("System ready!");
}

void loop() {
  // Nothing to do here - the web server handles everything in the background
  delay(10);
}
