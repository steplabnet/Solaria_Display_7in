#ifndef HOME_BUTTONS_H
#define HOME_BUTTONS_H
#include "Arduino.h"
#include "ui.h"
#include "main.h"
#include "incoming_data.h"

#define NUM_BUTTONS 12
/**/
String activeBtnLabel(const TYPE_DISPLAY_DATA &display, int i);
void UpdateBtnLabels(TYPE_DISPLAY_DATA display, String *oldLabels);
void UpdateBtnTemperatures(TYPE_DISPLAY_DATA display);
void UpdateBtnLeds(TYPE_DISPLAY_DATA display);
void UpdateTempLabelBackgrounds(TYPE_DISPLAY_DATA display);
#endif
