/**
 * @file main.c
 * @brief Demo of submitting an RGB color set to an MQTT service in AWS.
 *
 * Original license:  http://aws.amazon.com/apache2.0
 * This example allows a user to select a set of RBG colors and submit them to 
 * an MQTT service in AWS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <parser.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "sht31.h"
#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "core2forAWS.h"
#include "wifi.h"
#include "ui.h"
#include "mqtt.h"
#include "structs.h"
#include "state.h"
#include "aws_iot_shadow_interface.h"

#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 200

// Structure from shadow get
static char shadowFullDocument[600];

// Shadow structures
static jsonStruct_t maxTempShadow;

/* The time prefix used by the logger. */
static const char *TAG = "MAIN";

// Shadow states
static int maxTemp = 100;

/* CA Root certificate */
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");

/* Default MQTT HOST URL is pulled from the aws_iot_config.h */
// char HostAddress[255] = AWS_IOT_MQTT_HOST;

/* Default MQTT port is pulled from the aws_iot_config.h */
// uint32_t port = AWS_IOT_MQTT_PORT;

char * pClientID;

/* MQTT Cient */
AWS_IoT_Client client;

char* initializeDeviceClient() {
    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    char * clientID = malloc(CLIENT_ID_LEN + 1);

    ATCA_STATUS ret = Atecc608_GetSerialString(clientID);

    if (ret != ATCA_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to get device serial from secure element. Error: %i", ret);
        // TODO: Needs better handling or retry logic. 
        abort();
    }
    return clientID;
}

void handleSubscribeResponse(IoT_Error_t response_code, char * topic){
    if(SUCCESS != response_code) {
        ESP_LOGE(TAG, "Error %d subscribing to MQTT topic: %s ", response_code, topic);
        // TODO: Needs better handling or retry logic. 
        abort();
    } else {
        ESP_LOGE(TAG, "Subscribed to MQTT topic: %s ", topic);
    }
}

static void envergySavingTask(void* pvParameters){
    for(;;){
        if(FT6336U_WasPressed() == 1){
            setDisplayDimmed(false);
            resetLastTouch();
        }
 
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    vTaskDelete(NULL); /* Should never get to here... */
}

static void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
								const char *pReceivedJsonDocument, void *pContextData) {

	ESP_LOGD(TAG, "Shadow Updated");

	if(SHADOW_ACK_TIMEOUT == status) {
		ESP_LOGI(TAG, "Update Timeout--");
	} else if(SHADOW_ACK_REJECTED == status) {
		ESP_LOGI(TAG, "Update Rejected");
	} else if(SHADOW_ACK_ACCEPTED == status) {
		// ESP_LOGI(TAG, "Update Accepted !!");
	} else {
		ESP_LOGI(TAG, "Update yielded %i", status);
	}
}


IoT_Error_t publishToShadow(AWS_IoT_Client *client, char * clientID, jsonStruct_t max_temp_structure){
	char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
	size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

	IoT_Error_t	rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);

		if(SUCCESS == rc) {
			rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 1, &max_temp_structure);
			if(SUCCESS == rc) {
				rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);

				if(SUCCESS == rc) {
					// ESP_LOGI(TAG, "Shadow Updated: %s", JsonDocumentBuffer);
					ESP_LOGD(TAG, "Updating shadow with current values");
                    rc = aws_iot_shadow_update(client, clientID, JsonDocumentBuffer,
											   ShadowUpdateStatusCallback, NULL, 4, true);

                    if (rc != SUCCESS){
                        ESP_LOGE(TAG, "Shadow update error %i", rc);
                    }

				}
			}
		}

    return rc;

}

void publishCurrentState(int curMaxTemp){
    ESP_LOGD(TAG, "Current Max temp: %i  Shadow Max Temp: %i", curMaxTemp, maxTemp);
    maxTemp = curMaxTemp;
    ESP_LOGD(TAG, "Publishing max temp to shadow %i", maxTemp);
    IoT_Error_t rc = publishToShadow(&client, pClientID, maxTempShadow);
    if (rc != SUCCESS){
        ESP_LOGE(TAG, "Shadow publish error %i", rc);
    }
}

