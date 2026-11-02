#include <TFT_eSPI.h>
#include <SD.h>
//#include <USBMSC.h>
#include <lvgl.h>
#include <lv_port_disp.h>
#include <lv_port_indev.h>
#include <string.h>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();

uint16_t calData[5] = {250, 3701, 197, 3677, 1};

#define MAX_MARKS 10
#define DEVICE_ID 0x01

uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;

extern const lv_img_dsc_t mark_enemy;
// extern const lv_img_dsc_t mark_ally;
// extern const lv_img_dsc_t mark_base_enemy;
// extern const lv_img_dsc_t mark_base_ally;
// extern const lv_img_dsc_t mark_objective;

static lv_point_t point_start;
static int32_t world_x = 401;   // центр карты по X (803/2)
static int32_t world_y = 433;   // центр карты по Y (866/2)
static int32_t zoom = 1; // используется для вычислений
static int32_t zoom_level = 1; // используется для вывода
static lv_point_t point;
static lv_point_t vect;

static uint8_t * canvas_buf = NULL;
static lv_obj_t * canvas = NULL;
static lv_obj_t * clock_lbl = NULL;
static lv_img_dsc_t map_dsc;
uint8_t * buf = NULL;

typedef enum{
  MARK_ENEMY = 0,
  MARK_ALLY = 1,
  MARK_BASE_ALLY = 2,
  MARK_BASE_ENEMY = 3,
  MARK_OBJECTIVE = 4
} MarkType;

typedef struct{
  int32_t world_x;
  int32_t world_y;
  int16_t id;
  int8_t type;
  int32_t create_time;
} Mark;

static Mark marks[MAX_MARKS];
static uint8_t marks_count = 0;
static uint16_t next_local_id = 1;

static lv_obj_t * menu_popup = NULL;   // контейнер меню
static int32_t pending_wx = 0;         // координаты карты для будущей метки
static int32_t pending_wy = 0;
static lv_point_t pending_screen;      // координаты экрана для позиции меню

