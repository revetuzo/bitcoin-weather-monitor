/*
 The Bitcoin weather monitor.
 A little client for displaying various Bitcoin related information using an ESP8266 dev board (NodeMCU).
 By @revetuzo.
 Sept 22, 2022

 Price API examples:
 https://www.bitstamp.net/api/v2/ticker/btcusd/, look for "last" in JSON response
 https://api.bitfinex.com/v1/pubticker/btcusd, look for "mid" in JSON response
 https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT, look for "price" in JSON response

 Bitstamp seems most reliable and stable.

 NTP configuration along the lines of https://werner.rothschopf.net/202011_arduino_esp8266_ntp_en.htm.
 Thanks to integration in ESP core and time library, almost nothing has to be done to integrate NTP!

 Note about the timed light-sleep mode: timers are stopped during light sleep, so we need to keep track of time
 manually through light sleep. This may lead to some time drift, which is not a big problem though,
 because NTP is polled automatically whenever we reconnect to WiFi on data update.
 See here for more details: https://github.com/esp8266/Arduino/tree/master/libraries/esp8266/examples/LowPowerDemo
 */

// Core, WiFi and connectivity related libraries
#include <ESP8266WiFi.h>

extern "C" {
#include "user_interface.h"
}

#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// communication with the TFT via SPI
#include <SPI.h>

// time library for handling NTP and date/time, NTP callback
#include <time.h>
#include <coredecls.h>

// Configuration of NTP
#define MY_NTP_SERVER "pool.ntp.org"           
#define MY_TZ "GMT0BST,M3.5.0/1,M10.5.0" // London

// temperature and humidity sensor (SHT21 compatible) via I2C
#include <Wire.h>
#include <Sodaq_SHT2x.h>

// TFT libraries for 1.8" or 1.77" 160x128 display with ST7735 display controller
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

// include fonts
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

// define pins for the TFT display, necessary for compatibility with default I2C pins for temperature sensor
#define TFT_CS         2 //NodeMCU pin D4
#define TFT_RST        16                                            
#define TFT_DC         0 //NodeMCU pin D3

// define colors
#define TWHITE ST7735_WHITE
#define TBLACK ST7735_BLACK
#define TORANGE ST7735_ORANGE
#define TGREEN ST7735_GREEN
#define TBLUE ST7735_BLUE
#define TCYAN ST7735_CYAN

// http://www.barth-dev.de/online/rgb565-color-picker/
#define DARKRED 0x5000 //(RGB 80 / 0 / 0)
#define DARKGREEN 0x0280 // (RGB 0 / 0 / 80)
#define DARKYELLOW 0x51E0 // (RGB 80 / 60 / 0)


// assign tft object
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// secure WiFi client
WiFiClientSecure sslclient;

// servers and clients
char priceserver[] = "www.bitstamp.net";
char mempoolserver[] = "mempool.space";
char weatherserver[] = "api.openweathermap.org";

HttpClient priceclient = HttpClient(sslclient, priceserver, 443);
HttpClient mempoolclient = HttpClient(sslclient, mempoolserver, 443);
HttpClient weatherclient = HttpClient(sslclient, weatherserver, 443);

// the openweathermap API string holds the geographic coordinates and the (free) API key
char weatherAPIString[] = "/data/2.5/weather?lat=51.5&lon=0.0&appid=MYAPIKEY";

// JSON documents for data processing
StaticJsonDocument<2048> doc;
StaticJsonDocument<2048> weather;

// WiFi secrets
#include "arduino_secrets.h" 
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;        // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;      // the WiFi radio's status

// timers and intervals
int lastDataUpdate = 0;
int dataUpdateInterval = 5*60;  // data update interval, 5 minutes;

int timePerScreen = 5000; // show each screen for milliseconds given by this number

// time related variables and parameters
time_t now;                         // this is the epoch
tm tm;                              // the structure tm holds time information in a more convient way

// callback function for NTP sync
void timeIsSet(bool from_sntp) {  
  if (from_sntp){
    Serial.println();
    Serial.println("[NTP] NTP sync complete.");
    Serial.println();
  }
}

//Week Days
String weekDaysFull[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
//String weekDays[7]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

//Month names
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};


String price = "NaN";
long sats;

String blockheight = "0";
int feerates[3] = {0, 0, 0};

float temperature;
float humidity;

float temperatureSelfHeatingCorrection = -0.4; // compensate for the heating of the sensor by the NodeMCU board and the display


// END INCLUDES AND DECLARATIONS

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

// BEGIN SETUP

