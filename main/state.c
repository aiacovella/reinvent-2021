#include "structs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "state.h"

static int currentScreen = SCREEN_INIT;

static int fanState = 0;

void setFanState(int updatedFanState){
    fanState = updatedFanState;
}

int getFanState(){
    return fanState;
}

static environment_t envir = {
    0.00, // temperature   
    0.00 // humidity   
};


void setEnvironmentReadings(environment_t updatedEnvironmentReadings){
    envir = updatedEnvironmentReadings;
}

environment_t getEnvironmentReadings(){
    return envir;
}

void setCurrentScreen(int current){
    currentScreen = current;
}

int getCurrentScreen(){
    return currentScreen;
}
