#include "wifimanager.h"

char* wm_state_to_char(wm_state_t state){
  switch (state) {
    case WM_OK: return "WM_OK";
    case WM_ERR: return "WM_ERR";
    case WM_UNINITIALIZED: return "WM_UNINITIALIZED";
    case WM_NOT_STARTED: return "WM_NOT_STARTED";
    case WM_STARTED: return "WM_STARTED";
    case WM_NO_STA_SSID: return "WM_NO_STA_SSID";
    case WM_NO_AP_SSID: return "WM_NO_AP_SSID";
    case WM_MODE_NOT_SET: return "WM_MODE_NOT_SET";
    case WM_MODE_UNSUPPORTED: return "WM_MODE_UNSUPPORTED";
    case WM_NVS_INIT_ERROR: return "WM_NVS_INIT_ERROR";
    case WM_EVENT_LOOP_ERROR: return "WM_EVENT_LOOP_ERROR";
    case WM_WIFI_INIT_ERROR: return "WM_WIFI_INIT_ERROR";
    case WM_CONFIG_ERROR: return "WM_CONFIG_ERROR";
    case WM_WIFI_START_ERROR: return "WM_WIFI_START_ERROR";
    case WM_LISTENER_ERROR: return "WM_LISTENER_ERROR";
    case WM_MG_CALLBACK_USERDATA_DISABLED: return "WM_MG_CALLBACK_USERDATA_DISABLED";
    case WM_SCAN_ERROR: return "WM_SCAN_ERROR";
    case WM_BAD_HTML: return "WM_BAD_HTML";
    default: return "WM_UNKNOWN_ERROR";
  }
}

static wm_state_t wm_wifi_scan(wm_t* wm, uint8_t blocking);