void setup() {
  // Start NTP
  configTime(MY_TZ, MY_NTP_SERVER);
  settimeofday_cb(timeIsSet);

  //Initialize serial (for diagnostic output)
  Serial.begin(115200);

  // enable Wire for I2C communication
  Wire.begin();

  // initialize TFT
  tft.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
  tft.fillScreen(TBLACK);
  tft.setRotation(-1);

  tft.println("Bitcoin Weather Monitor");
  
  tft.println("Connecting to WiFi:");
  tft.print(ssid);

  Serial.print("Attempting to connect to SSID: ");
  Serial.print(ssid);

  WiFi.mode(WIFI_STA);
  
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    tft.print(".");
  }
  // you're connected now, so print out the data:
  Serial.println(" connected!");
  printWifiData();

  tft.println(" connected!");
  tft.print("IP: ");
  tft.println(WiFi.localIP());

  delay(100);

  // we choose to omit the certificate check for simplicity
  sslclient.setInsecure();
  
  tft.print("Price data... ");
  getPrice();
  lastDataUpdate = millis();
  if (price != "NaN"){
      tft.println("OK!");
  }
  delay(100);

  tft.print("Mempool data... "); 
  getBlockHeight();
  getRecommendedFees();
  lastDataUpdate = millis();
  if (blockheight != "0"){
    tft.println("OK!");
  }
  delay(100);

  tft.print("Local weather data... "); 
  getWeather();
  tft.println("OK!"); 
  
  delay(100);

  tft.print("Temperature and humidity sensor... "); 
  humidity = SHT2x.GetHumidity();
  temperature = SHT2x.GetTemperature();
  tft.println("OK!");  
  
  Serial.println();
  Serial.println("WiFi off");
  tft.println("WiFi off");
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin(0xFFFFFFF);
  delay(10);

  time(&now);
  lastDataUpdate = now;
  
  delay(1000);

}

// END SETUP

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

// BEGIN LOOP

void loop() {  
  // show clock screen
  tftClockScreen();

  // update data if last update is too long ago, given by dataUpdateInterval
  // while updating, keep showing the clock screen
  time(&now);
  if (now - lastDataUpdate >= dataUpdateInterval){
    dataUpdate();
    lastDataUpdate = now;
  }
  else {
    lightSleep(timePerScreen);
  }
  
  // update temperature and humidity, then show on screen
  getTempHum();
  tftTempWeatherScreen();
  lightSleep(timePerScreen);

  // show price screen
  tftPriceScreen();
  lightSleep(timePerScreen);

  // show mempool data screen
  tftMempoolScreen();
  lightSleep(timePerScreen);
}


// END LOOP

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////




// Temperature and Weather related functions and screens

void getTempHum() {
  // use an exponential moving average with alpha = 0.5 to get more stable readings
  float alpha = 0.5;
  humidity = alpha * SHT2x.GetHumidity() + (1-alpha) * humidity;
  temperature = alpha * SHT2x.GetTemperature() + (1-alpha) * temperature;
}


void tftTempWeatherScreen()
{
  Serial.println("[TEMPERATURE AND WEATHER SCREEN]");
  tft.fillScreen(TBLACK);

  // Let's draw a small table for debugging
  //tft.drawRect(0, 0, tft.width(), tft.height(), TWHITE);
  //tft.drawLine(0, 63, tft.width(), 63, TWHITE);

  char buffer[16];
  sprintf(buffer, "%4.1fC  %4.1f%% ", (temperature + temperatureSelfHeatingCorrection), humidity);

  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(5,18);
  tft.setTextColor(TGREEN);
  tft.println("Indoor");
  
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(TWHITE);
  drawCenterString(buffer, 79, 63-4-15);


  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(5,63+18-15);
  tft.setTextColor(TGREEN);
  tft.println("Outdoor");

  const char* outdoorDescription = weather["weather"][0]["description"];
  const float outdoorTemperature = weather["main"]["temp"];
  const int outdoorHumidity = weather["main"]["humidity"];
  const int outdoorPressure = weather["main"]["pressure"];
  const float outdoorWindspeed = weather["wind"]["speed"];

  tft.setFont(&FreeSansBold9pt7b);
  tft.setTextColor(TWHITE);
  
  sprintf(buffer, "%.1fC    %i%%", outdoorTemperature-273.15, outdoorHumidity);  
  drawCenterString(buffer, 79, 127-4-36);

  sprintf(buffer, "%ihPa  %i km/h", outdoorPressure, (int)(outdoorWindspeed*3.6));  
  drawCenterString(buffer, 79, 127-4-18);

  tft.setTextColor(TCYAN);
  drawCenterString(outdoorDescription, 79, 127-4);
}

