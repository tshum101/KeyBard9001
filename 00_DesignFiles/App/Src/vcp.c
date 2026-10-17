#include "vcp.h"

#include <string.h>
#include "dbg.h"
#include "hid_keyboard.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

#define VCP_RING_SIZE 512U
#define VCP_CDC_SET_LINE_CODING 0x20U
#define VCP_CDC_GET_LINE_CODING 0x21U
#define VCP_CDC_SET_CONTROL_LINE 0x22U
#define VCP_NOTIFY_SERIAL_STATE 0x20U
#define VCP_DISCONNECT_MS 300U
#define VCP_RELEASE_WAIT_MS 100U
/* "KB9 VCP 0" and "KB9 VCP 1" are 9 characters. The newline is not counted. */
#define VCP_CMD_TEXT_LEN ((uint8_t)(sizeof("KB9 VCP 0") - 1U))

extern USBD_HandleTypeDef hUsbDeviceFS;
extern uint8_t USBD_FS_DeviceDesc[USB_LEN_DEV_DESC];

static USBD_HandleTypeDef *s_pdev;
static uint8_t s_target;
static uint8_t s_live;
static volatile uint8_t s_dirty;
static uint8_t s_active;
static uint8_t s_line_state;
static uint8_t s_line_pending;
static uint8_t s_notify_busy;
static uint8_t s_tx_inflight;
static uint8_t s_zlp;
static uint16_t s_tx_len;
static uint8_t s_rx_armed;
static uint8_t s_holding;
static uint8_t s_hold_len;
static uint8_t s_line_start;
static uint8_t s_swallow_break;
static uint32_t s_switch_ms;
static uint8_t s_switch_waiting;

