// ttgo display
#include <TFT_eSPI.h>
#include <SPI.h>

// i2c
#include <Wire.h>

#include <Adafruit_BMP280.h>
#include <Adafruit_SCD30.h>



TFT_eSPI tft = TFT_eSPI();

Adafruit_BMP280 bmp; // I2C
Adafruit_SCD30  scd30; // I2C


void setup() {
  Serial.begin(115200);
  while ( !Serial ) delay(100);   // wait for native usb

  // display init
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_GREEN);
  tft.println("Init display... DONE");

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

  delay(2000);
}

void loop() {
  char ob[50];
  float temperature, pressure_sealevel, humidity, co2;

  temperature = bmp.readTemperature();
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

  sprintf(ob, "Temperature: %.1f *C", temperature);
  Serial.println(ob);
  tft.println(ob);

  sprintf(ob, "Pressure: %.1f hPa", pressure_sealevel);
  Serial.println(ob);
  tft.println(ob);

  sprintf(ob, "Humidity: %.1f %%", humidity);
  Serial.println(ob);
  tft.println(ob);

  sprintf(ob, "CO2 level: %.0f ppm", co2);
  Serial.println(ob);
  tft.println(ob);

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