void getWeather() {
  Serial.println();
  Serial.print("Making GET request to openweathermap server for current weather: ");
  Serial.println(weatherserver);
  
  weatherclient.get(weatherAPIString);

  // read the status code and body of the response
  int statusCode = weatherclient.responseStatusCode();
  String response = weatherclient.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  auto error = deserializeJson(weather, response);
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }

  const char* outdoorDescription = weather["weather"][0]["description"];
  const float temperature = weather["main"]["temp"];
  
}
////////////////////////////////////////////////////////////////////////////////////////////////////////

// Price related functions and screens

void getPrice() {
  Serial.println();
  Serial.print("Making GET request to price server: ");
  Serial.println(priceserver);
  
  //priceclient.get("/v1/pubticker/btcusd");  // bitfinex
  priceclient.get("/api/v2/ticker/btcusd/"); // bitstamp
  //priceclient.get("/api/v3/ticker/price?symbol=BTCUSDT");  // binance
  

  // read the status code and body of the response
  int statusCode = priceclient.responseStatusCode();
  String response = priceclient.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  auto error = deserializeJson(doc, response);
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }
  
  //const char* p = doc["mid"];  // bitfinex
  const char* p = doc["last"];  // bitstamp
  //const char* p = doc["price"];  // binance
  price = String(p);

  sats = 1/price.toFloat() * 100000000;

  Serial.print("Price: ");
  Serial.println(price.toFloat());
  Serial.print("Sats per USD: ");
  Serial.println(sats);
  
}


void tftPriceScreen()
{
  Serial.println("[PRICE SCREEN]");

  tft.fillScreen(TBLACK);

  // Let's draw a small table for debugging
  //tft.drawRect(0, 0, tft.width(), tft.height(), TWHITE);
  //tft.drawLine(0, 63, tft.width(), 63, TWHITE);
  
  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(5,18);
  tft.setTextColor(TORANGE);
  tft.println("BTCUSD");
  
  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextColor(TWHITE);
  drawAlignRightString(price.c_str(), 159-3, 63-4);

  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(5,63+18);
  tft.setTextColor(TORANGE);
  tft.println("Sats per USD");
  
  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextColor(TWHITE);
  drawAlignRightString(String(sats).c_str(), 159-3, 127-4);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

// Mempool related functions and screens

void tftMempoolScreen()
{
  Serial.println("[MEMPOOL SCREEN]");

  tft.fillScreen(TBLACK);

  // Let's draw a small table for debugging
  //tft.drawRect(0, 0, tft.width(), tft.height(), TWHITE);
  //tft.drawLine(0, 63, tft.width(), 63, TWHITE);
  
  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(3,18);
  tft.setTextColor(TORANGE);
  tft.println("Block Height");
  
  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextColor(TWHITE);
  drawAlignRightString(blockheight.c_str(), 159-3, 63-4);

  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(3,63+22);
  tft.setTextColor(TORANGE);
  tft.println("TX Fees (sats/vB)");
  
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(TWHITE);
  char buffer[16];
  tft.fillRect(0, 127-4-10-20, 54, 25, DARKRED);
  tft.fillRect(54, 127-4-10-20, 54, 25, DARKYELLOW);
  tft.fillRect(107, 127-4-10-20, 53, 25, DARKGREEN);
  
  sprintf(buffer, "%4d", feerates[0]);
  drawAlignRightString(buffer, 53-8, 127-4-10);
  
  sprintf(buffer, "%6d", feerates[1]);
  drawAlignRightString(buffer, 107-8, 127-4-10);
  
  sprintf(buffer, "%6d", feerates[2]);  
  drawAlignRightString(buffer, 159-8, 127-4-10);

}

void getBlockHeight() {
  Serial.println();
  Serial.print("Making GET request to mempool server for block height: ");
  Serial.println(mempoolserver);
  
  mempoolclient.get("/api/blocks/tip/height");

  // read the status code and body of the response
  int statusCode = mempoolclient.responseStatusCode();
  String response = mempoolclient.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  auto error = deserializeJson(doc, response);
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }

  blockheight = String(response);
}

void getRecommendedFees() {
  Serial.println();
  Serial.print("Making GET request to mempool server for recommended fees: ");
  Serial.println(mempoolserver);
  
  mempoolclient.get("/api/v1/fees/recommended");

  // read the status code and body of the response
  int statusCode = mempoolclient.responseStatusCode();
  String response = mempoolclient.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  auto error = deserializeJson(doc, response);
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }
  
  const int f = doc["fastestFee"];
  const int m = doc["halfHourFee"];
  const int s = doc["hourFee"];
  feerates[0] = f;
  feerates[1] = m;
  feerates[2] = s;

  Serial.println("Fees: " + String(f) + " " + String(m) + " " + String(s));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