// Функции
static void btn_event_cb(lv_event_t * e);
static void clock_timer_cb(lv_timer_t * timer);
static void drag_event_cb(lv_event_t * e);
static void long_press_cb(lv_event_t * e);
static void draw_map_to_canvas();
static void draw_task(void * pvParameters);
static void create_mark(int8_t type, int32_t w_x, int32_t w_y);
static void draw_icon(int sx, int sy, const lv_img_dsc_t * icon);
static const lv_img_dsc_t * get_icon_for_type(uint8_t type);
static void show_mark_menu(int sx, int sy);
static void hide_mark_menu();
static void menu_btn_cb(lv_event_t * e);

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println("Starting...");

  tft.init();
  tft.setSwapBytes(true);
  tft.setRotation(3);
  tft.setTouch(calData);
  tft.fillScreen(TFT_BLACK);  

  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), 255, 0);

  lv_obj_t * scr_layer = lv_obj_create(lv_scr_act());
  lv_obj_set_size(scr_layer, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_border_opa(scr_layer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(scr_layer, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(scr_layer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(scr_layer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(scr_layer, 0, 0);
  lv_obj_set_style_border_width(scr_layer, 0, 0);

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
  lv_obj_set_style_text_color(bat_sig_lbl, lv_color_hex(0xFFFFFF), 0);

  lv_obj_t * menu_btn = lv_btn_create(scr_layer);
  lv_obj_set_size(menu_btn, 40, 20);
  lv_obj_align(menu_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_border_opa(menu_btn, LV_OPA_TRANSP, 0);

  clock_lbl = lv_label_create(scr_layer);
  lv_label_set_text(clock_lbl, "00:00");
  lv_obj_align(clock_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_timer_create(clock_timer_cb, 1000, NULL);
  lv_obj_set_style_text_color(clock_lbl, lv_color_hex(0xFFFFFF), 0);

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

  canvas_buf = (uint8_t*)ps_malloc(480 * 320 * 2);
  if (!canvas_buf) {
    Serial.println("Canvas buffer alloc failed");
    return;
  }

  canvas = lv_canvas_create(scr_layer);
  lv_canvas_set_buffer(canvas, canvas_buf, 480, 320, LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_move_background(canvas);
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_pad_all(canvas, 0, 0);
  lv_obj_set_style_border_width(canvas, 0, 0);

  create_mark(MARK_ENEMY, 401, 433);
  draw_map_to_canvas();
  
  lv_obj_add_event_cb(zoomin_btn, btn_event_cb, LV_EVENT_CLICKED, (void*)"zoom_in");
  lv_obj_add_event_cb(zoomout_btn, btn_event_cb, LV_EVENT_CLICKED, (void*)"zoom_out");
  lv_obj_add_event_cb(menu_btn, btn_event_cb, LV_EVENT_CLICKED, (void*)"menu");
  lv_obj_add_event_cb(canvas, drag_event_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(canvas, drag_event_cb, LV_EVENT_PRESSING, NULL);
  lv_obj_add_event_cb(canvas, drag_event_cb, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(canvas, long_press_cb, LV_EVENT_LONG_PRESSED, NULL); 
}

void loop() {
  lv_timer_handler();
}

static void drag_event_cb(lv_event_t * e){
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t * indev = lv_indev_get_act();

  if (code == LV_EVENT_PRESSED){
    lv_indev_get_point(indev, &point);
    point_start = point;
  }

  if (code == LV_EVENT_PRESSING){
    lv_indev_get_point(indev, &vect);
    world_x -= (vect.x - point.x) / zoom;
    world_y -= (vect.y - point.y) / zoom;
    point = vect;

    draw_map_to_canvas();
    lv_obj_invalidate(canvas);
  }

  if (code == LV_EVENT_RELEASED){

  }
}

static void long_press_cb(lv_event_t * e){
  lv_indev_t * indev = lv_indev_get_act();
  lv_point_t p;
  lv_indev_get_point(indev, &p);

  int dx = abs(p.x - point_start.x);
  int dy = abs(p.y - point_start.y);
  if(dx > 10 || dy > 10){
    Serial.printf("Long press ignored");
    return;
  }

  int32_t wx = world_x + (p.x - 240) / zoom;
  int32_t wy = world_y + (p.y - 160) / zoom;

  Serial.printf("Long press at screen (%d,%d) -> map (%d,%d)\n", p.x, p.y, wx, wy);

  pending_wx = wx;
  pending_wy = wy;
  pending_screen = p;

  show_mark_menu(p.x, p.y);
  // create_mark(MARK_ENEMY, wx, wy);
  // draw_map_to_canvas();
  // lv_obj_invalidate(canvas);
}

static void draw_map_to_canvas() {
  uint16_t * canvas_px = (uint16_t*)canvas_buf;
  uint16_t * map_px = (uint16_t*)buf;

  float inv_zoom = 1.0f / zoom;
  float start_x = world_x + (0 - 240) / zoom;

  for (int sy = 0; sy < 320; sy++) {
    int32_t map_y = (int32_t)(world_y + (sy - 160) / zoom);

    if (map_y < 0 || map_y >= 866) {
      for (int sx = 0; sx < 480; sx++) {
        canvas_px[sy * 480 + sx] = 0;
      }
      continue;
    }

    uint16_t * map_row = &map_px[map_y * 803];   // ← вынести умножение
    float map_x_f = start_x;   // ← сброс для каждой строки
    uint16_t * canvas_row = &canvas_px[sy * 480];
    
    for (int sx = 0; sx < 480; sx++) {
      int32_t map_x = (int32_t)map_x_f;
      map_x_f += inv_zoom;
      
      if (map_x >= 0 && map_x < 803) {
        canvas_row[sx] = map_row[map_x];
      } else {
        canvas_row[sx] = 0;
      }
    }
  }

  for (int i = 0; i < marks_count; i++){
    Mark * m = &marks[i];

    int sx = (int)((m->world_x - world_x) * zoom + 240);
    int sy = (int)((m->world_y - world_y) * zoom + 160);
    
    sx -= 12; 
    sy -= 12;
    
    const lv_img_dsc_t * icon = get_icon_for_type(m->type);
    if (icon) draw_icon(sx, sy, icon);
  }
}

static void btn_event_cb(lv_event_t * e){
  lv_event_code_t code = lv_event_get_code(e);
  const char* id = (const char*)lv_event_get_user_data(e);

  if (code == LV_EVENT_CLICKED){
    if (strcmp(id, "zoom_in") == 0) zoom_level = min(zoom_level + 1, 2);
    if (strcmp(id, "zoom_out") == 0) zoom_level = max(zoom_level - 1, 1);
    zoom = zoom_level;
    Serial.printf("Zoom: %f\n", zoom_level);

    draw_map_to_canvas();
    lv_obj_invalidate(canvas);
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

static void create_mark(int8_t type, int32_t w_x, int32_t w_y){
  if (marks_count >= MAX_MARKS){
    Serial.printf("Marks full\n");
    return;
  }
  Mark m;
  m.world_x = w_x;
  m.world_y = w_y;
  m.type = type;
  m.id = (DEVICE_ID << 8) | (next_local_id++ & 0xFF);
  m.create_time = millis();
  marks[marks_count] = m;
  marks_count++;

  Serial.printf("Mark %d created\n", m.id);
}

static void draw_icon(int sx, int sy, const lv_img_dsc_t * icon){
    const uint8_t * src = icon->data;
    uint16_t * dst = (uint16_t*)canvas_buf;
    
    int w = icon->header.w;
    int h = icon->header.h;
    
    for (int iy = 0; iy < h; iy++) {
        for (int ix = 0; ix < w; ix++) {
            int px = sx + ix;
            int py = sy + iy;
            
            if (px < 0 || px >= 480 || py < 0 || py >= 320) continue;
            
            // 3 байта на пиксель: color_lo, color_hi, alpha
            const uint8_t * pixel = src + (iy * w + ix) * 3;
            uint16_t color = pixel[0] | (pixel[1] << 8);
            uint8_t alpha = pixel[2];
            
            if (alpha > 128) {
                dst[py * 480 + px] = color;
            }
        }
    }
}

static const lv_img_dsc_t * get_icon_for_type(uint8_t type) {
    switch (type) {
        case MARK_ENEMY: return &mark_enemy;
        // case MARK_ALLY: return &mark_ally;
        // case MARK_BASE_ALLY: return &mark_base_ally;
        // case MARK_BASE_ENEMY: return &mark_base_enemy;
        // case MARK_OBJECTIVE: return &mark_objective;
        default: return NULL;
    }
}

static void show_mark_menu(int sx, int sy){
  if (menu_popup){
    hide_mark_menu();
  }

  menu_popup = lv_obj_create(lv_scr_act());
  lv_obj_set_size(menu_popup, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_border_opa(menu_popup, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(menu_popup, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(menu_popup, 0, 0);
  lv_obj_add_event_cb(menu_popup, menu_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t * mark_btn[5];

  for(int i = 0; i < 5; i++){
    mark_btn[i] = lv_btn_create(menu_popup);
    lv_obj_add_event_cb(mark_btn[i], menu_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    lv_obj_set_style_radius(mark_btn[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(mark_btn[i], 60, 40);

    float angle = (i * 72.0f - 90.0f) * M_PI / 180.0f;
    int cx = sx + 60 * sin(angle);
    int cy = sy + 60 * sin(angle);

    lv_obj_set_pos(mark_btn[i], cx - 30, cy - 20);
    lv_obj_t * lbl = lv_label_create(mark_btn[i]);
    lv_label_set_text_fmt(lbl, "%d", i);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(mark_btn[i], menu_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
  }
}

static void hide_mark_menu(){
  if (menu_popup){
    lv_obj_del_async(menu_popup);
    menu_popup = NULL;
  }
}

static void menu_btn_cb(lv_event_t * e){
  uint8_t type = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  create_mark(type, pending_wx, pending_wy);
  draw_map_to_canvas();
  lv_obj_invalidate(canvas);
  hide_mark_menu();
}