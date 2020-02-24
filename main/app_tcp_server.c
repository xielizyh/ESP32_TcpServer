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

#define TCP_SERVER_TASK_STK_SIZE            2048
#define TCP_SERVER_TASK_PRIO                7
#define PROCESS_CLIENT_TASK_STK_SIZE        2048
#define PROCESS_CLIENT_TASK_PRIO            7

#define TCP_SERVER_PORT                     6666
#define TCP_SERVER_LISTEN_CLIENT_NUM        3


/* Private macro -------------------------------------------------------------*/
#define TAG_XLI                   __func__

/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static EventGroupHandle_t wifi_event_group;
static TaskHandle_t app_tcp_server_task_handler;
static TaskHandle_t _process_client_task_handler;

/* Private function ----------------------------------------------------------*/
static void app_tcp_server_task(void *arg);
static void _process_client_task(void *arg);

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
                (uint16_t       )TCP_SERVER_TASK_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )TCP_SERVER_TASK_PRIO,
                (TaskHandle_t*  )&app_tcp_server_task_handler);

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
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int serv_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sockfd < 0)
    {
        ESP_LOGE(TAG_XLI, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG_XLI, "Socket created");

    bzero(&serv_addr, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(TCP_SERVER_PORT);

    if (bind(serv_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        ESP_LOGE(TAG_XLI, "Socket unable to bind: errno %d", errno);
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG_XLI, "Socket binded");

    if (listen(serv_sockfd, TCP_SERVER_LISTEN_CLIENT_NUM) < 0)
    {
        ESP_LOGE(TAG_XLI, "Error occured during listen: errno %d", errno);
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG_XLI, "Socket listening");
    
    while (1)
    {
        int client_sockfd = accept(serv_sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sockfd < 0)
        {
            ESP_LOGE(TAG_XLI, "Unable to accept connection: errno %d", errno);
        }
        ESP_LOGI(TAG_XLI, "A new client is connected, socket_fd=%d, addr=%s", client_sockfd, inet_ntoa(client_addr.sin_addr));
        
        BaseType_t res = xTaskCreate((TaskFunction_t )_process_client_task,
                                    (const char*    )"_process_client_task",
                                    (uint16_t       )PROCESS_CLIENT_TASK_STK_SIZE,
                                    (void*          )client_sockfd,
                                    (UBaseType_t    )PROCESS_CLIENT_TASK_PRIO,
                                    (TaskHandle_t*  )&_process_client_task_handler);
        if (res != pdPASS)
        {
            shutdown(client_sockfd, 0);
            close(client_sockfd);
        }
    }
    
    vTaskDelete(NULL);
}

/**=============================================================================
 * @brief           Tcp客户端处理任务
 *
 * @param[in]       arg: 任务参数指针
 *
 * @return          none
 *============================================================================*/
static void _process_client_task(void *arg)
{
    int client_sockfd = (int)arg;

    char rx_buffer[128] = {0};
    while (1)
    {
        int len = recv(client_sockfd, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0)
        {
            ESP_LOGE(TAG_XLI, "Recv failed: errno %d", errno);
            break;
        }
        else if (len == 0)
        {
            ESP_LOGI(TAG_XLI, "Connection closed");
            break;
        }
        else
        {
            ESP_LOGI(TAG_XLI, "Received %d bytes from socket_fd %d:", len, client_sockfd);
            ESP_LOGI(TAG_XLI, "%s", rx_buffer);
        }
    }
    
    shutdown(client_sockfd, 0);
    close(client_sockfd);   
    vTaskDelete(NULL); 
}