// ttgo display
#include <TFT_eSPI.h> 
#include <SPI.h>

// i2c
#include <Wire.h>

#include <Adafruit_BMP280.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_MCP9808.h>


#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <Preferences.h>

#include <HTTPClient.h>        // https://github.com/espressif/arduino-esp32/blob/master/libraries/HTTPClient/src/HTTPClient.h


TFT_eSPI tft = TFT_eSPI();

Adafruit_BMP280 bmp; // I2C
Adafruit_SCD30  scd30; // I2C
Adafruit_MCP9808 mcp9808; // I2C

WiFiManager wifiManager;
Preferences preferences;

String influxdb_url, influxdb_user, influxdb_pass;

// flag for saving data
bool should_save_config = false;

//callback notifying us of the need to save config
void save_config_callback() {
  Serial.println("Should save config");
  should_save_config = true;
}

void do_save_config() {
  tft.print("Saving prefs... ");
  preferences.begin("air", false);
  preferences.putString("influxdb_url", influxdb_url);
  preferences.putString("influxdb_user", influxdb_user);
  preferences.putString("influxdb_pass", influxdb_pass);
  preferences.end();
  tft.println("DONE");
}

int influx_send_data(char* data) {
  Serial.print("Sending to influx db server: ");
  Serial.println(influxdb_url);
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(5000); // 5 seconds
  http.begin(influxdb_url);
  http.addHeader("Content-Type", "application/octet-stream");
  http.setAuthorization(influxdb_user.c_str(), influxdb_pass.c_str());
  http.POST(data);
  Serial.println("post done");
  String response = http.getString();
  Serial.print("Server Response: ");
  Serial.println(response);
  http.end();
}


void setup() {
  pinMode(35, INPUT_PULLUP);
  
  Serial.begin(115200);
  while ( !Serial ) delay(100);   // wait for native usb

  char config_ap_psk[16];
  sprintf(config_ap_psk, "%i", random(1000000, 99999999));
  
  // display init
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_GREEN);
  tft.println("Init display... DONE");

  // init preferences
  tft.print("Loading prefs... ");
  preferences.begin("air", false);
  influxdb_url = preferences.getString("influxdb_url", "https://influxdb.host:8086/write?db=foobar");
  influxdb_user = preferences.getString("influxdb_user", "admin");
  influxdb_pass = preferences.getString("influxdb_pass", "admin");
  preferences.end();
  tft.println("DONE");

  wifiManager.setTimeout(120); // timeout until config portal will get turned off
  wifiManager.setSaveConfigCallback(save_config_callback);
  WiFiManagerParameter wmp_influxdb_url("influxdb_url", "InfluxDB Submit URL", influxdb_url.c_str(), 128);
  WiFiManagerParameter wmp_influxdb_user("influxdb_user", "InfluxDB User", influxdb_user.c_str(), 64);
  WiFiManagerParameter wmp_influxdb_pass("influxdb_pass", "InfluxDB Pass", "[redacted]", 64);
  wifiManager.addParameter(&wmp_influxdb_url);
  wifiManager.addParameter(&wmp_influxdb_user);
  wifiManager.addParameter(&wmp_influxdb_pass);
  if(!digitalRead(35)) {
    tft.print("Force config...");
    wifiManager.startConfigPortal("air_sensor_ttgo");
    tft.println("DONE");
  }

  Serial.printf("CFG AP PSK: %s\n", config_ap_psk);
  tft.printf("CFG AP PSK: %s\n", config_ap_psk);
  tft.print("Init WIFI... ");
  if(!wifiManager.autoConnect("air_sensor_ttgo autoAP", config_ap_psk)) {
    tft.println("Error");
  }
  tft.println("DONE");

  if(should_save_config) {
    influxdb_url = wmp_influxdb_url.getValue();
    influxdb_user = wmp_influxdb_user.getValue();
    if (wmp_influxdb_pass.getValue() != "[redacted]") {
      Serial.print("Saving value for influxdb_pass: '");
      Serial.print(wmp_influxdb_pass.getValue());
      Serial.println("'");
      influxdb_pass = wmp_influxdb_pass.getValue();
    }
    do_save_config();
  }

  // BMP280 init
  tft.print("Init BMP280... ");
  if(!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)) {
    Serial.println("Error finding BMP280");
    tft.println("Error");
  }
  else {
    /* Default settings from datasheet. */
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
    tft.println("DONE");
  }


  // SCD30 init
  tft.print("Init SDC30... ");
  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30 chip");
    tft.println("Error");
  }
  scd30.setMeasurementInterval(2);
  //scd30.setAltitudeOffset(299); // we are 299m over sea level
  scd30.setTemperatureOffset(80); // 0.8*C x 100
  scd30.startContinuousMeasurement(bmp.readPressure()/100);
  tft.println("DONE");

  // MCP9808 init
  tft.print("Init MCP9808... ");
  if (!mcp9808.begin()) {
    Serial.println("Failed to find MCP9808 chip");
    tft.println("Error");
  }
  else {
    mcp9808.setResolution(3);
    // Mode Resolution SampleTime
    //  0    0.5°C       30 ms
    //  1    0.25°C      65 ms
    //  2    0.125°C     130 ms
    //  3    0.0625°C    250 ms
    mcp9808.wake();
    tft.println("DONE");
  }
  
  delay(2000);
}

