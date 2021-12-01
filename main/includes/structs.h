#pragma once

#include <stdbool.h> 

typedef struct Environment {
  float temperature;  
  float humidity;  
} environment_t;

// TODO Remove
typedef struct ControlStates {
  int fan;  
} controls_states_t;

typedef struct Shadow {
  int maxTemp;
  int lastUpdateTimeStamp;
} shadow_t;


