#include <Config.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <SPI.h>
#include <GxEPD.h>
#include <GxGDEW027W3/GxGDEW027W3.cpp> // GxGDEW027W3 ->b/w  GxGDEW027C44 -> Color
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
// FONT used for title / message body
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

//Converting fonts with ümlauts: ./fontconvert *.ttf 18 32 252
// Point this to the Webpage rendering your calendar
String calendarUrl = "http://calendar.fasani.de/carlos";
// Point this to the screenshot endpoint (Should be placed on screenshot/index.php)
String screenshotHost = "calendar.fasani.de";
String screenshotPath = "/screenshot/";
unsigned int secondsToDeepsleep = 120;
// IMPORTANT: The url to the screenshot should respond with a BMP image
// Take care with the route since should not return a redirect or any other response than what expected
bool debugMode = false;
// mDNS: display.local
const char* domainName = "calendar"; 
String message;
// Makes a div id="m" containing response message to dissapear after 3 seconds
String javascriptFadeMessage = "<script>setTimeout(function(){document.getElementById('m').innerHTML='';},3000);</script>";

// TCP server at port 80 will respond to HTTP requests
ESP8266WebServer server(80);


//BUSY = D6;

//CS   = D8;
//DC   = D3;
//RST  = D4;
// GxIO_SPI(SPIClass& spi, int8_t cs, int8_t dc, int8_t rst = -1, int8_t bl = -1)
GxIO_Class io(SPI, D8, D3, D4); 
// GxGDEP015OC1(GxIO& io, uint8_t rst = D4, uint8_t busy = D2);
GxEPD_Class display(io, D4, D6); 

//unsigned long  startMillis = millis();
const unsigned long  serverDownTime = millis() + 60 * 60 * 1000; // Min / Sec / Millis Delay between updates, in milliseconds, WU allows 500 requests per-day maximum, set to every 10-mins or 144/day

WiFiClient client; // wifi client object

// Displays message doing a partial update
void displayMessage(String message, int height) {
  Serial.println("DISPLAY prints: "+message);
  display.setTextColor(GxEPD_WHITE);
  display.fillRect(0,0,display.width(),height,GxEPD_BLACK);
  display.setCursor(2, 25);
  display.print(message);
  display.updateWindow(0,0,display.width(),height, true); // Attempt partial update
  display.update(); // -> Since could not make partial updateWindow work
}

void handle_http_not_found() {
  server.send(404, "text/plain", "Not Found");
}

void handle_http_root() {

  String headers = "<head><link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css\">";
  headers += "<meta name='viewport' content='width=device-width,initial-scale=1'></head>";
  String html = "<body><main role='main'><div class='container-fluid'><div class='row'>";
  html += "<div class='col-md-12'><h4>" + String(domainName) + ".local</h4>";
  html += "<form id='f' action='/web-image' target='frame' method='POST'>";
  html += "<div class='row'><div class='col-sm-12'>";

  html += "<select name='zoom' style='width:7em' class='form-control float-right'>";
  html += "<option value='2'>2</option>";
  html += "<option value='1.7'>1.7</option>";
  html += "<option value='1.5'>1.5</option>";
  html += "<option value='1.4'>1.4</option>";
  html += "<option value='1.3'>1.3</option>";
  html += "<option value='1.2'>1.2</option>";
  html += "<option value='1.1'>1.1 10% zoomed</option>";
  html += "<option value='' selected>Zoom</option>";
  html += "<option value='1'>1 no zoom</option>";
  html += "<option value='.9'>.9 10% smaller</option>";
  html += "<option value='.85'>.85</option>";
  html += "<option value='.8'>.8</option>";
  html += "<option value='.7'>.7</option>";
  html += "<option value='.6'>.6</option>";
  html += "<option value='.5'>.5 half size</option></select>&nbsp;";
  
  html += "<select name='brightness' style='width:8em' class='form-control float-right'>";
  html += "<option value='150'>+5</option>";
  html += "<option value='140'>+4</option>";
  html += "<option value='130'>+3</option>";
  html += "<option value='120'>+2</option>";
  html += "<option value='110'>+1</option>";
  html += "<option value='' selected>Brightness</option>";
  html += "<option value='90'>-1</option>";
  html += "</select>";

  html += "</div></div>";
  html += "<input placeholder='http://' id='url' name='url' type='url' class='form-control' value='"+calendarUrl+"'>";
  html += "<div class='row'><div class='col-sm-12 form-group'>";
  html += "<input type='submit' value='Website screenshot' class='btn btn-mini btn-dark'>&nbsp;";
  html += "<input type='button' onclick='document.getElementById(\"url\").value=\"\"' value='Clean url' class='btn btn-mini btn-default'></div>";
  html += "</div></form>";
  
  html += "<form id='f2' action='/display-write' target='frame' method='POST'>";
  html += "<label for='title'>Title:</label><input id='title' name='title' class='form-control'><br>";
  html += "<textarea placeholder='Content' name='text' rows=4 class='form-control'></textarea>";
  html += "<input type='submit' value='Send to display' class='btn btn-success'></form>";
  html += "<a class='btn btn-default' role='button' target='frame' href='/display-clean'>Clean screen</a><br>";
  html += "<iframe name='frame'></iframe>";
  html += "<a href='/deep-sleep' target='frame'>Deep sleep</a><br>";
  html += "</div></div></div></main>";
  html += "</body>";

  server.send(200, "text/html", headers + html);
}