static uint8_t s_rx[VCP_RING_SIZE];
static uint8_t s_tx[VCP_RING_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static volatile uint16_t s_tx_head;
static volatile uint16_t s_tx_tail;
static char s_hold[VCP_CMD_TEXT_LEN + 1U];

_Static_assert(sizeof("KB9 VCP 0") == sizeof("KB9 VCP 1"), "vcp commands differ in length");
_Static_assert(VCP_CMD_TEXT_LEN == 9U, "vcp command length");

__ALIGN_BEGIN static uint8_t s_line[7] __ALIGN_END;
__ALIGN_BEGIN static uint8_t s_line_rx[7] __ALIGN_END;
__ALIGN_BEGIN static uint8_t s_rx_pkt[VCP_DATA_PACKET] __ALIGN_END;
__ALIGN_BEGIN static uint8_t s_tx_pkt[VCP_DATA_PACKET] __ALIGN_END;
__ALIGN_BEGIN static uint8_t s_notify[10] __ALIGN_END;

static uint32_t Vcp_Lock(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void Vcp_Unlock(uint32_t primask)
{
  __set_PRIMASK(primask);
}

static uint16_t Vcp_Next(uint16_t index)
{
  index++;
  if (index >= VCP_RING_SIZE)
  {
    index = 0U;
  }
  return index;
}

static uint16_t Vcp_Used(uint16_t head, uint16_t tail)
{
  if (head >= tail)
  {
    return (uint16_t)(head - tail);
  }
  return (uint16_t)(VCP_RING_SIZE - tail + head);
}

static uint16_t Vcp_RxFree(void)
{
  return (uint16_t)(VCP_RING_SIZE - 1U - Vcp_Used(s_rx_head, s_rx_tail));
}

static uint16_t Vcp_TxFree(void)
{
  return (uint16_t)(VCP_RING_SIZE - 1U - Vcp_Used(s_tx_head, s_tx_tail));
}

static void Vcp_RxPush(uint8_t byte)
{
  uint16_t next;

  next = Vcp_Next(s_rx_head);
  if (next == s_rx_tail)
  {
    return;
  }
  s_rx[s_rx_head] = byte;
  s_rx_head = next;
}

static void Vcp_ResetLineCoding(void)
{
  uint32_t rate = 115200U;

  s_line[0] = (uint8_t)(rate & 0xFFU);
  s_line[1] = (uint8_t)((rate >> 8) & 0xFFU);
  s_line[2] = (uint8_t)((rate >> 16) & 0xFFU);
  s_line[3] = (uint8_t)((rate >> 24) & 0xFFU);
  s_line[4] = 0U;
  s_line[5] = 0U;
  s_line[6] = 8U;
}

static void Vcp_PatchDeviceDesc(void)
{
  uint16_t pid = s_live ? VCP_PID_COMPOSITE : VCP_PID_KEYBOARD;

  USBD_FS_DeviceDesc[4] = s_live ? 0xEFU : 0x00U;
  USBD_FS_DeviceDesc[5] = s_live ? 0x02U : 0x00U;
  USBD_FS_DeviceDesc[6] = s_live ? 0x01U : 0x00U;
  USBD_FS_DeviceDesc[10] = (uint8_t)(pid & 0xFFU);
  USBD_FS_DeviceDesc[11] = (uint8_t)((pid >> 8) & 0xFFU);
}

static void Vcp_ClearRings(void)
{
  s_rx_head = 0U;
  s_rx_tail = 0U;
  s_tx_head = 0U;
  s_tx_tail = 0U;
  s_tx_len = 0U;
  s_tx_inflight = 0U;
  s_zlp = 0U;
  s_rx_armed = 0U;
  s_holding = 0U;
  s_hold_len = 0U;
  s_line_start = 1U;
  s_swallow_break = 0U;
  s_notify_busy = 0U;
  s_line_pending = 0U;
}

static void Vcp_ArmRxLocked(void)
{
  if ((s_active == 0U) || (s_rx_armed != 0U) || (s_pdev == NULL))
  {
    return;
  }
  if (Vcp_RxFree() < VCP_DATA_PACKET)
  {
    return;
  }
  s_rx_armed = 1U;
  if (USBD_LL_PrepareReceive(s_pdev, VCP_OUT_EP, s_rx_pkt, VCP_DATA_PACKET) != (uint8_t)USBD_OK)
  {
    s_rx_armed = 0U;
  }
}

static void Vcp_KickTxLocked(void)
{
  uint16_t count;

  if ((s_active == 0U) || (s_tx_inflight != 0U) || (s_pdev == NULL))
  {
    return;
  }
  if (s_pdev->dev_state != USBD_STATE_CONFIGURED)
  {
    return;
  }

  if (s_tx_len == 0U)
  {
    count = 0U;
    while ((count < VCP_DATA_PACKET) && (s_tx_tail != s_tx_head))
    {
      s_tx_pkt[count] = s_tx[s_tx_tail];
      s_tx_tail = Vcp_Next(s_tx_tail);
      count++;
    }
    if (count == 0U)
    {
      if (s_zlp == 0U)
      {
        return;
      }
      s_zlp = 0U;
      s_tx_inflight = 1U;
      if (USBD_LL_Transmit(s_pdev, VCP_IN_EP, s_tx_pkt, 0U) != (uint8_t)USBD_OK)
      {
        s_tx_inflight = 0U;
      }
      return;
    }
    s_tx_len = count;
    s_zlp = ((count == VCP_DATA_PACKET) && (s_tx_tail == s_tx_head)) ? 1U : 0U;
  }

  s_tx_inflight = 1U;
  if (USBD_LL_Transmit(s_pdev, VCP_IN_EP, s_tx_pkt, s_tx_len) != (uint8_t)USBD_OK)
  {
    s_tx_inflight = 0U;
  }
}

static void Vcp_SendSerialState(uint16_t state)
{
  if ((s_active == 0U) || (s_notify_busy != 0U) || (s_pdev == NULL))
  {
    return;
  }
  s_notify[0] = 0xA1U;
  s_notify[1] = VCP_NOTIFY_SERIAL_STATE;
  s_notify[2] = 0U;
  s_notify[3] = 0U;
  s_notify[4] = (uint8_t)VCP_COMM_INTERFACE;
  s_notify[5] = 0U;
  s_notify[6] = 2U;
  s_notify[7] = 0U;
  s_notify[8] = (uint8_t)(state & 0xFFU);
  s_notify[9] = (uint8_t)((state >> 8) & 0xFFU);
  s_notify_busy = 1U;
  if (USBD_LL_Transmit(s_pdev, VCP_CMD_EP, s_notify, sizeof(s_notify)) != (uint8_t)USBD_OK)
  {
    s_notify_busy = 0U;
  }
}

static void Vcp_FlushHold(uint8_t extra, uint8_t has_extra)
{
  uint8_t index;

  for (index = 0U; index < s_hold_len; index++)
  {
    Vcp_RxPush((uint8_t)s_hold[index]);
  }
  if (has_extra != 0U)
  {
    Vcp_RxPush(extra);
  }
  s_hold_len = 0U;
  s_holding = 0U;
}

static void Vcp_Accept(uint8_t byte)
{
  if (s_swallow_break != 0U)
  {
    s_swallow_break = 0U;
    if ((byte == '\n') || (byte == '\r'))
    {
      s_line_start = 1U;
      return;
    }
  }

  if ((byte == '\n') || (byte == '\r'))
  {
    if (s_holding != 0U)
    {
      s_hold[s_hold_len] = '\0';
      if ((s_hold_len == VCP_CMD_TEXT_LEN) && (memcmp(s_hold, "KB9 VCP 0", VCP_CMD_TEXT_LEN) == 0))
      {
        s_hold_len = 0U;
        s_holding = 0U;
        s_line_start = 1U;
        s_swallow_break = 1U;
        Vcp_SetEnabled(false);
        return;
      }
      if ((s_hold_len == VCP_CMD_TEXT_LEN) && (memcmp(s_hold, "KB9 VCP 1", VCP_CMD_TEXT_LEN) == 0))
      {
        s_hold_len = 0U;
        s_holding = 0U;
        s_line_start = 1U;
        s_swallow_break = 1U;
        Vcp_SetEnabled(true);
        return;
      }
      Vcp_FlushHold(byte, 1U);
    }
    else if (s_line_start == 0U)
    {
      Vcp_RxPush(byte);
    }
    s_line_start = 1U;
    return;
  }

  if (s_line_start != 0U)
  {
    s_line_start = 0U;
    if (byte == (uint8_t)'K')
    {
      s_holding = 1U;
      s_hold_len = 1U;
      s_hold[0] = 'K';
      return;
    }
    Vcp_RxPush(byte);
    return;
  }

  if (s_holding != 0U)
  {
    if (s_hold_len < VCP_CMD_TEXT_LEN)
    {
      s_hold[s_hold_len] = (char)byte;
      s_hold_len++;
      return;
    }
    Vcp_FlushHold(byte, 1U);
    return;
  }

  Vcp_RxPush(byte);
}

static void Vcp_Reconnect(void)
{
  /* Stop with the USB IRQ off so the disconnect handler cannot run after
     the new pull-up is on. SysTick stays on, so the delay still runs. */
  NVIC_DisableIRQ(OTG_FS_IRQn);
  (void)USBD_Stop(&hUsbDeviceFS);
  s_live = s_target;
  Vcp_PatchDeviceDesc();
  NVIC_ClearPendingIRQ(OTG_FS_IRQn);
  NVIC_EnableIRQ(OTG_FS_IRQn);

  HAL_Delay(VCP_DISCONNECT_MS);

  NVIC_DisableIRQ(OTG_FS_IRQn);
  (void)USBD_Start(&hUsbDeviceFS);
  s_dirty = 0U;
  s_switch_waiting = 0U;
  NVIC_ClearPendingIRQ(OTG_FS_IRQn);
  NVIC_EnableIRQ(OTG_FS_IRQn);
  dbg_print1("VCP ", (int32_t)s_live);
}

void Vcp_Init(void)
{
#if VCP_BOOT_ENABLE
  s_target = 1U;
#else
  s_target = 0U;
#endif
  s_live = s_target;
  s_dirty = 0U;
  s_active = 0U;
  s_line_state = 0U;
  s_switch_waiting = 0U;
  Vcp_ClearRings();
  Vcp_ResetLineCoding();
  Vcp_PatchDeviceDesc();
}

void Vcp_SetEnabled(bool enabled)
{
  uint8_t next = enabled ? 1U : 0U;

  if (s_target == next)
  {
    return;
  }
  s_target = next;
  s_dirty = 1U;
  s_switch_waiting = 0U;
}

bool Vcp_IsEnabled(void)
{
  return s_target != 0U;
}

bool Vcp_ConfigIsCom(void)
{
  return s_live != 0U;
}

bool Vcp_IsUp(void)
{
  return s_active != 0U;
}

bool Vcp_SwitchPending(void)
{
  return s_dirty != 0U;
}

void Vcp_Poll(void)
{
  uint32_t lock;

  if (s_dirty != 0U)
  {
    if (s_target == s_live)
    {
      s_dirty = 0U;
      s_switch_waiting = 0U;
    }
    else
    {
      if (s_switch_waiting == 0U)
      {
        s_switch_ms = HAL_GetTick();
        s_switch_waiting = 1U;
      }
      if (HidKeyboard_ReleaseAll() || ((HAL_GetTick() - s_switch_ms) >= VCP_RELEASE_WAIT_MS))
      {
        Vcp_Reconnect();
      }
      return;
    }
  }

  lock = Vcp_Lock();
  Vcp_KickTxLocked();
  Vcp_ArmRxLocked();
  Vcp_Unlock(lock);
}

uint16_t Vcp_Write(const uint8_t *data, uint16_t length)
{
  uint16_t stored = 0U;
  uint32_t lock;

  if ((data == NULL) || (length == 0U) || (s_active == 0U))
  {
    return 0U;
  }

  lock = Vcp_Lock();
  while ((stored < length) && (Vcp_TxFree() > 0U))
  {
    s_tx[s_tx_head] = data[stored];
    s_tx_head = Vcp_Next(s_tx_head);
    stored++;
  }
  Vcp_KickTxLocked();
  Vcp_Unlock(lock);
  return stored;
}

uint16_t Vcp_Read(uint8_t *data, uint16_t length)
{
  uint16_t copied = 0U;
  uint32_t lock;

  if ((data == NULL) || (length == 0U))
  {
    return 0U;
  }

  lock = Vcp_Lock();
  while ((copied < length) && (s_rx_tail != s_rx_head))
  {
    data[copied] = s_rx[s_rx_tail];
    s_rx_tail = Vcp_Next(s_rx_tail);
    copied++;
  }
  Vcp_ArmRxLocked();
  Vcp_Unlock(lock);
  return copied;
}

void Vcp_Ep0Cancel(void)
{
  s_line_pending = 0U;
}

uint8_t Vcp_ClassSetup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  uint8_t interface = (uint8_t)(req->wIndex & 0xFFU);

  s_line_pending = 0U;
  if ((s_live == 0U) ||
      ((interface != VCP_COMM_INTERFACE) && (interface != VCP_DATA_INTERFACE)))
  {
    return VCP_SETUP_NOT_MINE;
  }
  if ((req->bmRequest & USB_REQ_TYPE_MASK) != USB_REQ_TYPE_CLASS)
  {
    return VCP_SETUP_NOT_MINE;
  }
  if (interface != VCP_COMM_INTERFACE)
  {
    USBD_CtlError(pdev, req);
    return VCP_SETUP_FAIL;
  }

  switch (req->bRequest)
  {
    case VCP_CDC_SET_LINE_CODING:
      if (req->wLength != sizeof(s_line_rx))
      {
        USBD_CtlError(pdev, req);
        return VCP_SETUP_FAIL;
      }
      s_line_pending = 1U;
      (void)USBD_CtlPrepareRx(pdev, s_line_rx, sizeof(s_line_rx));
      return VCP_SETUP_OK;

    case VCP_CDC_GET_LINE_CODING:
      (void)USBD_CtlSendData(pdev, s_line, (uint16_t)MIN(sizeof(s_line), req->wLength));
      return VCP_SETUP_OK;

    case VCP_CDC_SET_CONTROL_LINE:
      s_line_state = (uint8_t)(req->wValue & 0xFFU);
      if ((s_line_state & 0x01U) != 0U)
      {
        Vcp_SendSerialState(0x0003U);
      }
      else
      {
        Vcp_SendSerialState(0x0000U);
      }
      return VCP_SETUP_OK;

    default:
      USBD_CtlError(pdev, req);
      return VCP_SETUP_FAIL;
  }
}

void Vcp_Ep0RxReady(void)
{
  uint8_t index;

  if (s_line_pending == 0U)
  {
    return;
  }
  s_line_pending = 0U;
  for (index = 0U; index < sizeof(s_line); index++)
  {
    s_line[index] = s_line_rx[index];
  }
}

void Vcp_UsbStart(USBD_HandleTypeDef *pdev)
{
  s_pdev = pdev;
  Vcp_ClearRings();
  s_active = 1U;
  Vcp_ArmRxLocked();
}

void Vcp_UsbStop(void)
{
  s_active = 0U;
  s_pdev = NULL;
  Vcp_ClearRings();
}

void Vcp_OnIn(uint8_t epnum)
{
  uint32_t lock;

  epnum = (uint8_t)(epnum & 0x7FU);
  if (epnum == (VCP_CMD_EP & 0x7FU))
  {
    s_notify_busy = 0U;
    return;
  }
  if (epnum != (VCP_IN_EP & 0x7FU))
  {
    return;
  }

  lock = Vcp_Lock();
  s_tx_inflight = 0U;
  s_tx_len = 0U;
  Vcp_KickTxLocked();
  Vcp_Unlock(lock);
}

void Vcp_OnOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  uint32_t count;
  uint32_t index;
  uint32_t lock;

  if ((uint8_t)(epnum & 0x7FU) != VCP_OUT_EP)
  {
    return;
  }

  count = USBD_LL_GetRxDataSize(pdev, epnum);
  if (count > VCP_DATA_PACKET)
  {
    count = VCP_DATA_PACKET;
  }

  lock = Vcp_Lock();
  s_rx_armed = 0U;
  for (index = 0U; index < count; index++)
  {
    Vcp_Accept(s_rx_pkt[index]);
  }
  Vcp_ArmRxLocked();
  Vcp_Unlock(lock);
}