void maxTempCallback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    int * updated_max_temp = (int *) (pContext->pData);

    ESP_LOGD(TAG, "Max Temp Delta Received");

    if(pContext != NULL) {
        ESP_LOGE(TAG, "Setting max temp to %i in max temp callback", maxTemp);
        maxTemp = *updated_max_temp;

        // Let the shadow know that we received the update
        publishCurrentState(maxTemp);

        int currentScreen = getCurrentScreen();

        if (currentScreen == SCREEN_THERMOSTAT){
            updateThermostat(maxTemp);
        }

   }
   enableThermostat();
}

void initializeDeviceShadow(){
    IoT_Error_t rc;

    ShadowInitParameters_t sp = ShadowInitParametersDefault;
    sp.pHost = AWS_IOT_MQTT_HOST;
    sp.port = AWS_IOT_MQTT_PORT;
    sp.enableAutoReconnect = false;
    sp.disconnectHandler = disconnectCallbackHandler;

    sp.pRootCA = (const char *)aws_root_ca_pem_start;
    sp.pClientCRT = "#";
    sp.pClientKey = "#0";

    rc = aws_iot_shadow_init(&client, &sp);
	if(SUCCESS != rc) {
		IOT_ERROR("Shadow Initialization Error: %i", rc);
        // TODO: Needs better handling or retry logic. 
        abort();
	}

    ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
    scp.pMyThingName = pClientID;
    scp.pMqttClientId = pClientID;
    scp.mqttClientIdLen = CLIENT_ID_LEN;

	updateStatus("Connecting to shadow\n");

    rc = aws_iot_shadow_connect(&client, &scp);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_shadow_connect returned error %d, aborting...", rc);
        // TODO: Needs better handling or retry logic. 
        abort();
    }

	updateStatus("Registering shadow delta\n");

}

IoT_Error_t registerMaxTempDelta(jsonStruct_t *max_temp_struct){
    IoT_Error_t rc = aws_iot_shadow_register_delta(&client, max_temp_struct);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Unable to register shadow delta for Fan State");
        // TODO: Needs better handling or retry logic. 
        abort();
    }
    return rc;
}

void shadowCallBack(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
							const char *pReceivedJsonDocument, void *pContextData) {

	ESP_LOGD(TAG, "Shadow state document: %s", pReceivedJsonDocument);
	if(status != SHADOW_ACK_TIMEOUT) {
		strcpy(shadowFullDocument, pReceivedJsonDocument);
        shadow_t shadow = parseShadow(shadowFullDocument);
        ESP_LOGI(TAG, "Last shadow max value: %i last shadow update %i", shadow.maxTemp, shadow.lastUpdateTimeStamp);
        // Update the initial temperature set point
        maxTemp = shadow.maxTemp;
    
        ESP_LOGI(TAG, "=== === Updating max temp from shadow with %i", maxTemp);
        updateThermostat(maxTemp);

	} else {
        ESP_LOGE(TAG, "Unable to get the configured max set point from the shadow");
    }
}

void publishTelemetry() {
    int topicLen = CLIENT_ID_LEN + 40;
    char publish_topic[topicLen];
    int topic_size = snprintf(publish_topic, topicLen, "things/%s/env", pClientID);

    ESP_LOGD(TAG, "Publish telemetry topic [%s] Size %i", publish_topic, topic_size);
    char exPayload[200];
    
    environment_t env = getEnvironmentReadings();

    sprintf(exPayload, "{\"temperature\" : %.2f, \"humidity\" : %.2f, \"fan\" : %i, \"setpoint\" : %i}",  env.temperature, env.humidity, getFanState(), maxTemp);
    publish(&client, pClientID, publish_topic, topic_size, exPayload, aws_root_ca_pem_start);
}

void publishTelemetryTask(){
    for(;;){
        publishTelemetry();
        vTaskDelay(pdMS_TO_TICKS(5000));  // check every second
    }
}

