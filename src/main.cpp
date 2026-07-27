#include <TFT_eSPI.h>
#include <lvgl.h>

TFT_eSPI tft = TFT_eSPI();

// LVGL draw buffer
static lv_color_t buf[SCREEN_WIDTH * 40]; // 40 lines buffer

lv_display_t *disp;

// LVGL display flush callback
void my_disp_flush(lv_display_t *disp,
                   const lv_area_t *area,
                   uint8_t *px_map)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

// LVGL touch callback
void my_touch_read(lv_indev_t *indev,
                   lv_indev_data_t *data)
{
    uint16_t x, y;

    if (tft.getTouch(&x, &y))
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void setup()
{
    Serial.begin(115200);

    // TFT setup
    tft.begin();
    tft.setRotation(1);

    // LVGL setup
    lv_init();

    // Create LVGL display
    disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Set flush callback
    lv_display_set_flush_cb(disp, my_disp_flush);

    // Give LVGL the drawing buffer
    lv_display_set_buffers(
        disp,
        buf,
        NULL,
        sizeof(buf),
        LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Create a label
    lv_obj_t *label = lv_label_create(lv_screen_active());

    lv_label_set_text(label, "Hello, world!");

    lv_obj_set_style_text_font(
        label,
        &lv_font_montserrat_32,
        0);

    lv_obj_center(label);

    // Create touch input device
    lv_indev_t *indev = lv_indev_create();

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);

    lv_indev_set_read_cb(indev, my_touch_read);

    // Create a button
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 150, 70);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 60);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Press me");
    lv_obj_center(btn_label);
}

void loop()
{
    lv_timer_handler(); // Let LVGL update
    delay(5);
}