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
#define AP_WIFI_SSID                        "ESP32_AP"
#define AP_WIFI_PASS                        ""
#define AP_MAX_CONN_STA                     1

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
static TaskHandle_t app_tcp_server_single_conn_task_handler;
static TaskHandle_t app_tcp_server_multi_conn_task_handler;

/* Private function ----------------------------------------------------------*/
static void app_tcp_server_single_conn_task(void *arg);
static void app_tcp_server_multi_conn_task(void *arg);

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

#if 0
    xTaskCreate((TaskFunction_t )app_tcp_server_single_conn_task,
                (const char*    )"app_tcp_server_task",
                (uint16_t       )TCP_SERVER_TASK_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )TCP_SERVER_TASK_PRIO,
                (TaskHandle_t*  )&app_tcp_server_single_conn_task_handler);

#else
    xTaskCreate((TaskFunction_t )app_tcp_server_multi_conn_task,
                (const char*    )"app_tcp_server_multi_conn_task",
                (uint16_t       )TCP_SERVER_TASK_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )TCP_SERVER_TASK_PRIO,
                (TaskHandle_t*  )&app_tcp_server_multi_conn_task_handler);                

#endif
}




/**=============================================================================
 * @brief           Tcp服务端任务
 *
 * @param[in]       arg: 任务参数指针
 *
 * @return          none
 *============================================================================*/
static void app_tcp_server_single_conn_task(void *arg)
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

    if (listen(serv_sockfd, 1) < 0)
    {
        ESP_LOGE(TAG_XLI, "Error occured during listen: errno %d", errno);
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG_XLI, "Socket listening");
    
    char rx_buffer[128] = {0};
    while (1)
    {
        int client_sockfd = accept(serv_sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sockfd < 0)
        {
            ESP_LOGE(TAG_XLI, "Unable to accept connection: errno %d", errno);
        }
        ESP_LOGI(TAG_XLI, "A new client is connected, socket_fd=%d, addr=%s", client_sockfd, inet_ntoa(client_addr.sin_addr));

        while (1)
        {        
            int len = recv(client_sockfd, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0)
            {
                ESP_LOGE(TAG_XLI, "Recv failed: errno %d", errno);
                shutdown(client_sockfd, 0);
                close(client_sockfd);  
                break; 
            }
            else if (len == 0)
            {
                ESP_LOGI(TAG_XLI, "Connection closed");
                shutdown(client_sockfd, 0);
                close(client_sockfd);   
                break;
            }
            else
            {
                ESP_LOGI(TAG_XLI, "Received %d bytes from socket_fd %d:", len, client_sockfd);
                ESP_LOGI(TAG_XLI, "%s", rx_buffer);
            }
        }
    }
    
    vTaskDelete(NULL);
}


/**=============================================================================
 * @brief           Tcp服务端并发任务
 *
 * @param[in]       arg: 任务参数指针
 *
 * @return          none
 * 
 * @refrence        https://blog.csdn.net/u012351051/article/details/89398683?
 *                  depth_1-utm_source=distribute.pc_relevant.none-task&
 *                  utm_source=distribute.pc_relevant.none-task
 *============================================================================*/
static void app_tcp_server_multi_conn_task(void *arg)
{
    struct sockaddr_in serv_addr;
    fd_set all_set, read_set;    /*!< 定义文件句柄集合 */
    int sockfd_max = 0; /*!< 文件句柄最大值 */

    int serv_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sockfd < 0)
    {
        ESP_LOGE(TAG_XLI, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG_XLI, "Socket created, serv_sockfd=%d", serv_sockfd);

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
    
    FD_ZERO(&all_set);   /*!< 清空集合 */
    FD_SET(serv_sockfd, &all_set);   /*!< 添加serv_sockfd */
    sockfd_max = serv_sockfd;

    while (1)
    {
        read_set = all_set;
        if (select(sockfd_max+1, &read_set, NULL, NULL, NULL) == -1)    /*!< 监听句柄集的可读性 */
        {
            ESP_LOGE(TAG_XLI, "Sever select error");
        }
        for (int sockfd = 0; sockfd <= sockfd_max; sockfd++)    /*!< 从零开始判断 */
        {
            if (!FD_ISSET(sockfd, &read_set))   /*!< sockfd在集合中状态是否变化 */
            {
                continue;   /*!< 跳出当前循环 */
            }
            if (sockfd == serv_sockfd)   /*!< 新的客户端连接 */
            {
                struct sockaddr_in cli_addr;
                socklen_t cli_addr_len;
                int cli_sockfd;
                cli_addr_len = sizeof(cli_addr);
                memset(&cli_addr, 0, cli_addr_len);
                cli_sockfd = accept(serv_sockfd, (struct sockaddr *)&cli_addr, &cli_addr_len);
                if (cli_sockfd < 0)
                {
                    ESP_LOGE(TAG_XLI, "Unable to accept connection: errno %d", errno);
                }    
                else
                {
                    FD_SET(cli_sockfd, &all_set);
                    if (cli_sockfd > sockfd_max)
                    {
                        sockfd_max = cli_sockfd;
                    }
                    ESP_LOGI(TAG_XLI, "sockfd_max=%d", sockfd_max);
                    ESP_LOGI(TAG_XLI, "A new client[cli_sockfd=%d] is connected from %s", cli_sockfd, inet_ntoa(cli_addr.sin_addr));
                }             
            }
            else    /*!< 客户端的通信数据 */
            {
                char rx_buffer[128] = {0};
                int len = recv(sockfd, rx_buffer, sizeof(rx_buffer) - 1, 0);
                if (len < 0)
                {
                    ESP_LOGE(TAG_XLI, "Recv failed: errno %d", errno);
                    FD_CLR(sockfd, &all_set);   /*!< 从集合中删除 */
                    close(sockfd);
                    ESP_LOGI(TAG_XLI, "sockfd_max=%d", sockfd_max);
                    break;
                }
                else if (len == 0)
                {
                    ESP_LOGI(TAG_XLI, "Connection closed");
                    FD_CLR(sockfd, &all_set);   /*!< 从集合中删除 */
                    close(sockfd);
                    ESP_LOGI(TAG_XLI, "sockfd_max=%d", sockfd_max);
                    break;
                }
                else
                {
                    ESP_LOGI(TAG_XLI, "Received %d bytes from socket_fd[%d]:", len, sockfd);
                    ESP_LOGI(TAG_XLI, "%s", rx_buffer);
                }
            }
        }
    }
    
    vTaskDelete(NULL);
}