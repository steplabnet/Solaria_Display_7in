#ifndef MAIN_H
#define MAIN_H

#define DEBUG_ON 1
#define DEBUG_OFF 0

#define RS485_RX_PIN 43
#define RS485_TX_PIN 44

// Redefine serial port name
#define RS485 Serial1

#define MAX_LABELS 12 // numero di pulsanti gestiti
#define MAX_PAGES  10 // numero massimo di configurazioni home

typedef enum
{

    STATO_BTN1,
    STATO_BTN2,
    STATO_BTN3,
    STATO_BTN4,
    STATO_BTN5,
    STATO_BTN6,
    STATO_BTN7,
    STATO_BTN8,
    STATO_BTN9,
    STATO_BTN10,
    STATO_BTN11,
    STATO_BTN12,
    STATO_BTN_NONE

} TYPE_BTN_PRESSED;

extern int arcSetpoint;
extern bool SETPOINT_CHANGED;
extern int DISPLAY_TYPE[MAX_LABELS];
extern bool HANDLE_BUTTON;
extern int actualScreenId;
extern int valueChanged[MAX_LABELS];
extern float newSetPoint[MAX_LABELS];
extern long azzera_counter;
extern unsigned long arcLastTouchTime;
extern unsigned long switchLastTouchTime;
extern bool BTN_ALWAYS_ON;
extern bool BTN_PULSANTE_CONTROLLO_REMOTO;
extern bool PERFORM_OTA_UPDATE;
extern bool TRIGGER_RESTART;
extern bool TRIGGER_COMUNICATION;
extern bool SELECT_NEXT_PAGE;
extern int  activePage;
extern bool pageAvailable[MAX_PAGES];
#ifdef __cplusplus
extern String pageLabels[MAX_PAGES][MAX_LABELS];
#endif
#endif
