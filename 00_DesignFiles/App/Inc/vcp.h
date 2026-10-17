#ifndef VCP_H
#define VCP_H

#include <stdbool.h>
#include <stdint.h>
#include "usbd_def.h"

/* 0 boots as the keyboard only (PID 22315). 1 boots with the COM port
   (PID 22316). Vcp_SetEnabled changes it later; Vcp_Poll does the reconnect. */
#ifndef VCP_BOOT_ENABLE
#define VCP_BOOT_ENABLE 0
#endif

#define VCP_PID_KEYBOARD 22315U
#define VCP_PID_COMPOSITE 22316U

#define VCP_COMM_INTERFACE 1U
#define VCP_DATA_INTERFACE 2U
#define VCP_CMD_EP 0x82U
#define VCP_OUT_EP 0x03U
#define VCP_IN_EP 0x83U
#define VCP_CMD_PACKET 16U
#define VCP_DATA_PACKET 64U

#define VCP_SETUP_NOT_MINE 0U
#define VCP_SETUP_OK 1U
#define VCP_SETUP_FAIL 2U

void Vcp_Init(void);
void Vcp_Poll(void);
void Vcp_SetEnabled(bool enabled);
bool Vcp_IsEnabled(void);
bool Vcp_ConfigIsCom(void);
/* True while the host has the COM data endpoints open. */
bool Vcp_IsUp(void);
bool Vcp_SwitchPending(void);

/* Bytes queued or copied. Write returns 0 while the COM port is not up.
   A line that is exactly "KB9 VCP 0" or "KB9 VCP 1" is a switch command
   and is not returned by Vcp_Read. */
uint16_t Vcp_Write(const uint8_t *data, uint16_t length);
uint16_t Vcp_Read(uint8_t *data, uint16_t length);

void Vcp_Ep0Cancel(void);
uint8_t Vcp_ClassSetup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
void Vcp_Ep0RxReady(void);
void Vcp_UsbStart(USBD_HandleTypeDef *pdev);
void Vcp_UsbStop(void);
void Vcp_OnIn(uint8_t epnum);
void Vcp_OnOut(USBD_HandleTypeDef *pdev, uint8_t epnum);

#endif
