
// This code is made for an AI thinker ESP32-CAM board and will not work on 
// an arduino.

#include <Arduino.h>
#include "time.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <SPIFFS.h>
#include <FS.h>
#include <ESP32Servo.h>
#include <ezTime.h>

Servo myservo;

AsyncWebServer server(80);
boolean takeNewPhoto = false;
// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

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

// stuff for datetime
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 39600;
const int   daylightOffset_sec = 3600;

String SYS_PASS = "henry";
int feedingTime = 0;
bool accessGranted = false;
// input wifi name and password
const char* ssid = "Telstra01BF31";
const char* password = "y2fwvnsyfvmyncjz";
String timeInput;
String sizeInput;
int servoPrevTime = 0;
int gatePos=120;
bool gateOpen = false;
int currentTime = 0;
int feedPrevTime = 0;
int timeCheckPrevTime = 0;
String feedHour = "15";
String feedMin = "30";
String lastFeedDay = "0";
bool toFeed = false;
int amount;
 


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <h1>Home Page</h1><br>
  <a href="/ft"><button>Feeding Times</button></a><br>
  <a href="/cm"><button>Take a photo!</button></a><br>
  <form action="/get">
    Select Size:<select name="feedNowSize">
      <option value="1">1</option>
      <option value="2">2</option>
      <option value="3">3</option>
    </select>
    <button type="submit">Feed Now!</button>
  </form><br>
  <a href="/lock"><button>Lock</button></a><br>
</body></html>)rawliteral";

const char camera_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align:center; }
    .vert { margin-bottom: 10%; }
    .hori{ margin-bottom: 0%; }
  </style>
</head>
<body>
  <div id="container">
    <h2>ESP32-CAM Last Photo</h2>
    <p>It might take more than 5 seconds to capture a photo.</p>
    <p>
      <button onclick="capturePhoto()">CAPTURE PHOTO</button>
      <button onclick="location.reload();">REFRESH PAGE</button>
      <a href="/"><button>Back</button></a>
    </p>
  </div>
  <div><img src="saved-photo" id="photo" width="70%"></div><br>
  
</body>
<script>
  var deg = 0;
  function capturePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();
  }
  function isOdd(n) { return Math.abs(n % 2) == 1; }
</script>
</html>)rawliteral";

const char password_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <h1>Henry's Automatic Pet Feeder!</h1>
  <form action="/get">
    Input Passsword: <input type="text" name="password">
    <input type="submit" value="Submit">
  </form><br>
</body></html>)rawliteral";

const char setRegTime_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <h1>Set a regular feeding time!</h1>
  <form action="/get">
    Select a regular feeding time <input type="time" name="time">
    Select Size:<select name="feedNowSize">
      <option value="1">1</option>
      <option value="2">2</option>
      <option value="3">3</option>
    </select>
    <input type="submit" value="Submit">
  </form><br>
  <a href="/clearTime"><button>Clear Previous time</button></a><br>
  <a href="/"><button>Back</button></a>
