#ifndef NP_WIFIMANAGER_SRC_H_
#define NP_WIFIMANAGER_SRC_H_

#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_err.h>
#include "../lib/include/mongoose.h"

#define WM_TAG "WiFiManager"

#define WM_SSID_LEN 32
#define WM_PASSWORD_LEN 64
#define WM_CONN_BIT BIT0

#define WM2CHAR(s) (char *)s
#define WM2CCHAR(s) (const char *)s

#define WM2CB(cb) (mg_event_handler_t)cb

#define BOOLTOCHAR(bool) (bool ? "true" : "false")

typedef enum wm_state {
  WM_OK,
  WM_ERR,
  WM_UNINITIALIZED,
  WM_NOT_STARTED,
  WM_STARTED,
  WM_NO_STA_SSID,
  WM_NO_AP_SSID,
  WM_MODE_NOT_SET,
  WM_MODE_UNSUPPORTED,
  WM_NVS_INIT_ERROR,
  WM_EVENT_LOOP_ERROR,
  WM_WIFI_INIT_ERROR,
  WM_CONFIG_ERROR,
  WM_WIFI_START_ERROR,
  WM_LISTENER_ERROR,
  WM_MG_CALLBACK_USERDATA_DISABLED,
  WM_SCAN_ERROR,
  WM_BAD_HTML
} wm_state_t;

typedef enum wm_scan_state {
  WM_SCAN_STATE_NOTSCANNED,
  WM_SCAN_STATE_ACTIVE,
  WM_SCAN_STATE_DONE
} wm_scan_state_t;

typedef struct wm {
  wifi_config_t ap_config;
  wifi_config_t sta_config;
  wifi_mode_t mode;
  uint8_t mode_update;
  uint8_t wifi_connected;
  uint8_t wifi_has_ip;
  unsigned char* html;
  int html_len;
  char ip[16];
  EventGroupHandle_t wifi_event_group;
  struct mg_mgr mgr;
  struct mg_connection* nc;
  char port[5];
  wm_scan_state_t scan_state;
  uint16_t scan_num_found;
  wifi_ap_record_t* scan_result;
  uint8_t initialized;
  uint8_t started;
} wm_t;

#define ESP2EXIT(COND, ERR) if(COND != ESP_OK){ return ERR; }

wm_state_t wm_init(wm_t* wm);

wm_state_t wm_start(wm_t* wm);

wm_state_t wm_loop(wm_t* wm, int milli);

wm_state_t wm_print_info(wm_t* wm);

char* wm_state_to_char(wm_state_t state);

wm_state_t wm_set_port(wm_t* wm, char* port);

wm_state_t wm_set_html(wm_t* wm, unsigned char* html, int len);

wm_state_t wm_ap_set_config(wm_t* wm, wifi_config_t* ap_config);

wm_state_t wm_sta_set_config(wm_t* wm, wifi_config_t* ap_config);

wm_state_t wm_ap_set_login(wm_t* wm, char* ssid, char* password);

wm_state_t wm_sta_set_login(wm_t* wm, char* ssid, char* password);

#endif