void handleDeepSleep() {
  server.send(200, "text/html", "Going to deep-sleep. Reset to wake up");
  delay(1);
  ESP.deepSleep(20e6);
}


void handleDisplayClean() {
  Serial.println("handleDisplayClean");
  display.fillScreen(GxEPD_WHITE);
  display.update();
  server.send(200, "text/html", "Clearing display");
}

void handleDisplayWrite() {
  Serial.println("handleDisplayWrite");
  display.fillScreen(GxEPD_WHITE);

  // Analizo el POST iterando cada value
  if (server.args() > 0) {
    for (byte i = 0; i < server.args(); i++) {

      if (server.argName(i) == "title") {
        display.setFont(&FreeMonoBold24pt7b);
        display.setCursor(10, 43);
        display.print(server.arg(i));
      }
      if (server.argName(i) == "text") {
        display.setFont(&FreeMonoBold12pt7b);
        display.setCursor(10, 75);
        display.print(server.arg(i));
      }
    }
  }
  display.update();
  server.send(200, "text/html", "Text sent to display");
}

uint16_t read16()
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read(); // MSB
  //Serial.print(result, HEX);
  return result;
}

uint32_t read32()
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read();
  ((uint8_t *)&result)[2] = client.read();
  ((uint8_t *)&result)[3] = client.read(); // MSB
  return result;
}


void handleWebToDisplay() {
  String url = calendarUrl;
  String zoom = ".8";
  String brightness = "100";
  if (server.args() > 0) {
    for (byte i = 0; i < server.args(); i++) {
      if (server.argName(i) == "url" && server.arg(i) != "") {
        url = server.arg(i);
      }
      if (server.argName(i) == "zoom" && server.arg(i) != "") {
        zoom = server.arg(i);
      }
      if (server.argName(i) == "brightness" && server.arg(i) != "") {
        brightness = server.arg(i);
      }
    }
  }
    if (url == "") {
      display.println("No Url received");
      display.update();
      return;
    }
  
  String image = screenshotPath+"?u=" + url + "&z=" + zoom + "&b=" + brightness +"&eink=GxGDEW027C44";
  String request;
  request  = "GET " + image + " HTTP/1.1\r\n";
  request += "Host: " + screenshotHost + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  Serial.println(screenshotHost+image);
// Falta ponerle un timeout
//  if (! client.connect(host, 80)) {
//    Serial.println("connection failed");
//    client.stop();
//    return;
//  }
  client.connect(screenshotHost, 80);
  client.print(request); //send the http request to the server
  client.flush();
  display.fillScreen(GxEPD_WHITE);
  
  uint32_t startTime = millis();
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }
  
  int displayWidth = display.width();
  int displayHeight= display.height();// Not used now
  uint8_t buffer[displayWidth]; // pixel buffer, size for r,g,b

  long bytesRead = 32; // summing the whole BMP info headers

 // NOTE: No need to discard headers anymore, unless they contain 0x4D42
