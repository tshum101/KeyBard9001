#ifndef HID_KEYBOARD_H
#define HID_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>
#include "usbd_def.h"

#define HID_KEYBOARD_REPORT_LEN 8U
#define HID_CONSUMER_REPORT_LEN 1U

extern USBD_ClassTypeDef USBD_HID_Keyboard;

void HidKeyboard_SendIfChanged(const uint8_t report[HID_KEYBOARD_REPORT_LEN]);
void HidKeyboard_SendConsumerIfChanged(const uint8_t report[HID_CONSUMER_REPORT_LEN]);
bool HidKeyboard_ReleaseAll(void);

#endif
