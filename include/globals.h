// globals.h
#pragma once
#include "main.h"

#define DEBUG_ON 1
#define DEBUG_OFF 0
extern bool BROWSER_CONNECTED; // declare global variable
extern int RELOAD_CONFIGURATION;
extern bool STOP_PROGRAMMA_POMPA;
extern bool STOP_LOOP;
extern bool UPDATING_FIRMWARE;
extern bool CONFIG_RECEIVED;
extern bool SETPOINT_CHANGED;
extern bool SETPOINT_UPDATE;
extern int16_t DeviceID;
extern String centralName;

extern String outTopic;
extern String screenLabels[MAX_LABELS];

#include "incoming_data.h"

extern TYPE_DISPLAY_DATA display;

// #include <ArduinoJson.h>

// Tune this to the max JSON size you expect per message
#ifndef WS_JSON_CAPACITY
#define WS_JSON_CAPACITY 512
#endif

#ifndef WS_QUEUE_SIZE
#define WS_QUEUE_SIZE 50
#endif

// Global ring buffer and indices
extern JsonDocument Queue[WS_QUEUE_SIZE];
extern volatile int wsFront;
extern volatile int wsRear;

// Queue helpers

void compareString(String central);