# bitcoin-weather-monitor

The Bitcoin weather monitor shows Bitcoin and weather data on a small TFT display and is powered by an ESP8266 NodeMCU board.

Shopping list:
- NodeMCU v3.4 ESP8266 board (31.2 mm x 57.9 mm)
- 1.77" TFT SPI display (128 x 160) with ST7735 driver (other sizes 
- GY-21 breakout board for the HTU21 or SHT21 temprature and humidity sensor
- cables with female/female Dupont connectors, approx. 10cm length
- some M2.5 and M3 screws
- a 10 Ohm resistor and some heat-shrink tube (optional) (+ a soldering iron)

Instructions:
- print the enclosure on your 3D printer (three parts: enclosure, backplate, sensor rack)
- get an Openweathermap free API key
- run your own Bitcoin node (optional, but highly recommended anyway!)

- install Arduino IDE 1.8.x
- install ESP8266 core
- install libraries:
  - ArduinoHttpClient
  - ArduinoJson
  - Adafruit_GFX
  - Adafruit_ST7735
  - Sodaq_SHT2x
  
- find and follow instructions on how to connect the NodeMCU board with Arduino IDE via USB

- modify the .ino sketch file:
  - #define MY_TZ, this string defines your timezone and DST rules (set to GMT by default)
  - char weatherAPIString: here you have to place your Openweathermap API key
  - enter your WiFi SSID and password in arduino_secrets.h
  - you can change the date format in the tftClockScreen() function
  - you can change the temperature units in the tftTempWeatherScreen() function
  - set the data update inverval and time per screen with dataUpdateInterval and timePerScreen in the sketch header
  
- compile the sketch and upload to your NodeMCU board
- use the Serial Monitor during first startup to check that all data are correctly retrieved
  
Notes:
- NTP is included in the latest ESP8266 core library and does not require additional libraries. Sync is automatically performed whenever we turn on WiFi.
- See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for timezone strings.
- We put the ESP8266 into sleep mode as much as possible. WiFi is only turned on during data update. Otherwise, the module is put into "light sleep" mode. This requires some code tweaking, but reduces the current draw by approximately 8 mA and reduces the heat load on the temperature sensor. See the function lightSleep() for the annoying details.
- The 10 Ohm resistor for the TFT LED is optional. It reduces the screen brightness a bit and reduces the current draw by approximately 15 mA (roughly 30%).
- The data types for the various variables are not optimized.
- If you have problems connecting the NodeMCU board, the cable might be an issue. Make sure you have a high-quality cable suitable for data transfer (not power-only!).


  
