#include "hid_keyboard.h"

#include <stdbool.h>
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

#define HID_KB_EPIN_ADDR 0x81U
#define HID_KB_EPIN_SIZE HID_KEYBOARD_REPORT_LEN
#define HID_KB_BINTERVAL 0x01U
#define HID_KB_REPORT_LEN HID_KEYBOARD_REPORT_LEN
#define HID_KB_REPORT_DESC_SIZE 63U
#define HID_KB_CONFIG_DESC_SIZE 59U
#define HID_KB_HID_DESC_SIZE 9U

#define HID_CONS_EPIN_ADDR 0x82U
#define HID_CONS_EPIN_SIZE HID_CONSUMER_REPORT_LEN
#define HID_CONS_BINTERVAL 0x01U
#define HID_CONS_REPORT_LEN HID_CONSUMER_REPORT_LEN

#define HID_KB_REQ_SET_PROTOCOL 0x0BU
#define HID_KB_REQ_GET_PROTOCOL 0x03U
#define HID_KB_REQ_SET_IDLE 0x0AU
#define HID_KB_REQ_GET_IDLE 0x02U
#define HID_KB_REQ_SET_REPORT 0x09U
#define HID_KB_REQ_GET_REPORT 0x01U
#define HID_KB_DESCRIPTOR_TYPE 0x21U
#define HID_KB_REPORT_DESC_TYPE 0x22U

#define HID_KB_IDLE 0U
#define HID_KB_BUSY 1U

typedef struct
{
  volatile uint8_t protocol;
  volatile uint8_t idle_state;
  volatile uint8_t cons_idle;
  volatile uint8_t alt_setting;
  volatile uint8_t state;
  volatile uint8_t cons_state;
} HidKeyboard_ClassData;

static uint8_t HidKeyboard_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t HidKeyboard_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t HidKeyboard_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t HidKeyboard_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t HidKeyboard_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *HidKeyboard_GetHSCfgDesc(uint16_t *length);
static uint8_t *HidKeyboard_GetFSCfgDesc(uint16_t *length);
static uint8_t *HidKeyboard_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *HidKeyboard_GetDeviceQualifierDesc(uint16_t *length);
static void HidKeyboard_PatchInterval(uint8_t *cfg);
static uint8_t HidKeyboard_TransmitEp(uint8_t ep, volatile uint8_t *state,
                                      uint8_t *tx, const uint8_t *report, uint16_t len);
static uint8_t HidKeyboard_Interface(const USBD_SetupReqTypedef *req);

static HidKeyboard_ClassData s_class;
static USBD_HandleTypeDef *s_pdev;
static uint8_t s_tx[HID_KB_REPORT_LEN];
static uint8_t s_last[HID_KB_REPORT_LEN];
static uint8_t s_out[HID_KB_REPORT_LEN];
static uint8_t s_cons_tx[HID_CONS_REPORT_LEN];
static uint8_t s_cons_last[HID_CONS_REPORT_LEN];
static volatile bool s_have_sent;
static volatile bool s_pending;
static volatile bool s_cons_have_sent;
static volatile bool s_cons_pending;
static uint32_t s_last_send_ms;
static uint32_t s_cons_last_send_ms;

__ALIGN_BEGIN static uint8_t s_report_desc[HID_KB_REPORT_DESC_SIZE] __ALIGN_END =
{
  0x05, 0x01, /* Usage Page (Generic Desktop) */
  0x09, 0x06, /* Usage (Keyboard) */
  0xA1, 0x01, /* Collection (Application) */
  0x05, 0x07, /*   Usage Page (Keyboard/Keypad) */
  0x19, 0xE0, /*   Usage Minimum (Left Control) */
  0x29, 0xE7, /*   Usage Maximum (Right GUI) */
  0x15, 0x00, /*   Logical Minimum (0) */
  0x25, 0x01, /*   Logical Maximum (1) */
  0x75, 0x01, /*   Report Size (1) */
  0x95, 0x08, /*   Report Count (8) */
  0x81, 0x02, /*   Input (Data, Var, Abs) */
  0x95, 0x01, /*   Report Count (1) */
  0x75, 0x08, /*   Report Size (8) */
  0x81, 0x01, /*   Input (Const) */
  0x95, 0x05, /*   Report Count (5) */
  0x75, 0x01, /*   Report Size (1) */
  0x05, 0x08, /*   Usage Page (LEDs) */
  0x19, 0x01, /*   Usage Minimum (Num Lock) */
  0x29, 0x05, /*   Usage Maximum (Kana) */
  0x91, 0x02, /*   Output (Data, Var, Abs) */
  0x95, 0x01, /*   Report Count (1) */
  0x75, 0x03, /*   Report Size (3) */
  0x91, 0x01, /*   Output (Const) */
  0x95, 0x06, /*   Report Count (6) */
  0x75, 0x08, /*   Report Size (8) */
  0x15, 0x00, /*   Logical Minimum (0) */
  0x25, 0x65, /*   Logical Maximum (101) */
  0x05, 0x07, /*   Usage Page (Keyboard/Keypad) */
  0x19, 0x00, /*   Usage Minimum (0) */
  0x29, 0x65, /*   Usage Maximum (101) */
  0x81, 0x00, /*   Input (Data, Array) */
  0xC0        /* End Collection */
};

