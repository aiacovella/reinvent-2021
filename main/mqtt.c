/*
* Utility functions for configuring and initializing an MQTT client. 
*/
#include "esp_log.h"
#include "aws_iot_mqtt_client_interface.h"
#include "core2forAWS.h"
#include "ui.h"
#include <unistd.h>
#include <stdio.h>
#include "wifi.h"
#include "mqtt.h"
#include "parser.h"
#include "aws_iot_shadow_interface.h"

#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 200

/* Logger Prefix */
static const char *TAG = "MQTT";

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
    ESP_LOGW(TAG, "Disconnected from AWS IoT Core...");
    // ui_textarea_add("Disconnected from AWS IoT Core...", NULL, 0);
    IoT_Error_t rc = FAILURE;

    if(pClient == NULL) {
        return;
    }

    if(aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

void publish(AWS_IoT_Client *client, char *client_id, char *base_topic, uint16_t base_topic_len, char *payload, const uint8_t *aws_root_ca_pem_start){
    ESP_LOGD(TAG, "Publishing payload: %s", payload);

    IoT_Publish_Message_Params paramsQOS1;
    paramsQOS1.qos = QOS1;
    paramsQOS1.payload = (void *) payload;
    paramsQOS1.isRetained = 0;
    paramsQOS1.payloadLen = strlen(payload);

    IoT_Error_t rc = aws_iot_mqtt_publish(client, base_topic, base_topic_len, &paramsQOS1);

    while(MQTT_CLIENT_NOT_IDLE_ERROR == rc){
        ESP_LOGI(TAG, "re-attempting MQTT pubish");
        usleep(200000);
        rc = aws_iot_mqtt_publish(client, base_topic, base_topic_len, &paramsQOS1);
    }

    if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
        ESP_LOGE(TAG, "QOS1 publish ack not received.");
    }

}

