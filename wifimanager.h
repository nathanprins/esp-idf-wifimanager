//#include "libs/mongoose.h"
#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_err.h>

#define WM_TAG "WiFiManager"

#define WM_SSID_LEN 32
#define WM_PASSWORD_LEN 64
#define WM_CONN_BIT BIT0

typedef struct wm {
  wifi_config_t config;
  wifi_mode_t mode;
  EventGroupHandle_t wifi_event_group;
} wm_t;

typedef enum wm_state {
  WM_OK,
  WM_ERR,
  WM_NO_STA_SSID,
  WM_NO_AP_SSID,
  WM_MODE_NOT_SET,
  WM_MODE_UNSUPPORTED
} wm_state_t;

#define ESP2EXIT(COND, ERR) (COND == ESP_OK ? return ERR : NULL)

wm_state_t wm_init(wm_t* wm);

wm_state_t wm_start(wm_t* wm);

wm_state_t wm_print_info(wm_t* wm);

wm_state_t wm_set_config(wm_t* wm, wifi_config_t* config);

wm_state_t wm_set_ap_login(wm_t* wm, char* ssid, char* password);

wm_state_t wm_set_sta_login(wm_t* wm, char* ssid, char* password);
