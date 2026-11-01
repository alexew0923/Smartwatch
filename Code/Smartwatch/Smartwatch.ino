//todo: create uniform style across different widgets, UI update
//auto display on/off and auto brightness adjustment << play with resistor to see which controls the backlight

//icons from Phosphor Icons
//https://notisrac.github.io/FileToCArray/ to convert icons to C array
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "RTClib.h"
#include "Adafruit_SHT4x.h"
#include "Adafruit_BMP5xx.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "stopwatch.h"
#include "timer.h"
#include "temperature.h"
#include "gyroscope.h"
#include "pressure.h"

//Custom Icons
LV_IMG_DECLARE(STOPWATCH_ICON_INFO);
LV_IMG_DECLARE(TIMER_ICON_INFO);
LV_IMG_DECLARE(TEMP_ICON_INFO);
LV_IMG_DECLARE(GYRO_ICON_INFO);
LV_IMG_DECLARE(PRESSURE_ICON_INFO);

//SHT40
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
sensors_event_t humidity, temp;

//MPU6050
Adafruit_MPU6050 mpu;

//BMP681
const float SEALEVELPRESSURE_HPA = 1013.25;
Adafruit_BMP5xx bmp;

//DS3231
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

const int appNum = 5;
int page = 0;
int app = -1;
bool gyroMode = false;
int avgALS[10] = { 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000 };
int alsIndex = 0;

/*Set to your screen resolution and rotation*/
#define TFT_HOR_RES 240
#define TFT_VER_RES 240
#define TFT_ROTATION LV_DISPLAY_ROTATION_0

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

void my_print(lv_log_level_t level, const char *buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

/* LVGL calls it when a rendered image needs to copied to the display*/
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  /*Copy `px map` to the `area`*/

  /*For example ("my_..." functions needs to be implemented by you)
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  my_set_window(area->x1, area->y1, w, h);
  my_draw_bitmaps(px_map, w * h);
    */

  /*Call it to tell LVGL you are ready*/
  lv_display_flush_ready(disp);
}

/*use Arduinos millis() as tick source*/
static uint32_t my_tick(void) {
  return millis();
}

void setup() {
  //Button Pins
  pinMode(2, INPUT);
  pinMode(9, INPUT);
  ledcSetup(5, 131, 12);
  ledcAttachPin(4, 5);

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  Serial.begin(115200);
  Serial.println(LVGL_Arduino);

  Wire.begin();  //initialize i2c bus
  Wire.beginTransmission(0x29);
  byte command[3] = { 0x00, 0b10000000, 0b00011000 };  //initial setting for VEML6035
  Wire.write(command, 3);
  Wire.endTransmission();

  if (!sht4.begin()) {
    Serial.println("Couldn't find SHT4x");
    while (1) delay(1);
  }
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  bmp.begin(0x47, &Wire);
  bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_2X);
  bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_16X);
  bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP5XX_ODR_01_HZ);
  bmp.setPowerMode(BMP5XX_POWERMODE_FORCED);
  bmp.enablePressure(true);

  Wire.begin(6, 7);
  mpu.begin(0x69);
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    //rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  lv_init();

  /*Set a tick source so that LVGL will know how much time elapsed. */
  lv_tick_set_cb(my_tick);

  lv_log_register_print_cb(my_print);

  lv_display_t *disp;
  /*TFT_eSPI can be enabled lv_conf.h to initialize the display in a simple way*/
  disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, TFT_ROTATION);


  /* Create a simple label
    * ---------------------
    lv_obj_t *label = lv_label_create( lv_screen_active() );
    lv_label_set_text( label, "Hello Arduino, I'm LVGL!" );
    lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

    * Try an example. See all the examples
    *  - Online: https://docs.lvgl.io/master/examples.html
    *  - Source codes: https://github.com/lvgl/lvgl/tree/master/examples
    * ----------------------------------------------------------------

    lv_example_btn_1();

    * Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMO_WIDGETS
    * -------------------------------------------------------------------------------------------

    lv_demo_widgets();
    */
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xffffff), LV_PART_MAIN);
  widgetSetup();
  pageChange();
  setupHomepage();

  Serial.println("Setup done");
}

