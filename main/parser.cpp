#include "ArduinoJson.h"
#include "structs.h"
#include "esp_log.h"

char const TAG[7] = "Parser";

/*
 * This CPP file serves as a wrapper fro the Arduino Json library.    
 */

extern "C"{

    controls_states_t parseControlStates(char * json){
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, json);

        // Test if parsing succeeds.
        if (error) {
            ESP_LOGE(TAG, "Error parsing control states json %s", json);
            return controls_states_t{false};

        } else {
            bool fan = doc["fan"];
            return controls_states_t{fan};
        }

    }

    shadow_t parseShadow(char * json){
        StaticJsonDocument<800> doc;

	    ESP_LOGD(TAG, "Parsing shadow document: %s", json);

        DeserializationError error = deserializeJson(doc, json);

        // Test if parsing succeeds.
        if (error) {
            ESP_LOGE(TAG, "Error parsing last update timestamp json [%s]", json);
            ESP_LOGI(TAG, "Error Code %i", error.code());
            return shadow_t{0, 0};

        } else {
            int lastUpdateTimeStamp = doc["metadata"]["desired"]["fan"]["timestamp"];
            int maxTemp = doc["state"]["desired"]["maxTemp"];
            return shadow_t{maxTemp, lastUpdateTimeStamp};
        }
    }



    environment_t parseEnvironmentData(char * json){
        StaticJsonDocument<400> doc;
        DeserializationError error = deserializeJson(doc, json);

        // Test if parsing succeeds.
        if (error) {
            ESP_LOGI(TAG, "Error parsing environmental data json %s", json);
            return environment_t{0.0, 0.0};

        } else {

            float temperature = doc["temperature"];
            float humidity = doc["humidity"];  

            return  environment_t{
                temperature,
                humidity
            };
        }
    }

}