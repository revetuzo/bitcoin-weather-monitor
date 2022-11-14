# bitcoin-weather-monitor

The Bitcoin weather monitor shows Bitcoin and weather data on a small TFT display and is powered by an ESP8266 NodeMCU board.

![presentation](https://user-images.githubusercontent.com/104918401/201718622-333a17c6-e7ff-45ab-a54f-af61701729f0.jpg)

![Bitcoin-weather-monitor](https://user-images.githubusercontent.com/104918401/201689472-073c5be6-d85a-43f0-b94d-746bc6d09680.png)

Shopping list:
- NodeMCU v3.4 ESP8266 board (31.2 mm x 58 mm)
- 1.77" TFT SPI display (128 x 160) with ST7735 driver (other sizes with the same resolution and driver may work too)
- GY-21 breakout board for the HTU21 or SHT21 temprature and humidity sensor
- cables with female/female Dupont connectors, approx. 10cm length
- some M2.5x8 and short M3 screws
- a 10 Ohm resistor and some heat-shrink tube (optional) (+ a soldering iron)

Instructions:
- print the enclosure on your 3D printer (four parts: enclosure, foot, backplate, sensor rack)
- solder the header pins to the PCBs if they do not come preassembled
- assemble the electronics as shown in the Fritzing drawing (10 Ohm resistor is optional, a simple wire is fine as well)
  - the drawing shows a BMP180 sensor, but the SHT21/HTU21 has the same pinout
- mount the electronics in the enclosure with the M2.5 and M3 screws (you need a long screwdriver and maybe a pair of tweezers)
- assemble the enclosure and the sensor rack
- get an Openweathermap free API key
- find your geographical location on OpenStreetMap
- run your own Bitcoin node (optional, but highly recommended anyway!)
- install Arduino IDE 1.8.x
- install ESP8266 core in Arduino IDE
- install libraries in Arduino IDE:
  - ArduinoHttpClient
  - ArduinoJson
  - Adafruit_GFX
  - Adafruit_ST7735
  - Sodaq_SHT2x
- find and follow instructions on how to connect your NodeMCU board with Arduino IDE via USB
- modify the .ino sketch file:
  - #define MY_TZ, this string defines your timezone and DST rules (set to GMT/London by default)
  - point char mempoolserver[] at your own node or just leave it at the default mempool.space address
  - char weatherAPIString: here you have to place your latitude, longitude, and Openweathermap API key
  - enter your WiFi SSID and password in arduino_secrets.h
  - (optional) you can change the date format in the tftClockScreen() function (default is European / German style)
  - (optional) you can change the temperature units in the tftTempWeatherScreen() function
  - (optional) change the data update intverval (5 min) and time per screen (5 s) with int dataUpdateInterval and int timePerScreen in the sketch header
- compile the sketch and upload to your NodeMCU board
- use the Serial Monitor during first startup to check that all data are correctly retrieved
  
Notes:
- The holes for the screws can be loose or tight, depending on how well your printer profile is tuned. To tune the hole sizes, you may play with the flow settings, or use the horizontal hole expansion in Cura 5.x. The hole diameters work fine on my Ender 3 V2 with direct drive mod and a 0.3mm nozzle.
- NTP is included in the latest ESP8266 core library and does not require additional libraries. Sync is automatically performed whenever we turn on WiFi.
- See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for timezone strings.
- We put the ESP8266 into sleep mode as much as possible. WiFi is only turned on during data update. Otherwise, the module is put into "light sleep" mode. This requires some code tweaking, but reduces the current draw by approximately 8 mA and reduces the heat load on the temperature sensor. See the function lightSleep() for the annoying details.
- The 10 Ohm resistor for the TFT LED is optional. It reduces the screen brightness a bit and reduces the current draw by approximately 15 mA (roughly 30%).
- The sensor has to be placed outside the enclosure, because the device has a constant power consumption of 160 mW, which heats the sensor too much for reliable measurements.
- If you have problems connecting the NodeMCU board, the cable might be an issue. Make sure you have a high-quality cable suitable for data transfer (not power-only!).
- Once we reach seven-digit block height, the font size will have to be adjusted. (Too lazy for automation).
- The TX fees are the "immediate" (red), "10 min" (orange), and "30 min" (green) TX fees estimated by mempool.
- The smaller NodeMCU v3.2 board will also work. But it will need a different backplate, will have to adapt the design later.
- You can remove parts of the code quite easily: E.g. you don't want the temperature and weather data, just remove the related function calls in setup() and loop(). Additionally, you can remove the associated calls and variable declarations in the header and delete the associated functions in the .ino sketch altogether.

To Do:
- The data types for the various variables are not optimized yet.
- Better error handling.
- Make an enclosure that fits a battery pack?
- Come up with a better idea for the temperature sensor placement?
- Use a separate configuration file for user-configurable parameters.
- ...


  