</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  myservo.attach(14);
  myservo.write(gatePos);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
  }
  if (!SPIFFS.begin(true)) {
  Serial.println("An Error has occurred while mounting SPIFFS");
  ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }

  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  //pins for camera 
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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
  waitForSync();
  


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (accessGranted){
      request->send_P(200, "text/html", index_html);
    }
    else{
      request->send_P(200, "text/html", password_html);
    }
  });
  server.on("/ft", HTTP_GET, [](AsyncWebServerRequest *request){
    if(accessGranted) request->send_P(200, "text/html", setRegTime_html);
    else request->send_P(200, "text/html", password_html);
  });
  server.on("/cm", HTTP_GET, [](AsyncWebServerRequest *request){
    if(accessGranted) request->send_P(200, "text/html", camera_html);
    else request->send_P(200, "text/html", password_html);
  });
  server.on("/clearTime", HTTP_GET, [](AsyncWebServerRequest *request){
     if(accessGranted) {
      updateTime("--:---");
      request->send(200, "text/html", "Current time cleared <br><a href=\"/\"> Return to menu Menu</a>");
      Serial.println("Time cleared");
     }
     else request->send_P(200, "text/html", password_html);
    
  });
  server.on("/lock", HTTP_GET, [](AsyncWebServerRequest *request){
    accessGranted = false;
    request->send_P(200, "text/html", password_html);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (accessGranted){
      takeNewPhoto = true;
      request->send_P(200, "text/plain", "Taking Photo");
    } 
    else request->send_P(200, "text/html", password_html);
  });

  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
     if (accessGranted) request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
     else request->send_P(200, "text/html", password_html);
  });


  

  

  
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    if (request->hasParam("password")) {
      inputMessage = request->getParam("password")->value();
      inputParam = "password";
      if (inputMessage == SYS_PASS){
        accessGranted = true;
        request->send_P(200, "text/html", index_html);
      }
      else{
       request->send(200, "text/html", "password wrong!<br><a href=\"/\">try again</a>");
      }
      

    }
    else if (request->hasParam("time")&&accessGranted) {
      timeInput = request->getParam("time")->value();
      sizeInput = request->getParam("feedNowSize")->value();
      inputParam = "time";
      updateTime(timeInput+sizeInput);

      request->send(200, "text/html", "New feeding time set at " + timeInput + " with size " +sizeInput+ "<br><a href=\"/\">Return to Home Page</a>");

      

    }
    else if(request->hasParam("feedNowSize")&&accessGranted){
      inputMessage = request->getParam("feedNowSize")->value();
      inputParam = "feedNowSize";
      amount = inputMessage.toInt();
      toFeed = true;
      
      Serial.println("Feeding now with size: " + inputMessage);
      request->send(200, "text/html", "cat fed!<br><a href=\"/\">Return to Home Page</a>");
    }

    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
   
  });
  server.onNotFound(notFound);
  server.begin();
  Timezone Australia;
  Australia.setLocation("Sydney");
  
}

void loop() {
  currentTime = millis();
  checkFeedTime();
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
  }
  if(toFeed)feed(amount);

  delay(1);

}
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {
  //pointer to fb
  camera_fb_t * fb = NULL; 
  bool ok = 0; 
  do {
    
    Serial.println("Taking a photo");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera failed");
      return;
    }

    
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); 
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
   
    file.close();
    esp_camera_fb_return(fb);

   
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}
void feed(int level){
  if(gateOpen==false &&currentTime-servoPrevTime>1000){
    myservo.write(0);
    gateOpen = true;
    servoPrevTime = millis();
    Serial.println("opened");
  }
  if(gateOpen == true && currentTime-servoPrevTime>400+200*level){
    myservo.write(120);
    gateOpen = false;
    Serial.println("closed");
    servoPrevTime = millis();
    toFeed= false;
  }
}

void checkFeedTime(){
  if (currentTime-timeCheckPrevTime>30000){
  String feedTime = getTime();
  String feedHour = feedTime.substring(0,2);
  String feedMin = feedTime.substring(3,5);
  int feedAmount = feedTime.substring(5,6).toInt();
    if (feedHour!="--"){
      Serial.println("meows will be fed at: "+ feedHour + ":" + feedMin + "with size" + feedAmount);
      Timezone Australia;
      Australia.setLocation("Sydney");
      String dateTime = Australia.dateTime(ISO8601);
      if(!dateTime){
        Serial.println("Failed to obtain time");
      }
      else{
        Serial.println(dateTime);
        String day = (dateTime.substring(8,10));
        String hour = (dateTime.substring(11,13));
        String min = (dateTime.substring(14,16));
        
        

        if (day!=lastFeedDay && hour == feedHour && min == feedMin){
          toFeed = true;
          amount = feedAmount;
          lastFeedDay = day;
        }
        timeCheckPrevTime = millis();
      }
    }
  }
}
void updateTime(String newTime){
  File file = SPIFFS.open("/time.txt","w");
    if (!file) {
    Serial.println("Error opening file for writing");
    return;
    }
     int bytesWritten = file.print(newTime);
    if (bytesWritten > 0) {
      Serial.println("File was written");
      Serial.println(bytesWritten);
    } 
    else {
      Serial.println("File write failed");
    }
  file.close();
}
String getTime(){
  String fileContent;
  File file =  SPIFFS.open("/time.txt", "r");
  if (!file) {
    Serial.println("Error opening file for reading");
  }
  else{
    while(file.available()){
    fileContent+=String((char)file.read());
    }
  }
  return fileContent;
}
