#include "wifimanager.h"

static esp_err_t wm_event_handler(void *ctx, system_event_t *event){
  wm_t* wm = (wm_t*)ctx;
  switch(event->event_id) {
      case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
      case SYSTEM_EVENT_STA_GOT_IP:
        printf("got ip:%s \n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wm->wifi_event_group, WM_CONN_BIT);
        break;
      case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wm->wifi_event_group, WM_CONN_BIT);
        break;
      case SYSTEM_EVENT_AP_STACONNECTED:
        // ESP_LOGI(WM_TAG, "station:"MACSTR" join, AID=%d", MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
        break;
      case SYSTEM_EVENT_AP_STADISCONNECTED:
        // ESP_LOGI(WM_TAG, "station:"MACSTR"leave, AID=%d", MAC2STR(event->event_info.sta_disconnected.mac), event->event_info.sta_disconnected.aid);
        break;
      default:
        break;
    }
    return ESP_OK;
}

static void wm_strcpy(uint8_t* dest, char* src){
  memcpy(dest, src, strlen(src) + 1);
}

wm_state_t wm_init(wm_t* wm){
  esp_err_t err = nvs_flash_init();

  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  wm->wifi_event_group = xEventGroupCreate();
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(wm_event_handler, wm));

  wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init_config));

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

  wifi_config_t c = {
    .ap = {
      .ssid = "TestSSID",
      .password = "TestPASS!",
      .authmode = WIFI_AUTH_WPA2_PSK
    }
  };
  esp_wifi_set_config(WIFI_IF_AP, &c);

  ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_AP, &wm->config));
  ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &wm->config));
  if(wm->config.sta.ssid[0] != 0)
    wm->mode = WIFI_MODE_AP;

  wm_print_info(wm);

  return WM_OK;
}

static wm_state_t wm_save_config(wm_t* wm){
  switch (wm->mode) {
    case WIFI_MODE_AP:
      esp_wifi_set_config(WIFI_IF_AP, &wm->config);
      return WM_OK;
    case WIFI_MODE_APSTA:
      esp_wifi_set_config(WIFI_IF_AP, &wm->config);
      esp_wifi_set_config(WIFI_IF_STA, &wm->config);
      return WM_OK;
    case WIFI_MODE_NULL:
      return WM_MODE_NOT_SET;
    default:
      return WM_MODE_UNSUPPORTED;
  }
}

wm_state_t wm_start(wm_t* wm){
  if(!wm->config.sta.ssid[0])
    wm->mode = WIFI_MODE_APSTA;
  else
    wm->mode = WIFI_MODE_STA;

  ESP_ERROR_CHECK(esp_wifi_set_mode(wm->mode));
  wm_save_config(wm);
  ESP_ERROR_CHECK(esp_wifi_start());
  return WM_OK;
}

wm_state_t wm_print_info(wm_t* wm){
  printf("-------- WiFiManager --------\n");
  printf(" AP: %s/%s\n", wm->config.ap.ssid, wm->config.ap.password);
  printf("STA: %s/%s\n", wm->config.sta.ssid, wm->config.sta.password);
  if (wm->mode != WIFI_MODE_NULL)
    printf("Mode: %s\n", (wm->mode == WIFI_MODE_AP) ? "AP" : "APSTA");
  printf("-----------------------------\n");
  return WM_OK;
}

wm_state_t wm_set_config(wm_t* wm, wifi_config_t* config){
  memcpy(&wm->config, config, sizeof(wifi_config_t));
  return WM_OK;
}

wm_state_t wm_set_ap_config(wm_t* wm, wifi_ap_config_t* ap_config){
  memcpy(&wm->config.ap, ap_config, sizeof(wifi_ap_config_t));
  return WM_OK;
}

wm_state_t wm_set_sta_config(wm_t* wm, wifi_sta_config_t* sta_config){
  memcpy(&wm->config.sta, sta_config, sizeof(wifi_sta_config_t));
  return WM_OK;
}

wm_state_t wm_set_ap_login(wm_t* wm, char* ssid, char* password){
  if(ssid == NULL)
    return WM_NO_AP_SSID;

  wm_strcpy(wm->config.ap.ssid, ssid);
  if(password != NULL){
    wm_strcpy(wm->config.ap.password, password);
    wm->config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  }

  return WM_OK;
}

wm_state_t wm_set_sta_login(wm_t* wm, char* ssid, char* password){
  if(ssid == NULL)
    return WM_NO_STA_SSID;

  wm_strcpy(wm->config.sta.ssid, ssid);
  if(password != NULL)
    wm_strcpy(wm->config.sta.password, password);

  return WM_OK;
}