/* Mute, volume down, volume up, calculator. Bit order matches Matrix_FillConsumer. */
__ALIGN_BEGIN static uint8_t s_cons_report_desc[] __ALIGN_END =
{
  0x05, 0x0C,       /* Usage Page (Consumer) */
  0x09, 0x01,       /* Usage (Consumer Control) */
  0xA1, 0x01,       /* Collection (Application) */
  0x15, 0x00,       /*   Logical Minimum (0) */
  0x25, 0x01,       /*   Logical Maximum (1) */
  0x75, 0x01,       /*   Report Size (1) */
  0x95, 0x04,       /*   Report Count (4) */
  0x09, 0xE2,       /*   Usage (Mute) */
  0x09, 0xEA,       /*   Usage (Volume Decrement) */
  0x09, 0xE9,       /*   Usage (Volume Increment) */
  0x0A, 0x92, 0x01, /*   Usage (AL Calculator) */
  0x81, 0x02,       /*   Input (Data, Var, Abs) */
  0x95, 0x04,       /*   Report Count (4) */
  0x81, 0x01,       /*   Input (Const) */
  0xC0              /* End Collection */
};

__ALIGN_BEGIN static uint8_t s_cfg_desc[] __ALIGN_END =
{
  0x09,
  USB_DESC_TYPE_CONFIGURATION,
  (uint8_t)(HID_KB_CONFIG_DESC_SIZE & 0xFFU),
  0x00,
  0x02,
  0x01,
  0x00,
#if (USBD_SELF_POWERED == 1U)
  0xE0,
#else
  0xA0,
#endif
  USBD_MAX_POWER,

  0x09,
  USB_DESC_TYPE_INTERFACE,
  0x00,
  0x00,
  0x01,
  0x03,
  0x01, /* Boot */
  0x01, /* Keyboard */
  0x00,

  0x09,
  HID_KB_DESCRIPTOR_TYPE,
  0x11,
  0x01,
  0x00,
  0x01,
  HID_KB_REPORT_DESC_TYPE,
  HID_KB_REPORT_DESC_SIZE,
  0x00,

  0x07,
  USB_DESC_TYPE_ENDPOINT,
  HID_KB_EPIN_ADDR,
  0x03,
  HID_KB_EPIN_SIZE,
  0x00,
  HID_KB_BINTERVAL,

  0x09,
  USB_DESC_TYPE_INTERFACE,
  0x01,
  0x00,
  0x01,
  0x03,
  0x00,
  0x00,
  0x00,

  0x09,
  HID_KB_DESCRIPTOR_TYPE,
  0x11,
  0x01,
  0x00,
  0x01,
  HID_KB_REPORT_DESC_TYPE,
  (uint8_t)sizeof(s_cons_report_desc),
  (uint8_t)(sizeof(s_cons_report_desc) >> 8),

  0x07,
  USB_DESC_TYPE_ENDPOINT,
  HID_CONS_EPIN_ADDR,
  0x03,
  HID_CONS_EPIN_SIZE,
  0x00,
  HID_CONS_BINTERVAL,
};

__ALIGN_BEGIN static uint8_t s_hid_desc[HID_KB_HID_DESC_SIZE] __ALIGN_END =
{
  0x09,
  HID_KB_DESCRIPTOR_TYPE,
  0x11,
  0x01,
  0x00,
  0x01,
  HID_KB_REPORT_DESC_TYPE,
  HID_KB_REPORT_DESC_SIZE,
  0x00,
};

__ALIGN_BEGIN static uint8_t s_cons_hid_desc[HID_KB_HID_DESC_SIZE] __ALIGN_END =
{
  0x09,
  HID_KB_DESCRIPTOR_TYPE,
  0x11,
  0x01,
  0x00,
  0x01,
  HID_KB_REPORT_DESC_TYPE,
  (uint8_t)sizeof(s_cons_report_desc),
  (uint8_t)(sizeof(s_cons_report_desc) >> 8),
};

