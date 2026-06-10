#pragma once

/**
 *  Handskae with central:
 *
 *  On boot there is an array of variables that become 1 if received:
 *      -  array of button labels
 *      -  array
 *
 */

// FreeRTOS-compatible task function that polls RS485Serial for JSON and handles it.
void TaskSerialPoll(void *parameter);
