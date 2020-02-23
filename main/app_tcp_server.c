/**
  ******************************************************************************
  * @file			app_tcp_server.c
  * @brief			app_tcp_server function
  * @author			Xli
  * @email			xieliyzh@163.com
  * @version		1.0.0
  * @date			2020-02-21
  * @copyright		2020, EVECCA Co.,Ltd. All rights reserved
  ******************************************************************************
**/

/* Includes ------------------------------------------------------------------*/
#include "app_tcp_server.h"
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

/* Private constants ---------------------------------------------------------*/
#define AP_WIFI_SSID        "ESP32_AP"
#define AP_WIFI_PASS        ""
#define AP_MAX_CONN_STA     3

#define APP_TCP_SERVER_TASK_STK_SIZE     2048
#define APP_TCP_SERVER_TASK_PRIO         7

#define TCP_SERVER_PORT     3333

/* Private macro -------------------------------------------------------------*/
#define TAG_XLI                   __func__

/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/**
 * @brief sock句柄
 * 
 */
static int serv_sockfd = 0;

static EventGroupHandle_t wifi_event_group;
static TaskHandle_t app_tcp_server_task_handler;

/* Private function ----------------------------------------------------------*/
static void app_tcp_server_task(void *arg);


/**=============================================================================
 * @brief           WiFi事件回调
 *
 * @param[in]       none
 *
 * @return          none
 *============================================================================*/
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG_XLI, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG_XLI, "station:"MACSTR"leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    default:
        break;
    }
    return ESP_OK;
}

/**=============================================================================
 * @brief           WIFI初始化AP
 *
 * @param[in]       none
 *
 * @return          none
 *============================================================================*/
static void _wifi_init_ap(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_WIFI_SSID,
            .ssid_len = strlen(AP_WIFI_SSID),
            .password = AP_WIFI_PASS,
            .max_connection = AP_MAX_CONN_STA,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(AP_WIFI_PASS) == 0) 
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

/**=============================================================================
 * @brief           主函数
 *
 * @param[in]       none
 *
 * @return          none
 *============================================================================*/
void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    _wifi_init_ap();

    xTaskCreate((TaskFunction_t )app_tcp_server_task,
                (const char*    )"app_tcp_server_task",
                (uint16_t       )APP_TCP_SERVER_TASK_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )APP_TCP_SERVER_TASK_PRIO,
                (TaskHandle_t*  )&app_tcp_server_task_handler);

}


/**=============================================================================
 * @brief           Tcp服务端创建
 *
 * @param[in]       none
 *
 * @return          none
 *============================================================================*/
static int _tcp_server_create(void)
{
    struct sockaddr_in serv_addr;

    serv_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sockfd < 0)
    {
        ESP_LOGE(TAG_XLI, "Unable to create socket: errno %d", errno);
    }
    ESP_LOGI(TAG_XLI, "Socket created");

    bzero(&serv_addr, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(TCP_SERVER_PORT);

    return 0;
}

/**=============================================================================
 * @brief           Tcp服务端任务
 *
 * @param[in]       arg: 任务参数指针
 *
 * @return          none
 *============================================================================*/
static void app_tcp_server_task(void *arg)
{
    _tcp_server_create();

    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
}