__ALIGN_BEGIN static uint8_t s_qualifier_desc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};

_Static_assert(sizeof(s_report_desc) == HID_KB_REPORT_DESC_SIZE, "keyboard report descriptor size");
_Static_assert(sizeof(s_cfg_desc) == HID_KB_CONFIG_DESC_SIZE, "keyboard config descriptor size");
_Static_assert(sizeof(s_cons_report_desc) < 256U, "consumer report descriptor length");
_Static_assert(HID_KB_EPIN_SIZE == 8U, "boot keyboard report is 8 bytes");
_Static_assert(HID_CONS_EPIN_SIZE == 1U, "consumer report is 1 byte");

static void HidKeyboard_PatchInterval(uint8_t *cfg)
{
  USBD_EpDescTypeDef *ep = (USBD_EpDescTypeDef *)USBD_GetEpDesc(cfg, HID_KB_EPIN_ADDR);

  if (ep != NULL)
  {
    ep->bInterval = HID_KB_BINTERVAL;
  }
}

static uint8_t *HidKeyboard_GetFSCfgDesc(uint16_t *length)
{
  HidKeyboard_PatchInterval(s_cfg_desc);
  *length = (uint16_t)sizeof(s_cfg_desc);
  return s_cfg_desc;
}

static uint8_t *HidKeyboard_GetHSCfgDesc(uint16_t *length)
{
  return HidKeyboard_GetFSCfgDesc(length);
}

static uint8_t *HidKeyboard_GetOtherSpeedCfgDesc(uint16_t *length)
{
  return HidKeyboard_GetFSCfgDesc(length);
}

static uint8_t *HidKeyboard_GetDeviceQualifierDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(s_qualifier_desc);
  return s_qualifier_desc;
}

static uint8_t HidKeyboard_Interface(const USBD_SetupReqTypedef *req)
{
  return (uint8_t)(req->wIndex & 0xFFU);
}

static void HidKeyboard_PrepareFifos(USBD_HandleTypeDef *pdev)
{
  PCD_HandleTypeDef *pcd = (PCD_HandleTypeDef *)pdev->pData;

  /* OTG FS has 320 FIFO words. CubeMX assigns RX 128, EP0 TX 64, and EP1 TX 128.
     Give 64 of EP1's words to EP2 so the consumer report can transmit. */
  (void)HAL_PCDEx_SetTxFiFo(pcd, 1, 0x40);
  (void)HAL_PCDEx_SetTxFiFo(pcd, 2, 0x40);
}

static uint8_t HidKeyboard_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

  s_class.protocol = 1U;
  s_class.idle_state = 0U;
  s_class.cons_idle = 0U;
  s_class.alt_setting = 0U;
  s_class.state = HID_KB_IDLE;
  s_class.cons_state = HID_KB_IDLE;
  s_pdev = pdev;
  s_have_sent = false;
  s_pending = true;
  s_cons_have_sent = false;
  s_cons_pending = true;

  pdev->pClassDataCmsit[pdev->classId] = &s_class;
  pdev->pClassData = &s_class;

  HidKeyboard_PrepareFifos(pdev);

  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    pdev->ep_in[HID_KB_EPIN_ADDR & 0xFU].bInterval = HID_KB_BINTERVAL;
  }
  else
  {
    pdev->ep_in[HID_KB_EPIN_ADDR & 0xFU].bInterval = HID_KB_BINTERVAL;
  }
  pdev->ep_in[HID_CONS_EPIN_ADDR & 0xFU].bInterval = HID_CONS_BINTERVAL;

  (void)USBD_LL_OpenEP(pdev, HID_KB_EPIN_ADDR, USBD_EP_TYPE_INTR, HID_KB_EPIN_SIZE);
  pdev->ep_in[HID_KB_EPIN_ADDR & 0xFU].is_used = 1U;
  (void)USBD_LL_OpenEP(pdev, HID_CONS_EPIN_ADDR, USBD_EP_TYPE_INTR, HID_CONS_EPIN_SIZE);
  pdev->ep_in[HID_CONS_EPIN_ADDR & 0xFU].is_used = 1U;
  return (uint8_t)USBD_OK;
}

