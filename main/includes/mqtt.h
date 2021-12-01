#include "wifi.h"
#include "aws_iot_shadow_json_data.h"

#define CLIENT_ID_LEN (ATCA_SERIAL_NUM_SIZE * 2)
#define SUBSCRIBE_TOPIC_LEN (CLIENT_ID_LEN + 10)
#define BASE_PUBLISH_TOPIC_LEN (CLIENT_ID_LEN + 14)

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data);

void publish(AWS_IoT_Client *client, char *client_id, char *base_topic, uint16_t base_topic_len, char *payload, const uint8_t *aws_root_ca_pem_start);

void mqttConnect(AWS_IoT_Client *client, char *client_id, char *hostAddress, int port, const uint8_t *aws_root_ca_pem_start);

IoT_Error_t subscribe(AWS_IoT_Client *client, char *topic, pApplicationHandler_t callback_handler, char *from);




