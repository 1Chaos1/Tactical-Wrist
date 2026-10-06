#include <TFT_eSPI.h>
#include <SD.h>
#include <USBMSC.h>
#include <lvgl.h>
#include <lv_port_disp.h>

TFT_eSPI tft = TFT_eSPI();

uint16_t x, y;
uint16_t calData[5] = {223, 3571, 305, 3465, 7};

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println("Starting...");

  tft.init();
  tft.setSwapBytes(true);
  tft.setRotation(3);
  tft.setTouch(calData);
  tft.fillScreen(TFT_BLUE);  
  Serial.printf("tft.height() = %d\n", tft.height());

  lv_init();
  lv_port_disp_init();
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), 255, 0);
  if (!SD.begin(4)){
    Serial.println("SD mount failed");
  } else {
    Serial.println("SD mount success");
    Serial.printf("Card size: %llu MB", SD.cardSize() / (1024 * 1024));
  }
}

void loop() {
  lv_timer_handler();

  if (tft.getTouch(&x, &y)){
    Serial.printf("Touch: %d, %d\n", x, y);      
  }
}