long count = 0;
uint8_t lastByte;
// Start reading bits
while (client.available()) {
  count++;
  
  uint8_t clientByte = client.read();
  uint16_t bmp;
  ((uint8_t *)&bmp)[0] = lastByte; // LSB
  ((uint8_t *)&bmp)[1] = clientByte; // MSB
  if (debugMode) {
    Serial.print(bmp,HEX);Serial.print(" ");
    if (0 == count % 16) {
      Serial.println();
    }
    delay(1);
  }
  lastByte = clientByte;
  
  if (bmp == 0x4D42) { // BMP signature
    uint32_t fileSize = read32();
    uint32_t creatorBytes = read32();
    uint32_t imageOffset = read32(); // Start of image data
    uint32_t headerSize = read32();
    uint32_t width  = read32();
    uint32_t height = read32();
    uint16_t planes = read16();
    uint16_t depth = read16(); // bits per pixel
    uint32_t format = read32();
    
      Serial.print("->BMP starts here. File size: "); Serial.println(fileSize);
      Serial.print("Image Offset: "); Serial.println(imageOffset);
      Serial.print("Header size: "); Serial.println(headerSize);
      Serial.print("Width * Height: "); Serial.print(String(width) + " x " + String(height));
      Serial.print(" / Bit Depth: "); Serial.println(depth);
      Serial.print("Planes: "); Serial.println(planes);Serial.print("Format: "); Serial.println(format);
    
    if ((planes == 1) && (format == 0 || format == 3)) { // uncompressed is handled
      // Attempt to move pointer where image starts
      client.readBytes(buffer, imageOffset-bytesRead); 
      size_t buffidx = sizeof(buffer); // force buffer load
      
      for (uint16_t row = 0; row < height; row++) // for each line
      {
        //delay(1); // May help to avoid Wdt reset
        uint8_t bits;
        for (uint16_t col = 0; col < width; col++) // for each pixel
        {
          yield();
          // Time to read more pixel data?
          if (buffidx >= sizeof(buffer))
          {
            client.readBytes(buffer, sizeof(buffer));
            buffidx = 0; // Set index to beginning
          }
          switch (depth)
          {
            case 1: // one bit per pixel b/w format
              {
                if (0 == col % 8)
                {
                  bits = buffer[buffidx++];
                  bytesRead++;
                }
                uint16_t bw_color = bits & 0x80 ? GxEPD_WHITE : GxEPD_BLACK;
                display.drawPixel(col, displayHeight-row, bw_color);
                bits <<= 1;
              }
              break;
              
            case 4: // was a hard work to get here
              {
                if (0 == col % 2) {
                  bits = buffer[buffidx++];
                  bytesRead++;
                }   
                bits <<= 1;           
                bits <<= 1;
                uint16_t bw_color = bits > 0x80 ? GxEPD_WHITE : GxEPD_BLACK;
                display.drawPixel(col, displayHeight-row, bw_color);
                bits <<= 1;
                bits <<= 1;
              }
              break;
              
             case 24: // standard BMP format
              {
                uint16_t b = buffer[buffidx++];
                uint16_t g = buffer[buffidx++];
                uint16_t r = buffer[buffidx++];
                uint16_t bw_color = ((r + g + b) / 3 > 0xFF  / 2) ? GxEPD_WHITE : GxEPD_BLACK;
                display.drawPixel(col, displayHeight-row, bw_color);
                bytesRead = bytesRead +3;
              }
          }
        } // end pixel
      } // end line

       server.send(200, "text/html", "<div id='m'>Image sent to display</div>"+javascriptFadeMessage);
       Serial.println("Bytes read:"+ String(bytesRead));
       display.update();
       client.stop();
       break;
       
    } else {
      server.send(200, "text/html", "<div id='m'>Unsupported image format (depth:"+String(depth)+")</div>"+javascriptFadeMessage);
      display.setCursor(10, 43);
      display.print("Compressed BMP files are not handled. Unsupported image format (depth:"+String(depth)+")");
      display.update();
      
    }
  }
  
  }     
}

void loop() {
  // Add  milisec comparison to make server work for 1 min / 90 sec
  if (millis() < serverDownTime) {
    server.handleClient();
  } 
  // Note: Enable deepsleep only as last step when all the rest is working as you expect
  #ifdef DEEPSLEEP_ENABLED
    if (secondsToDeepsleep>SLEEP_AFTER_SECONDS) {
        Serial.println("Going to sleep one hour. Waking up only if D0 is connected to RST");
        ESP.deepSleep(120e6);  // 3600 = 1 hour in seconds . En 120 seg para prueba
    }
    secondsToDeepsleep++;
    delay(1000);
  #endif

}

void setup() {
  Serial.begin(115200);
  delay(100);
  display.init();
  display.setRotation(3); // Rotates display N times clockwise
  display.setFont(&FreeMonoBold12pt7b);
  display.setTextColor(GxEPD_BLACK);

  Serial.printf("Display w: %d h: %d", display.width(), display.height());
  uint8_t connectTries = 0;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED && connectTries<20) {
    Serial.print(" .");
    delay(500);
    connectTries++;
  }
Serial.println(WiFi.localIP());

  if (!MDNS.begin(domainName)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.printf("mDNS responder: %s.local\n",domainName);
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
  
  // Start HTTP server
  server.onNotFound(handle_http_not_found);
  // Routing
  server.on("/", handle_http_root);
  server.on("/display-write", handleDisplayWrite);
  server.on("/web-image", handleWebToDisplay);
  server.on("/display-clean", handleDisplayClean);
  server.on("/deep-sleep", handleDeepSleep);
  server.begin();

  handleWebToDisplay();
 /* if (WiFi.status() == WL_CONNECTED) {
     
     displayMessage("Estoy online", 40);
   } else {
     Serial.println(WiFi.status());
     displayMessage("Please check your credentials in Config.h\nCould not connect to "+String(WIFI_SSID),80);
  } 
  */
}