void monitorTemperatureTask(){
    for(;;){

        // read the temperature and humidity
        // if the temperature is over the setpoint, turn the fan on
        // if the temperature is under the setpoint, turn the fan off
        if (sht31_readTempHum()) {
            float humidity = sht31_readHumidity();
            float tempC = sht31_readTemperature();
            float tempF = tempC *1.8 + 32;
            
            ESP_LOGD(TAG, "Humidity: %.f, Temp: %.1f,  Max: %i", humidity, tempF, maxTemp);

            environment_t updatedEnvReadings = {tempF, humidity};
            setEnvironmentReadings(updatedEnvReadings);
            updateEnvironmentalReadingsUI(updatedEnvReadings);

            bool fanOn = getFanState() == 1;
            
            // TODO check current fan state too
            if (! fanOn && tempF > (float)maxTemp) { 
                ESP_LOGI(TAG, "Turning Fan ON");
                // turn the fan on
                Core2ForAWS_Port_Write(GPIO_NUM_26, 1);
                setFanState(1);

            } else if (fanOn && tempF < (float) maxTemp) {
                ESP_LOGI(TAG, "Turning Fan OFF");
                // turn the fan off
                Core2ForAWS_Port_Write(GPIO_NUM_26, 0);
                setFanState(0);
            }
        } else {
            ESP_LOGI(TAG, "sht31_readTempHum : failed");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  // check every second
    }
}

void awsIotTask(void *param) {

    IoT_Error_t rc = SUCCESS;

    pClientID = initializeDeviceClient();

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);    

    // Initialize device shadow
    updateStatus("Initializing device shadow\n");
    initializeDeviceShadow(); 

    // Build out the shadow structure for the max temp callback
    updateStatus("Registering max temp delta\n");
    maxTempShadow.cb = maxTempCallback;
    maxTempShadow.pKey = "maxTemp";
    maxTempShadow.pData = &maxTemp;
    maxTempShadow.type = SHADOW_JSON_UINT16;
    maxTempShadow.dataLength = sizeof(float);
    
    registerMaxTempDelta(&maxTempShadow);
    initThermostatScreen();
   
    // Get the latest max temperature from the shadow
    IoT_Error_t res = aws_iot_shadow_get(&client, pClientID, shadowCallBack, NULL, 1, false);
    if (res != SUCCESS){
        ESP_LOGD(TAG, "Restore from shadow failed with error code %i", res);
    }

    while((MQTT_CLIENT_NOT_IDLE_ERROR == rc || NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc )) {
        ESP_LOGD(TAG, "IoT task running on core %u", xPortGetCoreID());
        rc = aws_iot_shadow_yield(&client, 100);
        
        ESP_LOGD(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));

        uint32_t touchElapsed = lv_tick_elaps(getLastTouchMillis());

        if (touchElapsed > 60000){
            setDisplayDimmed(true);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
  
    }

    ESP_LOGE(TAG, "An error occurred in the network connection loop. %i", rc);
    // TODO: Needs better handling or retry logic. 
    abort();
}

void app_main()
{

    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(100);

    // Initialize the startup UI
    initStartupScreen();
    updateStatus("Starting M5 Core2 Demo\n");
    updateStatus("Initializing WIFI\n");

    // Connect via WIFI
    initialize_wifi();

    /* Wait for WiFI to show as connected */
    updateStatus("Retrieving client id from secure element\n");
    pClientID = initializeDeviceClient();

    // Display the device unique identifier
    updateStatus("Client ID: ");
    updateStatus(pClientID);
    updateStatus("\n");

    // spin up the awsIotTask
    xTaskCreatePinnedToCore(&awsIotTask, "aws_iot_task", 4096*3, NULL, configMAX_PRIORITIES, NULL, 1);
    xTaskCreate(envergySavingTask, "energy_saving_task", 2048, NULL, 10, NULL);

    sht31_init();   // TODO verify success, this crashes if not plugged in
    esp_err_t err = Core2ForAWS_Port_PinMode(GPIO_NUM_26, OUTPUT);
    if (err == ESP_OK){
        xTaskCreate(monitorTemperatureTask, "monitor_temp_task", 1024 * 4, NULL, 10, NULL);
        xTaskCreate(publishTelemetryTask, "publish_telemetry_task", 1024 * 4, NULL, 10, NULL);
    }

}