// clock related functions and screens

void tftClockScreen()
{
  Serial.println("[CLOCK SCREEN]");

  time(&now);
  localtime_r(&now, &tm);

  Serial.println(now);
  
  tft.fillScreen(TBLACK);

  char currentTimeFormatted[5];
  sprintf(currentTimeFormatted, "%02i:%02i", tm.tm_hour, tm.tm_min); 
  
  // Let's draw a small table for debugging
  //tft.drawRect(0, 0, tft.width(), tft.height(), TWHITE);
  //tft.drawLine(0, 63, tft.width(), 63, TWHITE);  
  
  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextColor(TWHITE);
  drawCenterString(currentTimeFormatted, 79, 63-10);

  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(TWHITE);
  drawCenterString(weekDaysFull[tm.tm_wday].c_str(), 79, 127-4-35);

  char currentDateFormatted[10];
  sprintf(currentDateFormatted, "%02d.%02d.%04d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900);  
  
  drawCenterString(currentDateFormatted, 79, 127-4-10);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////

// WiFi and connectivity related functions

void dataUpdate(){
    Serial.println();
    Serial.println("[DATA UPDATE]: Retrieving data...");
    tft.setFont();
    drawCenterString("Updating data...", 79, 5);
    Serial.print("[DATA UPDATE]: WiFi on and reconnect");
    WiFi.forceSleepWake();
    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("OK!");
    
    getPrice();
    getBlockHeight();
    getRecommendedFees();
    getWeather();
    lastDataUpdate = millis();  
      
    Serial.println("[DATA UPDATE]: WiFi off");
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin(0xFFFFFFF);
    delay(10);
}

    
void printWifiData() {
  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);

  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}



////////////////////////////////////////////////////////////////////////////////////////////////////////

// helpers for displaying text on the TFT

// x is the x-coordinate of the string center
// y is the y-coordinate of the bottom of the string
void drawCenterString(const char *buf, int x, int y)
{
    int16_t x1, y1;
    uint16_t w, h;
    tft.setTextWrap(false);
    tft.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    tft.setCursor(x - w / 2, y);
    tft.print(buf);
}

// x is the x-coordinate of the string bottom right corner
// y is the y-coordinate of the string bottom right corner
void drawAlignRightString(const char *buf, int x, int y)
{
    int16_t x1, y1;
    uint16_t w, h;
    tft.setTextWrap(false);
    tft.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    tft.setCursor(x - w, y);
    tft.print(buf);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

// internal helpers

// performs light sleep for a given time (in ms)
void lightSleep(uint32_t sleepTime){
  Serial.print("Light sleep (");
  Serial.print(sleepTime);
  Serial.print(" ms) ...");
  Serial.flush();

  // we need to keep track of time through light sleep manually, because timers are turned off during sleep
  // get current epoch
  time(&now);
  uint32_t sleepStart = now;

  wifi_set_opmode(NULL_MODE);
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  wifi_fpm_open();
  wifi_fpm_set_wakeup_cb(wakeupCallback);  // set wakeup callback
  wifi_fpm_do_sleep(sleepTime * 1000);        // Sleep range = 10000 ~ 268,435,454 uS (0xFFFFFFE, 2^28-1)
  delay(sleepTime + 1);                // delay needs to be 1 ms longer than sleep or it only goes into Modem Sleep
  Serial.println("... continue.");  // the interrupt callback hits before this is executed

  // update system time with time before sleep plus duration of sleep
  struct timeval tv;
  tv.tv_sec = time_t(sleepStart + sleepTime/1000);
  tv.tv_usec = suseconds_t(0);  // this must be set, it's full of garbage otherwise!
  settimeofday(&tv, NULL);
  WiFi.mode(WIFI_OFF);             // after waking up from light sleep we have to force the modem off, otherwise
  WiFi.forceSleepBegin(0xFFFFFFF); // it is turned back on and does not even go to auto modem-sleep
}

// callback function on wake-up to demonstrate that light sleep operates correctly
void wakeupCallback() { 
  Serial.println(" Callback OK! ");
  Serial.flush();
  wifi_fpm_close();
}

// set initial delay for NTP sync after connection
uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000 ()
{
  return 2000UL; // 2s
}



// END SKETCH
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