static esp_err_t wm_event_handler(void *ctx, system_event_t *event){
  wm_t* wm = (wm_t*)ctx;
  switch(event->event_id) {
      case SYSTEM_EVENT_STA_START:
        if(wm->mode == WIFI_MODE_APSTA){
          wm_wifi_scan(wm, 0 /* Non-blocking */);
        }else{
          esp_wifi_connect();
        }
        break;
      case SYSTEM_EVENT_STA_CONNECTED:
        wm->wifi_connected = 1;
      case SYSTEM_EVENT_STA_GOT_IP:
        printf("got ip:%s \n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        strcpy(wm->ip, ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        printf("saved ip:%s \n", wm->ip);
        wm->wifi_has_ip = 1;
        xEventGroupSetBits(wm->wifi_event_group, WM_CONN_BIT);
        break;
      case SYSTEM_EVENT_STA_DISCONNECTED:
        wm->wifi_connected = 0;
        esp_wifi_connect();
        xEventGroupClearBits(wm->wifi_event_group, WM_CONN_BIT);
        break;
      case SYSTEM_EVENT_AP_STACONNECTED:
        // ESP_LOGI(WM_TAG, "station:"MACSTR" join, AID=%d", MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
        break;
      case SYSTEM_EVENT_AP_STADISCONNECTED:
        // ESP_LOGI(WM_TAG, "station:"MACSTR"leave, AID=%d", MAC2STR(event->event_info.sta_disconnected.mac), event->event_info.sta_disconnected.aid);
        break;
      case SYSTEM_EVENT_SCAN_DONE:
        wm->scan_state = WM_SCAN_STATE_DONE;
        wm->scan_num_found = event->event_info.scan_done.number;
        free(wm->scan_result);
        wm->scan_result = malloc(sizeof(wifi_ap_record_t) * wm->scan_num_found);
        esp_wifi_scan_get_ap_records(&wm->scan_num_found, wm->scan_result);
        break;
      default:
        break;
    }
    return ESP_OK;
}

wm_state_t wm_init(wm_t* wm){
  #if !MG_ENABLE_CALLBACK_USERDATA
    return WM_MG_CALLBACK_USERDATA_DISABLED;
  #endif

  wm->initialized = 0;
  wm->started = 0;
  wm->wifi_connected = 0;
  wm->mode_update = 0;
  wm->wifi_has_ip = 0;
  wm->scan_num_found = 0;
  wm->scan_state = WM_SCAN_STATE_NOTSCANNED;
  wm->html = NULL;
  wm->html_len = 0;

  memset(wm->ip, 0, 16);

  if (nvs_flash_init() == ESP_ERR_NVS_NO_FREE_PAGES) {
    nvs_flash_erase();
    ESP2EXIT(nvs_flash_init(), WM_NVS_INIT_ERROR);
  }

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
  ESP2EXIT(esp_wifi_set_config(WIFI_IF_STA, &wm->sta_config), WM_CONFIG_ERROR);
  if(wm->mode == WIFI_MODE_APSTA)
    ESP2EXIT(esp_wifi_set_config(WIFI_IF_AP, &wm->ap_config), WM_CONFIG_ERROR);
  return WM_OK;
}

static wm_state_t wm_wifi_scan(wm_t* wm, uint8_t blocking){
  if(wm->scan_state != WM_SCAN_STATE_ACTIVE){
    wm->scan_state = WM_SCAN_STATE_ACTIVE;
    wifi_scan_config_t scan_conf = {
      .ssid = NULL,
      .bssid = NULL,
      .channel = 0,
      .show_hidden = 0
    };
    ESP2EXIT(esp_wifi_scan_start(&scan_conf, blocking), WM_SCAN_ERROR);
  }
  return WM_OK;
}

static int wm_is_equal(const struct mg_str *s1, const char *s2) {
  const struct mg_str _s2 = { s2, strlen(s2) };
  return s1->len == _s2.len && memcmp(s1->p, _s2.p, _s2.len) == 0;
}

static int wm_is_request(struct http_message *hm, const char* uri, const char* method){
  return (wm_is_equal(&hm->uri, uri) && wm_is_equal(&hm->method, method));
}

static char* wm_get_query_value (char *query, char *name, uint8_t urldecode) {
    int r_len = 0, cur_c = 0, nam_c = 0, v_skip = 0;
    uint8_t match = 0;
    char* value;
    while(query[cur_c]){
        match = 1;
        char q_char = query[cur_c + nam_c];
        // While (name isn't terminated OR (query isn't terminated AND char isn't '&' and '=')) AND still a match
        while((name[nam_c] || (q_char != '=' && q_char != '&' && q_char)) && match){
            if(name[nam_c] != q_char) match = 0;
            nam_c++;
            q_char = query[cur_c + nam_c];
        }
        nam_c--; // Overshot by 1
        if(match){
            if(query[cur_c + nam_c + 1] != '=') return NULL;

            char v_char = query[cur_c + nam_c + 2 + r_len];
            while(v_char && v_char != '&'){
                if(urldecode && v_char == '%'){
                    r_len += 3;
                    v_skip += 2;
                }else{
                    r_len++;
                }

                v_char = query[cur_c + nam_c + 2 + r_len];
            }
            if(r_len == 0) return NULL;

            value = calloc(r_len - v_skip + 1, sizeof(char));
            if(urldecode){
              mg_url_decode(&query[cur_c + nam_c + 2], r_len, value, r_len - v_skip + 1, 1);
            }else{
              memcpy(value, &query[cur_c + nam_c + 2], r_len - v_skip);
            }
            return value;
        }

        nam_c = 0;
        cur_c++;
    }
    return NULL;
}

static void wm_mg_handler(struct mg_connection *nc, int ev, void *p) {
  wm_t* wm = (wm_t*) nc->user_data;
  switch (ev) {
    case MG_EV_ACCEPT: {
      // Used for debugging connections
      char addr[32];
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      printf("Connection %p from %s\n", nc, addr);
      break;
    }
    case MG_EV_HTTP_REQUEST: {
      // Parse request
      struct http_message *hm = (struct http_message *) p;

      if(wm_is_request(hm, "/", "GET") && wm->html_len)
      {
        printf("Home\n");
        mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n%.*s", wm->html_len, wm->html);
      }
      if(wm_is_request(hm, "/api/wifi", "GET"))
      {
        mg_send_response_line(nc, 200, "Content-Type: application/json");
        mg_printf(nc, "\r\n{");
          mg_printf(nc, "\"mode\":\"%s\",", (wm->mode == WIFI_MODE_APSTA) ? "apsta" : "sta");
          mg_printf(nc, "\"sta\":{");
            mg_printf(nc, "\"connected\":%s,", BOOLTOCHAR(wm->wifi_connected));
            mg_printf(nc, "\"has_ip\":%s", BOOLTOCHAR(wm->wifi_has_ip));
            if(wm->wifi_has_ip)
              mg_printf(nc, ",\"ip\":\"%s\"", wm->ip);
            if(!wm->wifi_connected){
              mg_printf(nc, ",\"scanState\":%d,", wm->scan_state);
              mg_printf(nc, "\"scanNumFound\":%d,", wm->scan_num_found);
              mg_printf(nc, "\"scanResults\":[");
              for(int i = 0; i < wm->scan_num_found; i++)
                mg_printf(nc, "{\"name\":\"%s\",\"rssi\":%d}%s", (char*)wm->scan_result[i].ssid, wm->scan_result[i].rssi, (i < wm->scan_num_found - 1) ? "," : "");
              mg_printf(nc, "]");
            }
          mg_printf(nc, "}");
        mg_printf(nc, "}");
      }
      else
      if(wm_is_request(hm, "/api/wifi/sta", "POST"))
      {
        mg_send_response_line(nc, 200, "Content-Type: application/json");
        char* body = calloc(hm->body.len + 1, sizeof(char));
        memcpy(body, hm->body.p, hm->body.len);

        char* d_ssid = wm_get_query_value(body, "ssid", 1);
        char* d_pass = wm_get_query_value(body, "password", 1);

        printf("Received creds: %s/%s \n\r", d_ssid, d_pass);

        if(d_ssid){
          memcpy(wm->sta_config.sta.ssid, d_ssid, strlen(d_ssid) + 1);
          if(d_pass)
            memcpy(wm->sta_config.sta.password, d_pass, strlen(d_pass) + 1);
          wm_save_config(wm);
          esp_wifi_connect();
          mg_printf(nc, "\r\n{\"connecting\":1}");
        }else{
          mg_printf(nc, "\r\n{\"connecting\":0}");
        }
        free(d_ssid); free(d_pass);
        free(body);
      }
      else
      if(wm_is_request(hm, "/api/wifi/mode", "POST"))
      {
        mg_send_response_line(nc, 200, "Content-Type: application/json");
        char* body = calloc(hm->body.len + 1, sizeof(char));
        memcpy(body, hm->body.p, hm->body.len);

        char* d_mode = wm_get_query_value(body, "mode", 1);

        printf("Received mode: %s \n\r", d_mode);

        if(d_mode){
          mg_printf(nc, "\r\n{\"success\":1}");
          wm->mode = (strcmp(d_mode, "apsta") == 0) ? WIFI_MODE_APSTA : WIFI_MODE_STA;
          wm->mode_update = 5;
        }else{
          mg_printf(nc, "\r\n{\"success\":0}");
        }
        free(d_mode);
        free(body);
      }
      nc->flags |= MG_F_SEND_AND_CLOSE;
      break;
    }
    case MG_EV_CLOSE: {
      // Used for debugging connections
      printf("Connection %p closed\n", nc);
      break;
    }
  }
}

wm_state_t wm_start(wm_t* wm){
  if(wm->initialized == 0)
    return WM_UNINITIALIZED;
  if(wm->started)
    return WM_STARTED;

  wm->mode = wm->sta_config.sta.ssid[0] ? WIFI_MODE_STA : WIFI_MODE_APSTA;
  ESP2EXIT(esp_wifi_set_mode(wm->mode), WM_ERR);
  wm_save_config(wm);
  ESP2EXIT(esp_wifi_start(), WM_WIFI_START_ERROR);

  mg_mgr_init(&wm->mgr, NULL);
  wm->nc = mg_bind(&wm->mgr, wm->port, MG_CB(WM2CB(wm_mg_handler), wm));

  if(wm->nc == NULL)
    return WM_LISTENER_ERROR;

  mg_set_protocol_http_websocket(wm->nc);

  wm->started = 1;
  return WM_OK;
}

wm_state_t wm_loop(wm_t* wm, int milli){
  if(!wm->started)
    return WM_NOT_STARTED;

  mg_mgr_poll(&wm->mgr, milli);

  // Multiple loops required in order to be sure all connections are closed before shutting down the softAP
  if(wm->mode_update > 1) wm->mode_update--;
  if(wm->mode_update == 1){
    esp_wifi_set_mode(wm->mode);
    wm->mode_update = 0;
  }

  return WM_OK;
}

wm_state_t wm_set_html(wm_t* wm, unsigned char* html, int len){
  if(html == NULL) return WM_BAD_HTML;
  wm->html = html;
  wm->html_len = len ? len : strlen((char*)html);
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

  strcpy(WM2CHAR(wm->ap_config.ap.ssid), ssid);
  wm->ap_config.ap.ssid_len = strlen(WM2CCHAR(ssid));
  if(password != NULL){
    strcpy(WM2CHAR(wm->ap_config.ap.password), password);
    wm->ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  }

  return WM_OK;
}

wm_state_t wm_sta_set_login(wm_t* wm, char* ssid, char* password){
  if(ssid == NULL)
    return WM_NO_STA_SSID;

  strcpy(WM2CHAR(wm->sta_config.sta.ssid), ssid);
  if(password != NULL)
    strcpy(WM2CHAR(wm->sta_config.sta.password), password);

  return WM_OK;
}
