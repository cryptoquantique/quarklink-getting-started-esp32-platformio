#include <freertos/FreeRTOS.h>
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_https_ota.h"

#include "quarklink.h"
#include "led_strip.h"

#include "mqtt_client.h"
#include "esp_ota_ops.h"

#define LED_STRIP_BLINK_GPIO  8 // GPIO assignment
#define LED_STRIP_LED_NUMBERS 1 // LED numbers in the strip
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000) // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static const char *TAG = "wifi station";
static const char *IEF_TAG = "IEF";
static const char *QL_TAG = "QL";
static const char *MQTT_TAG = "mqtt event handler";

int CONFIG_BROKER_BIN_SIZE_TO_SEND = 20000;

quarklink_context_t quarklink;

    char *CQ_ENDPOINT = "";
    int CQ_PORT = 0;
    char *CQ_GATEWAY_CERT = "";

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGI(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "MQTT_EVENT_CONNECTED", 0, 1, 0);
        ESP_LOGI(MQTT_TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(MQTT_TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(MQTT_TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(MQTT_TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(MQTT_TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(MQTT_TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(MQTT_TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    default:
        ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void error_loop(const char *message) {
    if (message != NULL) {
        ESP_LOGE(QL_TAG, "%s", message);
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

int mqtt_init(esp_mqtt_client_handle_t* client){

    static bool is_running = false;
    if (is_running == true) {
        return 0;
    }
    static char deviceKey[QUARKLINK_MAX_KEY_LENGTH];

    quarklink_return_t ret = quarklink_getDeviceKey(&quarklink,deviceKey,QUARKLINK_MAX_KEY_LENGTH);
    if (ret != QUARKLINK_SUCCESS){
        ESP_LOGE(TAG, "quarklink_getDeviceKey Error");
    }
    
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.hostname = quarklink.iotHubEndpoint,
            .address.port = quarklink.iotHubPort,
            .address.transport = MQTT_TRANSPORT_OVER_SSL,
            .verification.certificate = quarklink.iotHubRootCert
        },
        .credentials ={
            .client_id = quarklink.deviceID,
            .authentication = {
                .certificate= quarklink.deviceCert,
                .key = deviceKey,
            }
        }
    };

    ESP_LOGI(MQTT_TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    *client = esp_mqtt_client_init(&mqtt_cfg);
    if (*client == NULL){
        return -1;
    }
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(*client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (esp_mqtt_client_start(*client) == ESP_OK) {
        is_running = true;
        return 0;
    }
    else {
        is_running = false;
        return -1;
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(IEF_TAG, "OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(IEF_TAG, "Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(IEF_TAG, "Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(IEF_TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(IEF_TAG, "Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGD(IEF_TAG, "Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(IEF_TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(IEF_TAG, "OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(IEF_TAG, "OTA abort");
                break;
        }
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config;
    /* Load existing configuration and prompt user */
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

    ESP_ERROR_CHECK(esp_wifi_start() );
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s",
                 wifi_config.sta.ssid);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                 wifi_config.sta.ssid);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

void getting_started_task(void *pvParameter) {

    quarklink_return_t ql_ret;
    quarklink_return_t ql_status = QUARKLINK_ERROR;
    
    /* get status */
    ESP_LOGI(QL_TAG, "Get status");
    ql_status = quarklink_status(&quarklink);
    switch (ql_status) {
        case QUARKLINK_STATUS_ENROLLED:
            ESP_LOGI(QL_TAG, "Enrolled");
            // if Enrollement context was not stored force  a new Enroll
            if (strcmp(quarklink.iotHubEndpoint,"") == 0){
                ql_status = QUARKLINK_STATUS_NOT_ENROLLED;
            }
            ESP_LOGI(QL_TAG, "endpoint %s", quarklink.iotHubEndpoint);
            break;
        case QUARKLINK_STATUS_FWUPDATE_REQUIRED:
            ESP_LOGI(QL_TAG, "Firmware Update required");
            break;
        case QUARKLINK_STATUS_NOT_ENROLLED:
            ESP_LOGI(QL_TAG, "Not enrolled");
            break;
        case QUARKLINK_STATUS_CERTIFICATE_EXPIRED:
            ESP_LOGI(QL_TAG, "Certificate expired");
            break;
        case QUARKLINK_STATUS_REVOKED:
            ESP_LOGI(QL_TAG, "Device revoked");
            break;
        default:
            ESP_LOGI(QL_TAG, "Error during status request");
            break;
    }

    if (ql_status == QUARKLINK_STATUS_NOT_ENROLLED ||
        ql_status == QUARKLINK_STATUS_CERTIFICATE_EXPIRED ||
        ql_status == QUARKLINK_STATUS_REVOKED) {
        /* enroll */
        ESP_LOGI(QL_TAG, "Enrol to %s", quarklink.endpoint);
        ql_ret = quarklink_enrol(&quarklink);
        switch (ql_ret) {
            case QUARKLINK_SUCCESS:
                ESP_LOGI(QL_TAG, "Successfully enrolled!");
                ql_ret = quarklink_persistEnrolmentContext(&quarklink);
                if (ql_ret != QUARKLINK_SUCCESS){
                    ESP_LOGW(QL_TAG, "Failed to store the Enrolment context");
                    break;
                }
                ql_status = QUARKLINK_STATUS_ENROLLED;
                ESP_LOGI(QL_TAG, "endpoint %s", quarklink.iotHubEndpoint);
                break;
            case QUARKLINK_DEVICE_DOES_NOT_EXIST:
                ESP_LOGW(QL_TAG, "Device does not exist");
                break;
            case QUARKLINK_DEVICE_REVOKED:
                ESP_LOGW(QL_TAG, "Device revoked");
                break;
            case QUARKLINK_CACERTS_ERROR:
            default:
                ESP_LOGI(QL_TAG, "Error during enrol");
                break;
        }
    }
    
    if (ql_status == QUARKLINK_STATUS_FWUPDATE_REQUIRED) {
        /* firmware update */
        ESP_LOGI(QL_TAG, "Get firmware update");
        ql_ret = quarklink_firmwareUpdate(&quarklink, NULL);
        switch (ql_ret) {
            case QUARKLINK_FWUPDATE_UPDATED:
                ESP_LOGI(QL_TAG, "Firmware updated. Rebooting...");
                esp_restart();
                break;
            case QUARKLINK_FWUPDATE_NO_UPDATE:
                ESP_LOGI(QL_TAG, "No firmware update");
                break;
            case QUARKLINK_FWUPDATE_WRONG_SIGNATURE:
                ESP_LOGI(QL_TAG, "Wrong firmware signature");
                break;
            case QUARKLINK_FWUPDATE_MISSING_SIGNATURE:
                ESP_LOGI(QL_TAG, "Missing required firmware signature");
                break;
            case QUARKLINK_FWUPDATE_ERROR:
            default:
                ESP_LOGI(QL_TAG, "Error while updating firmware");
                break;
        }
    }

    esp_mqtt_client_handle_t client = NULL;
    ESP_LOGI(MQTT_TAG, "mqtt_init\n");
    mqtt_init(&client);

    char outTopic[100];
    strcpy(outTopic, "topic/");
    strcat(outTopic, quarklink.deviceID);

    while (1 && (ql_status == QUARKLINK_STATUS_ENROLLED)) {

        esp_mqtt_client_publish(client, outTopic, "PUBLISHING", 0, 0, 0);
        ESP_LOGI(MQTT_TAG, "PUBLISHING");
        vTaskDelay(10000 / portTICK_PERIOD_MS); 
        ESP_LOGI(QL_TAG, "Get status");
        ql_status = quarklink_status(&quarklink);
        if (ql_status == QUARKLINK_STATUS_FWUPDATE_REQUIRED) {
            ql_ret = quarklink_firmwareUpdate(&quarklink, NULL);
            switch (ql_ret) {
                case QUARKLINK_FWUPDATE_UPDATED:
                    ESP_LOGI(QL_TAG, "Firmware updated. Rebooting...");
                    esp_restart();
                    break;
                case QUARKLINK_FWUPDATE_NO_UPDATE:
                    ESP_LOGI(QL_TAG, "No firmware update");
                    break;
                case QUARKLINK_FWUPDATE_WRONG_SIGNATURE:
                    ESP_LOGI(QL_TAG, "Wrong firmware signature");
                    break;
                case QUARKLINK_FWUPDATE_MISSING_SIGNATURE:
                    ESP_LOGI(QL_TAG, "Missing required firmware signature");
                    break;
                case QUARKLINK_FWUPDATE_ERROR:
                default:
                    ESP_LOGI(QL_TAG, "Error while updating firmware");
                    break;
            }
        }
    }
}

void set_led(void){

    // LED Strip object handle
    led_strip_handle_t led_strip;

    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    led_strip_set_pixel(led_strip, 0, 0, 255, 0);
    led_strip_refresh(led_strip);
}

void app_main(void)
{
    printf("\n quarklink-getting_started esp32-c3 MQTT 0.1.0 GREEN LED\n");
    set_led(); // esp32-c3 RGB LED

    /* quarklink init */
    ESP_LOGI(QL_TAG, "Initialising QuarkLink");
    quarklink_return_t ql_ret = quarklink_init(&quarklink, CQ_ENDPOINT, 6000, CQ_GATEWAY_CERT);
    ql_ret = quarklink_loadStoredContext(&quarklink);
    if (ql_ret != QUARKLINK_SUCCESS || ql_ret != QUARKLINK_CONTEXT_NO_ENROLMENT_INFO_STORED) {
        ESP_LOGE(QL_TAG, "ql_ret %d\n", ql_ret);
    }
    ESP_LOGI(QL_TAG, "Device ID: %s", quarklink.deviceID);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    xTaskCreate(&getting_started_task, "getting_started_task", 1024 * 8, NULL, 5, NULL);
}
