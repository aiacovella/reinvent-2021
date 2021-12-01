/*
 * UI API
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "core2forAWS.h"
#include "ui.h"
#include "aws_iot_mqtt_client_interface.h"
#include "mqtt.h"
#include "lvgl/lvgl.h"
#include "wifi.h"
#include "parser.h"
#include "state.h"
#include "aws_iot_shadow_interface.h"

static const char *TAG = "UI";

extern AWS_IoT_Client client;
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");

static bool wifi_state = false;

extern char * pClientID;
static lv_obj_t * pWIFILabel;
static lv_obj_t * pThermostatSlider;

static lv_obj_t * pTemperatureVal;
static lv_obj_t * pHumidityVal;
static lv_obj_t * pThermostatLabel;
static lv_obj_t * pStatusTextArea;

static uint32_t last_touch_millis = 0;

static lv_style_t temperatureStyle;
static lv_style_t humidityStyle;
static lv_style_t currentLimitStyle;

static bool displayDimmed = false;

void resetLastTouch(){
    last_touch_millis = lv_tick_get();
}

uint32_t getLastTouchMillis() {
    return last_touch_millis;
}

void setDisplayDimmed(bool dim) {
    if (dim && !displayDimmed){
        ESP_LOGE(TAG, "Dimming the display");
        Core2ForAWS_Display_SetBrightness(50);
        displayDimmed = true;
    }
    else if (!dim && displayDimmed){
        ESP_LOGD(TAG, "Restoring brightness");
        Core2ForAWS_Display_SetBrightness(100);
        resetLastTouch();
        displayDimmed = false;
    }
}

void clearCurrentScreen(){
    lv_obj_clean(lv_scr_act());
}

void publishDesiredMaxTemp(int maxTemp) {
    int topicLen = CLIENT_ID_LEN + 40;
    char publish_topic[topicLen];
    int topic_size = snprintf(publish_topic, topicLen, "$aws/things/%s/shadow/update", pClientID);

    ESP_LOGI(TAG, "Publishing desired max temp [%i] to topic [%s] Size %i",maxTemp, publish_topic, topic_size);
    char exPayload[200];
    
    sprintf(exPayload, "{\"state\":{\"desired\":{\"maxTemp\": %i}}}",  maxTemp);

    ESP_LOGD(TAG, "Publishing payload [%s]", exPayload);
    publish(&client, pClientID, publish_topic, topic_size, exPayload, aws_root_ca_pem_start);
}

void update_wifi_state(){
    if (wifi_state == false) {
        ESP_LOGI(TAG, "Updating WIFI for false");

        lv_label_set_text(pWIFILabel, LV_SYMBOL_WIFI);
    } 
    else{
        ESP_LOGI(TAG, "Updating WIFI for true");
        char buffer[25];
        sprintf (buffer, "#0000ff %s #", LV_SYMBOL_WIFI);
        lv_label_set_text(pWIFILabel, buffer);
    }
    ESP_LOGI(TAG, "WIFI updated");
}

void ui_textarea_add(lv_obj_t * text_area, char *baseTxt) {
    if( baseTxt != NULL ){
       lv_textarea_add_text(text_area, baseTxt); 
    } 
    else{
        ESP_LOGE(TAG, "Textarea baseTxt is NULL!"); 
    }
}

void uiWIFILabelUpdate(bool state){
    wifi_state = state;
    ESP_LOGI(TAG, "Updating WIFI label");
    update_wifi_state();
}

lv_obj_t* createLabel(lv_obj_t *container, const char * text){
    lv_obj_t *label = lv_label_create(container, NULL);
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t * addButton(lv_coord_t x_offset, lv_coord_t y_offset, lv_align_t align, const char * label_text, lv_event_cb_t event_cb, lv_coord_t width, lv_coord_t height){
    lv_obj_t * button = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_event_cb(button, event_cb);
    lv_obj_align(button, NULL, align, x_offset, y_offset);
    lv_obj_set_size(button, width, height); 

    createLabel(button, label_text);

    return button;
}

lv_obj_t * addSlider(lv_coord_t width, lv_coord_t height,  int16_t rangeMin, int16_t rangeMax, lv_coord_t x_offset, lv_coord_t y_offset, lv_align_t align, const char * label_text, lv_event_cb_t event_cb, int16_t initial_value){
    lv_obj_t *slider;
    lv_obj_t *label = createLabel(lv_scr_act(), label_text);

    slider = lv_slider_create(lv_scr_act(), NULL);
    lv_obj_set_width(slider, width);
    lv_obj_set_height(slider, height);
    lv_obj_align(slider, NULL, align, x_offset, y_offset);
    lv_slider_set_value(slider, initial_value, LV_ANIM_ON);
    lv_slider_set_range(slider, rangeMin, rangeMax);
    lv_obj_align(label, slider, LV_ALIGN_OUT_RIGHT_TOP, 0, -20);
    lv_obj_set_event_cb(slider, event_cb);

    return slider;
}

void addWifiLabel(){
    pWIFILabel = createLabel(lv_scr_act(), LV_SYMBOL_WIFI);
    lv_obj_align(pWIFILabel,NULL,LV_ALIGN_IN_TOP_LEFT, 5, 6);
    lv_label_set_recolor(pWIFILabel, true);
}

void updateStatus(char *text){
    ui_textarea_add(pStatusTextArea, text);
}

void initStartupScreen() {
    clearCurrentScreen();

    pStatusTextArea = lv_textarea_create(lv_scr_act(), NULL);
    lv_textarea_set_text(pStatusTextArea, "");
    lv_obj_set_size(pStatusTextArea, 300, 180);
    lv_obj_align(pStatusTextArea, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -12);
    lv_textarea_set_max_length(pStatusTextArea, 1024);
    lv_textarea_set_text_sel(pStatusTextArea, false);
    lv_textarea_set_cursor_hidden(pStatusTextArea, true);
    lv_obj_set_style_local_border_color(pStatusTextArea, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_SILVER);
    lv_obj_set_style_local_border_color(pStatusTextArea, LV_CONT_PART_MAIN, LV_STATE_FOCUSED, LV_COLOR_SILVER);

    addWifiLabel();

    update_wifi_state();

}

void updateThermostatLabel(int temperature){
    char buf[8];
    snprintf(buf, sizeof(buf), "%i", temperature);
    lv_label_set_text(pThermostatLabel, buf);
    lv_obj_align(pThermostatLabel, pThermostatSlider, LV_ALIGN_OUT_RIGHT_TOP, -30, -50);
    lv_obj_add_style(pThermostatLabel, LV_LABEL_PART_MAIN, &currentLimitStyle);
}

void updateThermostat(int maxTemp){
    lv_slider_set_value(pThermostatSlider, maxTemp, LV_ANIM_ON);
    updateThermostatLabel(maxTemp);
}

void enableThermostat(){
    if (getCurrentScreen() == SCREEN_THERMOSTAT) {
        lv_btn_set_state(pThermostatSlider, LV_BTN_STATE_RELEASED);
    }
}


void initThermostatScreen(){

    setCurrentScreen(SCREEN_THERMOSTAT);
    ESP_LOGE(TAG, "Initializing Thermostat Screen\n");
    clearCurrentScreen();

    lv_style_init(&temperatureStyle);
    lv_style_set_text_font(&temperatureStyle ,LV_STATE_DEFAULT, &lv_font_montserrat_48);
    lv_style_set_text_color(&temperatureStyle, LV_STATE_DEFAULT, LV_COLOR_BLUE);

    lv_style_init(&humidityStyle);
    lv_style_set_text_font(&humidityStyle ,LV_STATE_DEFAULT, &lv_font_montserrat_32);
    lv_style_set_text_color(&humidityStyle, LV_STATE_DEFAULT, LV_COLOR_GREEN);

    lv_style_init(&currentLimitStyle);
    lv_style_set_text_font(&currentLimitStyle ,LV_STATE_DEFAULT, &lv_font_montserrat_32);
    lv_style_set_text_color(&currentLimitStyle, LV_STATE_DEFAULT, LV_COLOR_RED);

    addWifiLabel();
    update_wifi_state();

    void thermostatSliderEventCallback(lv_obj_t * thermostat, lv_event_t event) {
        int16_t thermostat_value = lv_slider_get_value(thermostat);

        updateThermostatLabel(thermostat_value);

        if (event == LV_EVENT_RELEASED){
            ESP_LOGD(TAG, "Change Slider Value %i", thermostat_value);
            publishDesiredMaxTemp(thermostat_value);
        }

    }

    pThermostatSlider = addSlider(230, 35, 50, 100, 0, -34, LV_ALIGN_IN_BOTTOM_MID, "", thermostatSliderEventCallback, 0);
    pThermostatLabel = createLabel(lv_scr_act(), "");

    pTemperatureVal = createLabel(lv_scr_act(), "00.0 °F");
    lv_obj_align(pTemperatureVal, NULL, LV_ALIGN_IN_TOP_MID, -45, 25);
    lv_obj_add_style(pTemperatureVal, LV_LABEL_PART_MAIN, &temperatureStyle);
    
    pHumidityVal = createLabel(lv_scr_act(), "00 %");
    lv_obj_align(pHumidityVal, pTemperatureVal, LV_ALIGN_OUT_BOTTOM_MID, -25, 5);
    lv_obj_add_style(pHumidityVal, LV_LABEL_PART_MAIN, &humidityStyle);

}

void updateHumidityValue(float humidity){
    char value_ary[20];
    
    sprintf(value_ary, "%i %%", (int)humidity);
    lv_label_set_text(pHumidityVal, value_ary);
    lv_obj_add_style(pHumidityVal, LV_LABEL_PART_MAIN, &humidityStyle);
}

void updateTemperatureValue(float temperature){
    char value_ary[20];
    sprintf(value_ary, "%0.1f °F", temperature);
    lv_label_set_text(pTemperatureVal, value_ary);
    lv_obj_add_style(pTemperatureVal, LV_LABEL_PART_MAIN, &temperatureStyle);
}

void updateEnvironmentalReadingsUI(environment_t env){
    if (getCurrentScreen() == SCREEN_THERMOSTAT) {
        updateTemperatureValue(env.temperature);
        updateHumidityValue(env.humidity);
    }
}


