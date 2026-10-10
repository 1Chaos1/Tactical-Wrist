#include <TFT_eSPI.h>
#include <SD.h>
//#include <USBMSC.h>
#include <lvgl.h>
#include <lv_port_disp.h>
#include <lv_port_indev.h>
#include <string.h>

TFT_eSPI tft = TFT_eSPI();

uint16_t x, y;
uint16_t calData[5] = {223, 3571, 305, 3465, 7};

uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;

static lv_obj_t * clock_lbl = NULL;
static lv_obj_t * map_img = NULL;
static int zoom_level = 2;

static lv_img_dsc_t map_dsc;
uint8_t * buf = NULL;

static void btn_event_cb(lv_event_t * e);
static void clock_timer_cb(lv_timer_t * timer);

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println("Starting...");

  tft.init();
  tft.setSwapBytes(true);
  tft.setRotation(3);
  tft.setTouch(calData);
  tft.fillScreen(TFT_BLACK);  
  Serial.printf("tft.height() = %d\n", tft.height());

  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), 255, 0);

  lv_obj_t * scr_layer = lv_obj_create(lv_scr_act());
  lv_obj_set_size(scr_layer, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_border_opa(scr_layer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(scr_layer, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(scr_layer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(scr_layer, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * zoomin_btn = lv_btn_create(scr_layer);
  lv_obj_set_size(zoomin_btn, 40, 20);
  lv_obj_align(zoomin_btn, LV_ALIGN_BOTTOM_RIGHT, -45, 0);
  lv_obj_set_style_border_opa(zoomin_btn, LV_OPA_TRANSP, 0);

  lv_obj_t * zoomin_lbl = lv_label_create(zoomin_btn);
  lv_label_set_text(zoomin_lbl, "+");
  lv_obj_center(zoomin_lbl);

  lv_obj_t * zoomout_btn = lv_btn_create(scr_layer);
  lv_obj_set_size(zoomout_btn, 40, 20);
  lv_obj_align(zoomout_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  lv_obj_t * zoomout_lbl = lv_label_create(zoomout_btn);
  lv_label_set_text(zoomout_lbl, "-");
  lv_obj_center(zoomout_lbl);

  lv_obj_t * bat_sig_lbl = lv_label_create(scr_layer);
  lv_label_set_text(bat_sig_lbl, "BTR: 100% | SGN: 95%");
  lv_obj_align(bat_sig_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t * menu_btn = lv_btn_create(scr_layer);
  lv_obj_set_size(menu_btn, 40, 20);
  lv_obj_align(menu_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_border_opa(menu_btn, LV_OPA_TRANSP, 0);

  clock_lbl = lv_label_create(scr_layer);
  lv_label_set_text(clock_lbl, "00:00");
  lv_obj_align(clock_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_timer_create(clock_timer_cb, 1000, NULL);

  lv_obj_add_event_cb(zoomin_btn, btn_event_cb, LV_EVENT_CLICKED, (void*)"zoom_in");
  lv_obj_add_event_cb(zoomout_btn, btn_event_cb, LV_EVENT_CLICKED, (void*)"zoom_out");
  lv_obj_add_event_cb(menu_btn, btn_event_cb, LV_EVENT_CLICKED, (void*)"menu");

  SD.begin(4);
  File f = SD.open("/maps/poligon1.bin");
  if (!f) { Serial.println("File not found"); return; }

  size_t size = f.size();
  buf = (uint8_t*)ps_malloc(size);
  if (!buf) { Serial.println("PSRAM alloc failed"); return; }

  f.read(buf, size);
  f.close();
  Serial.printf("Loaded %d bytes\n", size);

  map_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  map_dsc.header.w = 803;
  map_dsc.header.h = 866;
  map_dsc.data_size = size;
  map_dsc.data = buf;
  map_dsc.header.always_zero = 0;
  map_dsc.header.reserved = 0;

  map_img = lv_img_create(scr_layer);
  lv_img_set_src(map_img, &map_dsc);
  lv_obj_align(map_img, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_move_background(map_img);
  lv_obj_set_size(map_img, 320, 240);
  lv_obj_set_style_clip_corner(map_img, true, 0);
  
  Serial.printf("PSRAM: %d bytes free\n", ESP.getFreePsram());
}

void loop() {
  lv_timer_handler();
}

static void btn_event_cb(lv_event_t * e){
  lv_event_code_t code = lv_event_get_code(e);
  const char* id = (const char*)lv_event_get_user_data(e);
  if (code == LV_EVENT_CLICKED){
    if (strcmp(id, "zoom_in") == 0) {
      zoom_level = min(zoom_level + 1, 5);
    }
    if (strcmp(id, "zoom_out") == 0){
      zoom_level = max(zoom_level - 1, 1);
    }
    Serial.printf("Zoom Level = %d\n", zoom_level);
    uint16_t zoom_factor = 128 + (zoom_level - 1) * 128;
    lv_img_set_zoom(map_img, zoom_factor);
    lv_img_set_offset_x(map_img, 0);
    lv_img_set_offset_y(map_img, 0);
    lv_obj_invalidate(lv_scr_act());

    if (strcmp(id, "menu") == 0) Serial.println("Menu clicked");
  }
}

static void clock_timer_cb(lv_timer_t * timer){
  char buf[32];
  seconds++;
  if (seconds >= 60){
    seconds = 0;
    minutes++;
    if (minutes >= 60){
      minutes = 0;
      hours++;
      if (hours >= 24){
        hours = 0;
      }
    }
  }
  snprintf(buf, sizeof(buf), "%02d:%02d", hours, minutes);
  lv_label_set_text(clock_lbl, buf);
}