void loop() {
  static uint8_t counter = 0;
  char ob[128];
  float temperature, pressure_sealevel, humidity, co2;

  //temperature = bmp.readTemperature();
  //mcp9808.wake();
  temperature = mcp9808.readTempC();
  //mcp9808.shutdown();
  pressure_sealevel = bmp.readPressure()/100 + 38; // nuernberg is 38mBar above sea level

  if (scd30.dataReady()) {  
    if (!scd30.read()){
      Serial.println("Error reading sensor data");
      return;
    }
    humidity = scd30.relative_humidity;
    co2 = scd30.CO2;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 50);
  //tft.println("Hello world! ");
  tft.print("IP: ");
  tft.println(WiFi.localIP());
  tft.printf("RSSI: %d dBm\n", WiFi.RSSI());
  tft.println("\n");
  
  sprintf(ob, "Temperature: %.1f *C", temperature);
  Serial.println(ob);
  tft.println(ob);
  tft.println("");

  sprintf(ob, "Pressure: %.1f hPa", pressure_sealevel);
  Serial.println(ob);
  tft.println(ob);
  tft.println("");

  sprintf(ob, "Humidity: %.1f %%", humidity);
  Serial.println(ob);
  tft.println(ob);
  tft.println("");

  sprintf(ob, "CO2 level: %.0f ppm", co2);
  Serial.println(ob);
  tft.println(ob);

  // reconnect WiFi.localIP()
  if (WiFi.status() != WL_CONNECTED) {
    tft.println("\n\n\nWIFI DISCONNECT\n\n-> Resetting...");
    Serial.println("WIFI DISCONNECT --> Resetting...");
    delay(4000);
    // reset on wifi disconnect
    ESP.restart();
  }

  Serial.print("Counter: ");
  Serial.println(counter);
  // send data to influxdb
  if (counter == 5) {
    if (humidity > 0 && co2 > 100 && co2 < 1e6) {
      // only send values that make sense
      sprintf(ob, "air temperature=%.1f,pressure=%.1f,humidity=%.1f,co2=%.0f\n", temperature, pressure_sealevel, humidity, co2);
      influx_send_data(ob);
    }
    else {
      Serial.println("Not sending bad values");
    }
  }
  else if (counter > 30) {
    counter = 0;
  }
  counter += 1;

  
  Serial.println("");

  delay(2000);
  // pinMode(4, OUTPUT);
  // digitalWrite(4, LOW); switch off backlight
}



    /*
    Serial.print("Ambient pressure offset: ");
    Serial.print(scd30.getAmbientPressureOffset());
    Serial.println(" mBar");
  
    Serial.print("Altitude offset: ");
    Serial.print(scd30.getAltitudeOffset());
    Serial.println(" meters");
  
    Serial.print("Temperature offset: ");
    Serial.print((float)scd30.getTemperatureOffset()/100.0);
    Serial.println(" degrees C");
  
    Serial.print("Forced Recalibration reference: ");
    Serial.print(scd30.getForcedCalibrationReference());
    Serial.println(" ppm");
  
    if (scd30.selfCalibrationEnabled()) {
      Serial.println("Self calibration enabled");
    } else {
      Serial.println("Self calibration disabled");
    }
    */
