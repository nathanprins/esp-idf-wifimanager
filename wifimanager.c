#include "wifimanager.h"

char* wm_state_to_char(wm_state_t state){
  switch (state) {
    case WM_OK: return "WM_OK";
    case WM_ERR: return "WM_ERR";
    case WM_NO_STA_SSID: return "WM_NO_STA_SSID";
    case WM_NO_AP_SSID: return "WM_NO_AP_SSID";
    case WM_MODE_NOT_SET: return "WM_MODE_NOT_SET";
    case WM_MODE_UNSUPPORTED: return "WM_MODE_UNSUPPORTED";
    case WM_UNINITIALIZED: return "WM_UNINITIALIZED";
    case WM_NVS_INIT_ERROR: return "WM_NVS_INIT_ERROR";
    case WM_EVENT_LOOP_ERROR: return "WM_EVENT_LOOP_ERROR";
    case WM_WIFI_INIT_ERROR: return "WM_WIFI_INIT_ERROR";
    case WM_CONFIG_ERROR: return "WM_CONFIG_ERROR";
    case WM_WIFI_START_ERROR: return "WM_WIFI_START_ERROR";
    case WM_LISTENER_ERROR: return "WM_LISTENER_ERROR";
    default: return "WM_UNKNOWN_ERROR";
  }
}

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

static void wm_strcpy(uint8_t* dest, char* src, uint8_t rlen){
  if(rlen != 0)
    memset(dest, 0, rlen);
  memcpy(dest, src, strlen(src) + 1);
}

wm_state_t wm_init(wm_t* wm){
  wm->initialized = 0;

  esp_err_t err = nvs_flash_init();

  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  ESP2EXIT(err, WM_NVS_INIT_ERROR);

  wm->wifi_event_group = xEventGroupCreate();
  tcpip_adapter_init();
  ESP2EXIT(esp_event_loop_init(wm_event_handler, wm), WM_EVENT_LOOP_ERROR);

  wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP2EXIT(esp_wifi_init(&init_config), WM_WIFI_INIT_ERROR);

  esp_wifi_set_storage(WIFI_STORAGE_FLASH);

  esp_wifi_set_mode(WIFI_MODE_APSTA);

  esp_wifi_get_config(WIFI_IF_AP, &wm->ap_config);
  esp_wifi_get_config(WIFI_IF_STA, &wm->sta_config);
  wm->mode = wm->sta_config.sta.ssid[0] ? WIFI_MODE_STA : WIFI_MODE_APSTA;

  wm_set_port(wm, "80");

  wm->initialized = 1;
  return WM_OK;
}

static wm_state_t wm_save_config(wm_t* wm){
  switch (wm->mode) {
    case WIFI_MODE_STA:
      ESP2EXIT(esp_wifi_set_config(WIFI_IF_STA, &wm->sta_config), WM_CONFIG_ERROR);
      return WM_OK;
    case WIFI_MODE_APSTA:
      ESP2EXIT(esp_wifi_set_config(WIFI_IF_AP, &wm->ap_config), WM_CONFIG_ERROR);
      ESP2EXIT(esp_wifi_set_config(WIFI_IF_STA, &wm->sta_config), WM_CONFIG_ERROR);
      return WM_OK;
    case WIFI_MODE_NULL:
      return WM_MODE_NOT_SET;
    default:
      return WM_MODE_UNSUPPORTED;
  }
}

static void wm_endpoint_home(struct mg_connection *nc, int ev, void* ev_data){
  (void) ev; (void) ev_data;

  mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n<h1>/</h1>");

  nc->flags |= MG_F_SEND_AND_CLOSE;
}

static void wm_endpoint_connect(struct mg_connection *nc, int ev, void* ev_data){
  (void) ev; (void) ev_data;

  mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n<h1>/connect</h1>");

  nc->flags |= MG_F_SEND_AND_CLOSE;
}

