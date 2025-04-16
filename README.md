> [!WARNING]
> Moved to https://codeberg.org/asdil12/air_sensor_ttgo

Open `~/Arduino/libraries/TFT_eSPI/User_Setup_Select.h`

Comment out:
#include <User_Setup.h>

Activate:
#include <User_Setups/Setup25_TTGO_T_Display.h>    // Setup file for ESP32 and TTGO T-Display ST7789V SPI bus TFT