/*  When creating a new app, take the following steps
    1. Create new widgets if necessary
      1.1 Set up the initial setting of the widget in widgetSetup() 
      1.2 Edit pageChange() and appChange(), so that the new widget can be hidden accordingly
    2. Edit pageChange() and appChange() to decide what to show and hide
    3. Create a new page setup function to change the widgets' position, initial value, etc...
*/
//LVGL Widgets
lv_obj_t *label_time;
lv_obj_t *label_name;
lv_obj_t *label_date;
lv_obj_t *panel_name;
lv_obj_t *image_icon;
lv_obj_t *chart_temp;
lv_chart_series_t *ser_temp;
lv_chart_series_t *ser_hum;
lv_obj_t *scale_temp;
lv_obj_t *scale_hum;
lv_obj_t *gyro_line;
lv_point_precise_t gyro_line_points[6];
lv_obj_t *gauge_pressure;
lv_obj_t *line_pressure;
lv_obj_t *circle;
lv_obj_t *scale_battery;
lv_scale_section_t *battery_status;
static lv_style_t battery_style;

void widgetSetup() {
  //chart: temp
  chart_temp = lv_chart_create(lv_screen_active());
  lv_obj_center(chart_temp);
  lv_chart_set_type(chart_temp, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(chart_temp, LV_CHART_UPDATE_MODE_SHIFT);
  lv_obj_set_size(chart_temp, 240, 240);
  lv_chart_set_div_line_count(chart_temp, 6, 6);
  lv_obj_set_style_border_opa(chart_temp, LV_OPA_0, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(chart_temp, 30, LV_PART_MAIN);
  ser_temp = lv_chart_add_series(chart_temp, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
  ser_hum = lv_chart_add_series(chart_temp, lv_color_hex(0x0000FF), LV_CHART_AXIS_SECONDARY_Y);

  //label: time
  label_time = lv_label_create(lv_screen_active());
  lv_obj_set_align(label_time, LV_ALIGN_CENTER);
  lv_obj_set_style_text_align(label_time, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_line_space(label_time, 5, LV_PART_MAIN);

  //label: date
  label_date = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(label_date, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(label_date, LV_ALIGN_CENTER, 0, -80);
  lv_obj_set_style_text_align(label_date, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  //panel: name
  panel_name = lv_obj_create(lv_screen_active());
  lv_obj_set_style_bg_color(panel_name, lv_color_hex(0x3f51b5), LV_PART_MAIN);
  lv_obj_set_style_border_opa(panel_name, LV_OPA_0, LV_PART_MAIN);
  lv_obj_set_style_radius(panel_name, 20, LV_PART_MAIN);
  lv_obj_set_align(panel_name, LV_ALIGN_CENTER);
  lv_obj_set_y(panel_name, 70);

  //circle
  circle = lv_obj_create(lv_screen_active());
  lv_obj_set_style_bg_color(circle, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_border_opa(circle, LV_OPA_0, LV_PART_MAIN);
  lv_obj_set_style_radius(circle, 10, LV_PART_MAIN);
  lv_obj_set_align(circle, LV_ALIGN_CENTER);
  lv_obj_set_size(circle, 10, 10);

  //label: name
  label_name = lv_label_create(lv_screen_active());
  lv_obj_align(label_name, LV_ALIGN_CENTER, 0, 70);
  lv_obj_set_style_text_align(label_name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  //image: icon
  image_icon = lv_image_create(lv_screen_active());
  lv_obj_align(image_icon, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_img_recolor(image_icon, lv_color_hex(0xffffff), LV_PART_MAIN);
  lv_obj_set_style_img_recolor_opa(image_icon, LV_OPA_TRANSP, LV_PART_MAIN);

  //scale: temp
  scale_temp = lv_scale_create(lv_screen_active());
  lv_scale_set_mode(scale_temp, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_label_show(scale_temp, true);
  lv_scale_set_total_tick_count(scale_temp, 13);
  lv_scale_set_major_tick_every(scale_temp, 6);
  lv_obj_set_style_length(scale_temp, 5, LV_PART_ITEMS);
  lv_obj_set_style_length(scale_temp, 10, LV_PART_INDICATOR);

  lv_scale_set_angle_range(scale_temp, 160);
  lv_scale_set_rotation(scale_temp, -80);
  lv_obj_set_size(scale_temp, 240, 240);
  lv_obj_set_align(scale_temp, LV_ALIGN_CENTER);

  //scale: hum
  scale_hum = lv_scale_create(lv_screen_active());
  lv_scale_set_mode(scale_hum, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_label_show(scale_hum, true);
  lv_scale_set_total_tick_count(scale_hum, 13);
  lv_scale_set_major_tick_every(scale_hum, 6);
  lv_obj_set_style_length(scale_hum, 5, LV_PART_ITEMS);
  lv_obj_set_style_length(scale_hum, 10, LV_PART_INDICATOR);

  lv_scale_set_angle_range(scale_hum, 160);
  lv_scale_set_rotation(scale_hum, 100);
  lv_obj_set_size(scale_hum, 240, 240);
  lv_obj_set_align(scale_hum, LV_ALIGN_CENTER);

  //scale: battery
  scale_battery = lv_scale_create(lv_screen_active());
  lv_scale_set_mode(scale_battery, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_label_show(scale_battery, true);
  lv_scale_set_total_tick_count(scale_battery, 3);
  lv_scale_set_major_tick_every(scale_battery, 1);
  lv_obj_set_style_length(scale_battery, 5, LV_PART_ITEMS);
  lv_obj_set_style_length(scale_battery, 10, LV_PART_INDICATOR);
  lv_scale_set_range(scale_battery, 100, 0);

  lv_scale_set_angle_range(scale_battery, 50);
  lv_scale_set_rotation(scale_battery, -25);
  lv_obj_set_size(scale_battery, 240, 240);
  lv_obj_set_align(scale_battery, LV_ALIGN_CENTER);

  battery_status = lv_scale_add_section(scale_battery);
  lv_style_init(&battery_style);
  lv_style_set_arc_width(&battery_style, 13);
  lv_scale_set_section_style_main(scale_battery, battery_status, &battery_style);

  //line: gyro
  gyro_line = lv_line_create(lv_screen_active());
  lv_obj_set_size(gyro_line, 240, 240);
  lv_obj_set_style_line_width(gyro_line, 2, LV_PART_MAIN);

  //gauge: pressure
  gauge_pressure = lv_scale_create(lv_screen_active());
  lv_scale_set_mode(gauge_pressure, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_total_tick_count(gauge_pressure, 20);
  lv_scale_set_major_tick_every(gauge_pressure, 4);
  lv_scale_set_label_show(gauge_pressure, true);
  lv_scale_set_range(gauge_pressure, 950, 1050);
  lv_scale_set_angle_range(gauge_pressure, 260);
  lv_scale_set_rotation(gauge_pressure, 140);
  lv_obj_set_size(gauge_pressure, 240, 240);
  lv_obj_set_align(gauge_pressure, LV_ALIGN_CENTER);

  //line: pressure
  line_pressure = lv_line_create(lv_screen_active());
  lv_obj_set_style_line_width(line_pressure, 5, LV_PART_MAIN);
  lv_obj_set_style_line_rounded(line_pressure, true, LV_PART_MAIN);
}


void pageChange() {  //other than page 0, all other pages have the same format: icon, panel and a label
  lv_obj_add_flag(chart_temp, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(scale_temp, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(scale_hum, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(gyro_line, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(gauge_pressure, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(line_pressure, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(circle, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(label_date, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(image_icon, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(scale_battery, LV_OBJ_FLAG_HIDDEN);

  lv_obj_remove_flag(panel_name, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(label_name, LV_OBJ_FLAG_HIDDEN);
  if (page == 0) { //time
    setupHomepage();
    lv_obj_remove_flag(label_time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(label_date, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(scale_battery, LV_OBJ_FLAG_HIDDEN);
  } else {
    setupPage();
    lv_obj_remove_flag(image_icon, LV_OBJ_FLAG_HIDDEN);
  }
}

void setupPage() {
  lv_obj_align(label_name, LV_ALIGN_CENTER, 0, 70);
  lv_obj_set_style_text_align(label_name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  if (page == 1) {
    //label: name
    lv_label_set_text(label_name, "Stopwatch");
    //panel: name
    lv_obj_set_size(panel_name, 95, 35);
    //image: icon
    lv_img_set_src(image_icon, &STOPWATCH_ICON_INFO);
  } else if (page == 2) {
    lv_label_set_text(label_name, "Timer");
    lv_obj_set_size(panel_name, 60, 35);
    lv_img_set_src(image_icon, &TIMER_ICON_INFO);
  } else if (page == 3) {
    lv_obj_set_size(panel_name, 115, 35);
    lv_label_set_text(label_name, "Temp & Hum");
    lv_img_set_src(image_icon, &TEMP_ICON_INFO);
  } else if (page == 4) {
    lv_obj_set_size(panel_name, 115, 35);
    lv_label_set_text(label_name, "Acc & Gyro");
    lv_img_set_src(image_icon, &GYRO_ICON_INFO);
  } else if (page == 5) {
    lv_obj_set_size(panel_name, 130, 35);
    lv_label_set_text(label_name, "Air Pressure");
    lv_img_set_src(image_icon, &PRESSURE_ICON_INFO);
  }
}

void appChange() {
  lv_obj_add_flag(panel_name, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(label_name, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(label_date, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(image_icon, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(scale_battery, LV_OBJ_FLAG_HIDDEN);

  if (app == 0) {  //time
    setupHomeApp();
    lv_obj_remove_flag(label_time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(label_name, LV_OBJ_FLAG_HIDDEN);
  } else if (app == 1) {  //stopwatch
    setupStopwatchApp();
    lv_obj_remove_flag(label_time, LV_OBJ_FLAG_HIDDEN);
  } else if (app == 2) {  //timer
    setupTimerApp();
    lv_obj_remove_flag(label_time, LV_OBJ_FLAG_HIDDEN);
  } else if (app == 3) {  //temperature
    setupTempApp();
    lv_obj_remove_flag(label_time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(chart_temp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(scale_temp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(scale_hum, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(label_name, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(label_date, LV_OBJ_FLAG_HIDDEN);
  } else if (app == 4) {  //accelerometer & gyroscope
    setupGyroApp();
    lv_obj_remove_flag(label_time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(gyro_line, LV_OBJ_FLAG_HIDDEN);
  } else if (app == 5) { //barometer
    setupPressureApp();
    lv_obj_remove_flag(label_time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(gauge_pressure, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(line_pressure, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(circle, LV_OBJ_FLAG_HIDDEN);
  }
}


void setupHomepage() {
  //label: time
  lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, LV_PART_MAIN); /*Set a larger font*/
  lv_obj_set_style_text_letter_space(label_time, 5, LV_PART_MAIN);
  lv_obj_set_y(label_time, -10);
  updateTime(0);

  //label: name
  lv_obj_align(label_name, LV_ALIGN_CENTER, 0, 70);
  lv_obj_set_style_text_align(label_name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(label_name, "Welcome, Eunwoo");

  //label: date
  lv_obj_align(label_date, LV_ALIGN_CENTER, 0, -85);
  lv_obj_set_style_text_align(label_date, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(label_date, &lv_font_montserrat_20, LV_PART_MAIN);

  //panel: name
  lv_obj_set_size(panel_name, 150, 35);
}

void setupHomeApp() {
  //label: name
  lv_label_set_text(label_name, "Please select new time");
}

void setupStopwatchApp() {
  //label: time
  lv_obj_set_style_text_font(label_time, &lv_font_montserrat_44, LV_PART_MAIN); /*Set a larger font*/
  lv_obj_set_style_text_letter_space(label_time, 0, LV_PART_MAIN);
  lv_label_set_text(label_time, "00:00:00");
  lv_obj_set_y(label_time, 0);
}

void setupTimerApp() {
  //label: time
  lv_obj_set_style_text_font(label_time, &lv_font_montserrat_44, LV_PART_MAIN); /*Set a larger font*/
  lv_obj_set_style_text_letter_space(label_time, 0, LV_PART_MAIN);
  lv_label_set_text(label_time, "00:00:00");
  lv_obj_set_y(label_time, 0);
}

void setupTempApp() {
  //label: time
  lv_obj_set_style_text_font(label_time, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_letter_space(label_time, 0, LV_PART_MAIN);
  lv_obj_set_y(label_time, 50);
  
  //label: name
  lv_obj_align(label_name, LV_ALIGN_CENTER, 80, 0);
  lv_obj_set_style_text_align(label_name, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  //label: date
  lv_obj_align(label_date, LV_ALIGN_CENTER, -80, 0);
  lv_obj_set_style_text_align(label_date, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, LV_PART_MAIN);

  updateAxis(ser_temp, true);
  updateAxis(ser_hum, false);
}

void setupGyroApp() {
  //label: time
  lv_obj_set_y(label_time, 0);
  lv_obj_set_style_text_letter_space(label_time, 0, LV_PART_MAIN);
  lv_obj_set_style_text_font(label_time, &lv_font_montserrat_20, LV_PART_MAIN);
}

void setupPressureApp() {
  //label: time
  lv_obj_set_style_text_font(label_time, &lv_font_montserrat_20, LV_PART_MAIN); /*Set a larger font*/
  lv_obj_set_style_text_letter_space(label_time, 0, LV_PART_MAIN);
  lv_obj_set_y(label_time, 40);

  updatePressureApp();
}

DateTime now;
void updateTime(int minute) {
  // Get the current time from the RTC
  now = rtc.now();

  if (minute != 0) {
    int newMinute = now.minute() + minute;
    if (newMinute < 0) {
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour() - 1, 59, 0));
    } else if (newMinute > 59) {
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour() + 1, 0, 0));
    } else {
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), newMinute, 0));
    }
  }

  // Getting each time field in individual variables
  // And adding a leading zero when needed;
  String yearStr = String(now.year(), DEC);
  String monthStr = (now.month() < 10 ? "0" : "") + String(now.month(), DEC);
  String dayStr = (now.day() < 10 ? "0" : "") + String(now.day(), DEC);
  String hourStr = (now.hour() < 10 ? "0" : "") + String(now.hour(), DEC);
  String minuteStr = (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC);
  String secondStr = (now.second() < 10 ? "0" : "") + String(now.second(), DEC);
  String dayOfWeek = daysOfTheWeek[now.dayOfTheWeek()];

  // Complete time string
  String formattedTime = dayOfWeek + ", " + yearStr + "-" + monthStr + "-" + dayStr + " " + hourStr + ":" + minuteStr + ":" + secondStr;

  // Print the complete formatted time
  Serial.println(formattedTime);

  lv_label_set_text_fmt(label_time, "%02u\n%02u", now.hour(), now.minute());
  lv_label_set_text_fmt(label_date, "%u-%02u-%02u", now.year(), now.month(), now.day());
}


bool stopwatchOn = false;
long stopwatchTime;
void updateStopwatch() {
  if (stopwatchTime == 0) {
    lv_label_set_text(label_time, "00:00:00");
  } else {
    unsigned long tempTime = millis() - stopwatchTime;
    uint8_t milliSecond = (tempTime / 10) % 100;
    uint8_t second = (tempTime / 1000) % 60;
    uint8_t minute = (tempTime / 1000) / 60;

    lv_label_set_text_fmt(label_time, "%02u:%02u:%02u", minute, second, milliSecond);
  }
}


bool timerOn = false;
unsigned long timerTime;
unsigned long timerStartTime;
unsigned long beepTime;
bool beepOn;
void updateTimer() {
  unsigned long tempTime = timerTime;

  if (timerOn) {
    if (millis() - timerStartTime > tempTime) {
      tempTime = 0;
      if (millis() - beepTime > 500) {
        if (!beepOn) {
          ledcWrite(5, 0);
        } else {
          ledcWrite(5, 2048);
        }
        beepOn = !beepOn;
        beepTime = millis();
      }
    } else {
      tempTime -= millis() - timerStartTime;
    }
  }

  uint8_t second = (tempTime / 1000) % 60;
  uint8_t minute = (tempTime / 60000) % 60;
  uint8_t hour = (tempTime / 3600000);

  lv_label_set_text_fmt(label_time, "%02u:%02u:%02u", hour, minute, second);
}


void updateAxis(lv_chart_series_t *axis, bool primaryY) {
  int max = -10000;
  int min = 10000;
  int32_t *arr = lv_chart_get_series_y_array(chart_temp, axis);

  for (int i = 0; i < 10; i++) {
    int arr_val = *(arr + i);
    if (-10000 < arr_val && arr_val < 10000) {
      if (arr_val > max) {
        max = arr_val;
      }
      if (arr_val < min) {
        min = arr_val;
      }
    }
  }

  int buffer = (max - min) * 0.1;
  max += buffer;
  min -= buffer;
  float quarterValue = (min + (max - min)/4)/10;
  float threeQuarterValue = (min + 3*(max - min)/4)/10;
  
  if (primaryY) {
    lv_chart_set_axis_range(chart_temp, LV_CHART_AXIS_PRIMARY_Y, min, max);
    lv_scale_set_range(scale_temp, max, min);
    lv_label_set_text_fmt(label_time, "%.2f °C\n%.2f %%", temp.temperature, humidity.relative_humidity);
    lv_label_set_text_fmt(label_name, "%.1f\n\n\n\n\n\n%.1f", threeQuarterValue, quarterValue);
  } else {
    lv_chart_set_axis_range(chart_temp, LV_CHART_AXIS_SECONDARY_Y, min, max);
    lv_scale_set_range(scale_hum, min, max);
    lv_label_set_text_fmt(label_date, "%.1f\n\n\n\n\n\n%.1f", threeQuarterValue, quarterValue);
  }
}


void updateGyro() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  int x, y, z;
  if (!gyroMode) {
    x = 120 + 120 * a.acceleration.x / 10;
    y = 120 - 120 * a.acceleration.y / 10;
    z = 60 * a.acceleration.z / 10;

    lv_label_set_text_fmt(label_time, "Acceleration\nX: %.2f\nY: %.2f\nZ: %.2f", a.acceleration.x, a.acceleration.y, a.acceleration.z);
  } else {
    x = 120 + 120 * g.gyro.x / 15;
    y = 120 - 120 * g.gyro.y / 15;
    z = 60 * g.gyro.z / 15;

    lv_label_set_text_fmt(label_time, "Gyroscope\nX: %.2f\nY: %.2f\nZ: %.2f", g.gyro.x, g.gyro.y, g.gyro.z);
  }
  setGyroCoord(x, y, z);
  lv_line_set_points(gyro_line, gyro_line_points, 6);
}

void setGyroCoord(int &x, int &y, int &z) {
  gyro_line_points[0] = { 120, 120 };
  gyro_line_points[1] = { x, 120 };
  gyro_line_points[2] = { 120, 120 };
  gyro_line_points[3] = { 120, y };
  gyro_line_points[4] = { 120, 120 };
  gyro_line_points[5] = { 120 - z, 120 + z };
}

void updatePressureApp() {
  //label: time
  lv_label_set_text_fmt(label_time, "%.2f hPa \n Alt: %.2f m", bmp.pressure, bmp.readAltitude());

  //gauge: pressure
  lv_scale_set_line_needle_value(gauge_pressure, line_pressure, 80, bmp.pressure);
}

void darkMode(int avgLux) {
  if (avgLux < 10000) {
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(image_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_time, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_color(label_name, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_color(label_date, lv_color_hex(0xffffff), LV_PART_MAIN);
  } else {
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(image_icon, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_time, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_color(label_name, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_color(label_date, lv_color_hex(0x000000), LV_PART_MAIN);
  }
}


struct previousMillis {
  unsigned long lvgl;
  unsigned long time;
  unsigned long timeFlicker;
  unsigned long temp;
  unsigned long gyro;
  unsigned long als;
  unsigned long air;
};
previousMillis prevMillis;

struct buttonSetting {
  unsigned long time = 0;
  bool previous = false;
  bool next = false;
  bool both = false;
};
buttonSetting button;


void loop() {
  if (digitalRead(2) == LOW && digitalRead(9) == LOW) {
    if (app == 2 && !button.both) {
      timerOn = true;
      timerStartTime = millis();
      button.previous = false;
      button.next = false;
      button.both = true;
    }
  } else if (digitalRead(2) == LOW) {
    if (!button.previous && !button.both) {
      button.previous = true;
      button.time = millis();
    }
  } else if (digitalRead(9) == LOW) {
    if (!button.next && !button.both) {
      button.next = true;
      button.time = millis();
    }
  } else {
    button.both = false;
    if (button.previous) {
      Serial.println("Previous");
      button.previous = false;
      if (millis() - button.time > 500) {
        Serial.print("Entering App");
        app = page;
        appChange();
      } else {
        if (app == -1) {
          if (page > 0) {
            page--;
          } else {
            page = appNum;
          }
          pageChange();
        } else if (app == 0) {
          updateTime(1);
        } else if (app == 1) {
          if (!stopwatchOn) {
            stopwatchOn = true;
          } else {
            stopwatchOn = false;
          }
          stopwatchTime = millis() - stopwatchTime;
        } else if (app == 2) {
          if (timerOn) {
            timerOn = false;
            if (millis() - timerStartTime > timerTime) {
              timerTime = 0;
            } else {
              timerTime -= millis() - timerStartTime;
            }
          } else {
            timerTime += 1000;
          }
        } else if (app == 3) {

        } else if (app == 4) {
          gyroMode = !gyroMode;
        }
      }
    }
    if (button.next) {
      Serial.println("Next");
      button.next = false;
      if (millis() - button.time > 500) {
        Serial.print("Exiting App");
        app = -1;
        pageChange();
      } else {
        if (app == -1) {
          if (page < appNum) {
            page ++;
          } else {
            page = 0;
          }
          pageChange();
        } else if (app == 0) {
          updateTime(-1);
        } else if (app == 1 && !stopwatchOn) {
          stopwatchTime = 0;
          updateStopwatch();
        } else if (app == 2) {
          if (!timerOn) {
            if (timerTime > 1000) {
              timerTime -= 1000;
            } else {
              timerTime = 0;
            }
          }
        } else if (app == 3) {

        } else if (app == 4) {
          gyroMode = !gyroMode;
        }
      }
    }
  }

  if (page == 0 && millis() - prevMillis.time > 500) {
    updateTime(0);

    float battery;
    for (int i = 0; i < 100; i++) { //read battery voltage 100 times to average it out
      battery += analogReadMilliVolts(3);
    }
    battery = 2.0 * battery / 100.0;
    Serial.println(battery);
    int batteryPercent = map(battery, 3400, 4200, 0, 100);
    Serial.println(batteryPercent);
    lv_scale_set_section_range(scale_battery, battery_status, batteryPercent, 0);
    if (batteryPercent <= 20) {
      lv_style_set_arc_color(&battery_style, lv_color_hex(0xFF4D00));
    } else {
      lv_style_set_arc_color(&battery_style, lv_color_hex(0x000000));
    }

    prevMillis.time = millis();
  }

  if (app == 0 && millis() - prevMillis.timeFlicker > 500) {
    updateTime(0);
    if (lv_obj_has_flag(label_time, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_remove_flag(label_time, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);
    }
    prevMillis.timeFlicker = millis();
  }

  if (app == 1 && stopwatchOn) {
    updateStopwatch();
  }

  if (app == 2) {
    updateTimer();
  }

  if (millis() - prevMillis.temp > 3600000) {
    sht4.getEvent(&humidity, &temp);
    lv_chart_set_next_value(chart_temp, ser_temp, 10 * temp.temperature);
    lv_chart_set_next_value(chart_temp, ser_hum, 10 * humidity.relative_humidity);
    if (app == 3) {
      updateAxis(ser_temp, true);
      updateAxis(ser_hum, false);
    }
    prevMillis.temp = millis();
  }

  if (app == 4) {
    updateGyro();
  }

  if (app == 5 && millis() - prevMillis.air > 1000) {
    bmp.setPowerMode(BMP5XX_POWERMODE_FORCED);
    bmp.performReading();
    updatePressureApp();
    prevMillis.air = millis();
  }

  if (millis() - prevMillis.als > 500) {
    //calculate ambient brightness with VEML6035
    Wire.beginTransmission(0x29);
    Wire.write(0x04);
    Wire.endTransmission(false);
    Wire.requestFrom(0x29, 2);
    uint16_t rawLux = 0;
    byte LSB = 0;
    byte MSB = 0;
    while (Wire.available() > 0) {
      LSB = Wire.read();
      MSB = Wire.read();
    }
    rawLux = MSB << 8 | LSB;
    int milliLux = rawLux * 128;
    avgALS[alsIndex] = milliLux;
    if (alsIndex < 9) {
      alsIndex++;
    } else {
      alsIndex = 0;
    }
    unsigned int sum;
    for (int i = 0; i < 10; i++) {
      sum += avgALS[i];
    }
    Serial.println(sum / 10);
    darkMode(sum / 10);
    prevMillis.als = millis();
  }


  if (millis() - prevMillis.lvgl > 5) {
    lv_timer_handler(); /* let the GUI do its work */
    prevMillis.lvgl = millis();
  }
}