static uint8_t HidKeyboard_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

  (void)USBD_LL_CloseEP(pdev, HID_KB_EPIN_ADDR);
  (void)USBD_LL_CloseEP(pdev, HID_CONS_EPIN_ADDR);
  pdev->ep_in[HID_KB_EPIN_ADDR & 0xFU].is_used = 0U;
  pdev->ep_in[HID_KB_EPIN_ADDR & 0xFU].bInterval = 0U;
  pdev->ep_in[HID_CONS_EPIN_ADDR & 0xFU].is_used = 0U;
  pdev->ep_in[HID_CONS_EPIN_ADDR & 0xFU].bInterval = 0U;
  pdev->pClassDataCmsit[pdev->classId] = NULL;
  pdev->pClassData = NULL;
  s_pdev = NULL;
  s_class.state = HID_KB_IDLE;
  s_class.cons_state = HID_KB_IDLE;
  return (uint8_t)USBD_OK;
}

static uint8_t HidKeyboard_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  HidKeyboard_ClassData *data = (HidKeyboard_ClassData *)pdev->pClassDataCmsit[pdev->classId];
  USBD_StatusTypeDef ret = USBD_OK;
  uint16_t len;
  uint8_t *buf;
  uint16_t status_info = 0U;

  if (data == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_CLASS:
      switch (req->bRequest)
      {
        case HID_KB_REQ_SET_PROTOCOL:
          if (HidKeyboard_Interface(req) == 0U)
          {
            data->protocol = (uint8_t)req->wValue;
          }
          break;

        case HID_KB_REQ_GET_PROTOCOL:
          (void)USBD_CtlSendData(pdev, (uint8_t *)&data->protocol, 1U);
          break;

        case HID_KB_REQ_SET_IDLE:
          if (HidKeyboard_Interface(req) == 1U)
          {
            data->cons_idle = (uint8_t)(req->wValue >> 8);
          }
          else
          {
            data->idle_state = (uint8_t)(req->wValue >> 8);
          }
          break;

        case HID_KB_REQ_GET_IDLE:
          if (HidKeyboard_Interface(req) == 1U)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&data->cons_idle, 1U);
          }
          else
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&data->idle_state, 1U);
          }
          break;

        case HID_KB_REQ_GET_REPORT:
          if (HidKeyboard_Interface(req) == 1U)
          {
            (void)USBD_CtlSendData(pdev, s_cons_last, (uint16_t)MIN(HID_CONS_REPORT_LEN, req->wLength));
          }
          else
          {
            (void)USBD_CtlSendData(pdev, s_last, (uint16_t)MIN(HID_KB_REPORT_LEN, req->wLength));
          }
          break;

        case HID_KB_REQ_SET_REPORT:
          if (HidKeyboard_Interface(req) != 0U)
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
          }
          if (req->wLength == 0U)
          {
            break;
          }
          if (req->wLength > sizeof(s_out))
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
          }
          (void)USBD_CtlPrepareRx(pdev, s_out, req->wLength);
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_STATUS:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_GET_DESCRIPTOR:
          if (HidKeyboard_Interface(req) == 1U)
          {
            if ((req->wValue >> 8) == HID_KB_REPORT_DESC_TYPE)
            {
              len = (uint16_t)MIN(sizeof(s_cons_report_desc), req->wLength);
              buf = s_cons_report_desc;
            }
            else if ((req->wValue >> 8) == HID_KB_DESCRIPTOR_TYPE)
            {
              len = (uint16_t)MIN(sizeof(s_cons_hid_desc), req->wLength);
              buf = s_cons_hid_desc;
            }
            else
            {
              USBD_CtlError(pdev, req);
              ret = USBD_FAIL;
              break;
            }
          }
          else if ((req->wValue >> 8) == HID_KB_REPORT_DESC_TYPE)
          {
            len = (uint16_t)MIN(sizeof(s_report_desc), req->wLength);
            buf = s_report_desc;
          }
          else if ((req->wValue >> 8) == HID_KB_DESCRIPTOR_TYPE)
          {
            len = (uint16_t)MIN(sizeof(s_hid_desc), req->wLength);
            buf = s_hid_desc;
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
          }
          (void)USBD_CtlSendData(pdev, buf, len);
          break;

        case USB_REQ_GET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&data->alt_setting, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            data->alt_setting = (uint8_t)req->wValue;
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    default:
      USBD_CtlError(pdev, req);
      ret = USBD_FAIL;
      break;
  }

  return (uint8_t)ret;
}

static uint8_t HidKeyboard_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
  UNUSED(pdev);
  return (uint8_t)USBD_OK;
}

static uint8_t HidKeyboard_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  HidKeyboard_ClassData *data;

  data = (HidKeyboard_ClassData *)pdev->pClassDataCmsit[pdev->classId];
  if (data != NULL)
  {
    if ((epnum & 0x7FU) == (HID_CONS_EPIN_ADDR & 0x7FU))
    {
      data->cons_state = HID_KB_IDLE;
    }
    else
    {
      data->state = HID_KB_IDLE;
    }
  }
  return (uint8_t)USBD_OK;
}

