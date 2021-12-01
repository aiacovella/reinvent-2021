#include "structs.h"

void setEnvironmentReadings(environment_t updatedEnvironmentReadings);
environment_t getEnvironmentReadings();

void setFanState(int state);
int getFanState();

enum {
    SCREEN_INIT,
    SCREEN_THERMOSTAT /* Number of states*/
};

void setCurrentScreen(int current);
int getCurrentScreen();

  


