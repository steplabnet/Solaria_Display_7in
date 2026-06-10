#ifndef INCOMING_DATA_H
#define INCOMING_DATA_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "user_sd.h"

#define NUM_SENSORS 14
typedef struct
{
    int display_id;
    String display_name;
    String sensorLabel;
    long idRiga;
    String temperature[NUM_SENSORS]; // temperature visualizzate sulla pagina
    String btnLabel[NUM_SENSORS];
    float setPoint[NUM_SENSORS];
    String regulatorLabel[NUM_SENSORS];
    int outValue;
    int mode[NUM_SENSORS]; // modalità del display o: cambia, 1:visualizza, 3: boolean
    int min[NUM_SENSORS];
    int max[NUM_SENSORS];
    int btnEn[NUM_SENSORS]; // enable state for mode-2 buttons: 0=off(red), 1=on(green)

} TYPE_DISPLAY_DATA;

/** contenuto del display
 *  variabili, parametri, id
 */





#endif