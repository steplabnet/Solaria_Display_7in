#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int   display_get_max(int idx);
int   display_get_min(int idx);
void  display_set_setpoint(int idx, float val);
void  display_confirm_setpoint(int idx, float val);

#ifdef __cplusplus
}
#endif
