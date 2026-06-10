#include <Arduino.h>
#include "globals.h"
#include "display_helpers.h"

extern float  pageNewSetPoints[MAX_PAGES][MAX_LABELS];
extern int    pageValueChanged[MAX_PAGES][MAX_LABELS];
extern int    activePage;
extern float  newSetPoint[MAX_LABELS];
extern int    valueChanged[MAX_LABELS];

extern "C" {

int display_get_max(int idx)
{
    return display.max[idx];
}

int display_get_min(int idx)
{
    return display.min[idx];
}

void display_set_setpoint(int idx, float val)
{
    display.setPoint[idx] = val;
}

void display_confirm_setpoint(int idx, float val)
{
    display.setPoint[idx]              = val;
    newSetPoint[idx]                   = val;
    valueChanged[idx]                  = 1;
    pageNewSetPoints[activePage][idx]  = val;
    pageValueChanged[activePage][idx]  = 1;
}

} // extern "C"