static uint8_t HidKeyboard_TransmitEp(uint8_t ep, volatile uint8_t *state,
                                      uint8_t *tx, const uint8_t *report, uint16_t len)
{
  USBD_HandleTypeDef *pdev;
  HidKeyboard_ClassData *data;
  uint32_t primask;
  uint16_t index;

  primask = __get_PRIMASK();
  __disable_irq();
  pdev = s_pdev;
  data = (pdev == NULL) ? NULL : (HidKeyboard_ClassData *)pdev->pClassDataCmsit[pdev->classId];
  if ((pdev == NULL) || (data == NULL) ||
      (pdev->dev_state != USBD_STATE_CONFIGURED) ||
      (*state != HID_KB_IDLE))
  {
    __set_PRIMASK(primask);
    return (uint8_t)USBD_BUSY;
  }
  *state = HID_KB_BUSY;
  __set_PRIMASK(primask);

  for (index = 0U; index < len; index++)
  {
    tx[index] = report[index];
  }

  if (USBD_LL_Transmit(pdev, ep, tx, len) != USBD_OK)
  {
    *state = HID_KB_IDLE;
    return (uint8_t)USBD_BUSY;
  }
  return (uint8_t)USBD_OK;
}

static uint8_t HidKeyboard_Transmit(const uint8_t report[HID_KB_REPORT_LEN])
{
  return HidKeyboard_TransmitEp(HID_KB_EPIN_ADDR, &s_class.state, s_tx, report, HID_KB_REPORT_LEN);
}

void HidKeyboard_SendIfChanged(const uint8_t report[HID_KB_REPORT_LEN])
{
  uint8_t index;
  bool different = (s_have_sent == false);

  if (different == false)
  {
    for (index = 0U; index < HID_KB_REPORT_LEN; index++)
    {
      if (report[index] != s_last[index])
      {
        different = true;
        break;
      }
    }
  }

  if (different || s_pending)
  {
    if (HidKeyboard_Transmit(report) == (uint8_t)USBD_OK)
    {
      for (index = 0U; index < HID_KB_REPORT_LEN; index++)
      {
        s_last[index] = report[index];
      }
      s_have_sent = true;
      s_pending = false;
      s_last_send_ms = HAL_GetTick();
    }
    else
    {
      s_pending = true;
    }
    return;
  }

  if ((s_class.idle_state != 0U) &&
      ((HAL_GetTick() - s_last_send_ms) >= ((uint32_t)s_class.idle_state * 4U)))
  {
    if (HidKeyboard_Transmit(s_last) == (uint8_t)USBD_OK)
    {
      s_last_send_ms = HAL_GetTick();
    }
  }
}

void HidKeyboard_SendConsumerIfChanged(const uint8_t report[HID_CONS_REPORT_LEN])
{
  bool different = (s_cons_have_sent == false);

  if ((different == false) && (report[0] != s_cons_last[0]))
  {
    different = true;
  }

  if (different || s_cons_pending)
  {
    if (HidKeyboard_TransmitEp(HID_CONS_EPIN_ADDR, &s_class.cons_state,
                              s_cons_tx, report, HID_CONS_REPORT_LEN) == (uint8_t)USBD_OK)
    {
      s_cons_last[0] = report[0];
      s_cons_have_sent = true;
      s_cons_pending = false;
      s_cons_last_send_ms = HAL_GetTick();
    }
    else
    {
      s_cons_pending = true;
    }
    return;
  }

  if ((s_class.cons_idle != 0U) &&
      ((HAL_GetTick() - s_cons_last_send_ms) >= ((uint32_t)s_class.cons_idle * 4U)))
  {
    if (HidKeyboard_TransmitEp(HID_CONS_EPIN_ADDR, &s_class.cons_state,
                              s_cons_tx, s_cons_last, HID_CONS_REPORT_LEN) == (uint8_t)USBD_OK)
    {
      s_cons_last_send_ms = HAL_GetTick();
    }
  }
}

USBD_ClassTypeDef USBD_HID_Keyboard =
{
  HidKeyboard_Init,
  HidKeyboard_DeInit,
  HidKeyboard_Setup,
  NULL,
  HidKeyboard_EP0_RxReady,
  HidKeyboard_DataIn,
  NULL,
  NULL,
  NULL,
  NULL,
  HidKeyboard_GetHSCfgDesc,
  HidKeyboard_GetFSCfgDesc,
  HidKeyboard_GetOtherSpeedCfgDesc,
  HidKeyboard_GetDeviceQualifierDesc,
};