static void wm_mg_handler(struct mg_connection *nc, int ev, void *p) {
  static const char *reply_fmt =
      "HTTP/1.0 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "Hello %s\n";

  switch (ev) {
    case MG_EV_ACCEPT: {
      char addr[32];
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                          MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      printf("Connection %p from %s\n", nc, addr);
      break;
    }
    case MG_EV_HTTP_REQUEST: {
      char addr[32];
      struct http_message *hm = (struct http_message *) p;
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                          MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      printf("HTTP request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
             hm->method.p, (int) hm->uri.len, hm->uri.p);
      mg_printf(nc, reply_fmt, addr);
      nc->flags |= MG_F_SEND_AND_CLOSE;
      break;
    }
    case MG_EV_CLOSE: {
      printf("Connection %p closed\n", nc);
      break;
    }
  }
}

wm_state_t wm_start(wm_t* wm){
  if(wm->initialized == 0)
    return WM_UNINITIALIZED;

  wm->mode = wm->sta_config.sta.ssid[0] ? WIFI_MODE_STA : WIFI_MODE_APSTA;
  esp_wifi_set_mode(wm->mode);
  wm_save_config(wm);
  ESP2EXIT(esp_wifi_start(), WM_WIFI_START_ERROR);

  mg_mgr_init(&wm->mgr, NULL);
  wm->nc = mg_bind(&wm->mgr, wm->port, wm_mg_handler);

  if(wm->nc == NULL)
    return WM_LISTENER_ERROR;

  switch (wm->mode) {
    case WIFI_MODE_APSTA:
      mg_register_http_endpoint(wm->nc, "/", wm_endpoint_home);
      mg_register_http_endpoint(wm->nc, "/connect", wm_endpoint_connect);
      break;
    case WIFI_MODE_STA:
      mg_register_http_endpoint(wm->nc, "/", wm_endpoint_home);
      break;
    default:
      break;
  }
  mg_set_protocol_http_websocket(wm->nc);

  return WM_OK;
}

wm_state_t wm_loop(wm_t* wm){
  mg_mgr_poll(&wm->mgr, 1000);
  return WM_OK;
}

wm_state_t wm_print_info(wm_t* wm){
  printf("-------- WiFiManager --------\n");
  printf(" STA: %s/%s (%s/%s)\n", wm->sta_config.sta.ssid, wm->sta_config.sta.password, wm->sta_config.ap.ssid, wm->sta_config.ap.password);
  printf("  AP: %s/%s (%s/%s)\n", wm->ap_config.ap.ssid, wm->ap_config.ap.password, wm->ap_config.sta.ssid, wm->ap_config.sta.password);
  if (wm->mode != WIFI_MODE_NULL)
    printf("MODE: %s\n", (wm->mode == WIFI_MODE_STA) ? "STA" : "APSTA");
  printf("-----------------------------\n");
  return WM_OK;
}

wm_state_t wm_set_port(wm_t* wm, char* port){
  strcpy(wm->port, port);
  return WM_OK;
}

wm_state_t wm_ap_set_config(wm_t* wm, wifi_config_t* ap_config){
  memcpy(&wm->ap_config, ap_config, sizeof(wifi_config_t));
  return WM_OK;
}

wm_state_t wm_sta_set_config(wm_t* wm, wifi_config_t* sta_config){
  memcpy(&wm->sta_config, sta_config, sizeof(wifi_config_t));
  return WM_OK;
}

wm_state_t wm_ap_set_login(wm_t* wm, char* ssid, char* password){
  if(ssid == NULL)
    return WM_NO_AP_SSID;

  wm_strcpy(wm->ap_config.ap.ssid, ssid, WM_SSID_LEN);
  if(password != NULL){
    wm_strcpy(wm->ap_config.ap.password, password, WM_PASSWORD_LEN);
    wm->ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  }

  return WM_OK;
}

wm_state_t wm_sta_set_login(wm_t* wm, char* ssid, char* password){
  if(ssid == NULL)
    return WM_NO_STA_SSID;

  wm_strcpy(wm->sta_config.sta.ssid, ssid, WM_SSID_LEN);
  if(password != NULL)
    wm_strcpy(wm->sta_config.sta.password, password, WM_PASSWORD_LEN);

  return WM_OK;
}
