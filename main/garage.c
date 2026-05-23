#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"

#include "include/garage.h"

struct mive_garage_s
{
  int valid;
  nvs_handle_t mem_handle;
};

struct remote_data {
    uint32_t data;
};


static const char* TAG = "GARAGE";

static struct mive_garage_s g_garage_instance;

static const char * garage_data_part_label = "nvs_garage";
static const char * garage_data_namespace = "garage_data";

static const uint32_t magic_value = 0x494d4556;

static esp_err_t garage_wifi_init(void)
{
  esp_err_t ret = ESP_OK;
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ret = esp_netif_init();
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_netif_init failed: %d", ret);
    return ret;
  }
  ret = esp_event_loop_create_default();
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_event_loop_create_default failed: %d", ret);
    return ret;
  }
  ret = esp_wifi_init(&cfg);
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_wifi_init failed: %d", ret);
    esp_event_loop_delete_default();
    return ret;
  }
  ret = esp_wifi_set_storage(WIFI_STORAGE_RAM) ;
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_wifi_set_storage failed: %d", ret);
    esp_wifi_deinit();
    esp_event_loop_delete_default();
    return ret;
  }
  ret = esp_wifi_set_mode(WIFI_MODE_STA) ;
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_wifi_set_mode failed: %d", ret);
    esp_wifi_deinit();
    esp_event_loop_delete_default();
    return ret;
  }

  esp_wifi_set_country_code("HR", 0);

  ret = esp_wifi_start();
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_wifi_start failed: %d", ret);
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_event_loop_delete_default();
    return ret;
  }
  ret = esp_wifi_set_channel(13, WIFI_SECOND_CHAN_NONE);
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_wifi_set_channel failed: %d", ret);
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_event_loop_delete_default();
    return ret;
  }

  return ESP_OK;
}

static esp_err_t garage_espnow_init(nvs_handle_t mem_handle)
{
  // Read keys from flash
  uint8_t pmk[32] = {0};
  uint8_t lmk[32] = {0};
  uint8_t mac[16] = {0};

  esp_err_t ret = 0;
  size_t len = sizeof(pmk) - 1;
  ret = nvs_get_blob(mem_handle, "pmk", pmk, &len);

  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to get pmk: %d", ret);
    return ret;
  }

  len = sizeof(lmk) - 1;
  ret = nvs_get_blob(mem_handle, "lmk", lmk, &len);

  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to get lmk: %d", ret);
    return ret;
  }

  len = sizeof(mac) - 1;
  ret = nvs_get_blob(mem_handle, "garage_mac", mac, &len);

  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to get MAC: %d", ret);
    return ret;
  }

  /* Initialize ESPNOW and register sending and receiving callback function. */
  ESP_ERROR_CHECK( esp_now_init() );
  /* Set primary master key. */
  ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)pmk));

  /* Add broadcast peer information to peer list. */
  esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
  if (peer == NULL) {
    ESP_LOGE(TAG, "Malloc peer information fail");
    esp_now_deinit();
    return ESP_FAIL;
  }
  memset(peer, 0, sizeof(esp_now_peer_info_t));
  peer->channel = 13;
  peer->ifidx = ESP_IF_WIFI_STA;
  peer->encrypt = true;
  memcpy(peer->lmk, lmk, 16);
  memcpy(peer->peer_addr, mac, ESP_NOW_ETH_ALEN);
  ESP_ERROR_CHECK( esp_now_add_peer(peer) );
  free(peer);

  return ESP_OK;
}

void mive_garage_init(void)
{
  nvs_handle_t mem_handle;
  esp_err_t ret = nvs_flash_init_partition(garage_data_part_label);
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGE(TAG, "Invalid nvs partition. Check if it's flashed correctly");
    g_garage_instance.valid = 0;
    return;
  }

  ret = nvs_open_from_partition(garage_data_part_label, garage_data_namespace, NVS_READONLY, &mem_handle);

  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to open nvs partition: %d", ret);
    g_garage_instance.valid = 0;
    return;
  }

  ret = garage_wifi_init();
  if(ret != ESP_OK)
  {
    goto end;
  }

  ret = garage_espnow_init(mem_handle);
  if(ret != ESP_OK)
  {
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_event_loop_delete_default();
    goto end;
  }

  g_garage_instance.valid = 1;

end:
  nvs_close(mem_handle);

}

void mive_garage_deinit(void)
{
  if(g_garage_instance.valid)
  {
    esp_now_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_event_loop_delete_default();

    g_garage_instance.valid = 0;
  }
}

void mive_garage_activate(void)
{
  if(!g_garage_instance.valid)
  {
    ESP_LOGE(TAG, "Instance invalid");
    return;
  }

  struct remote_data data = {.data = magic_value};

  esp_now_send((uint8_t*)NULL, (uint8_t*)&data, sizeof(data));
}
