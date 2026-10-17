#include "hid_keyboard.h"

#include <stdbool.h>
#include "led.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"
#include "vcp.h"

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
#define HID_KB_EP0_NONE 0U
#define HID_KB_EP0_FEATURE 1U
#define HID_KB_FEATURE_LEN 8U
#define HID_VCP_CONFIG_DESC_SIZE 100U
#define HID_RID_KEYBOARD 0x01U
#define HID_RID_CONSUMER 0x02U
#define HID_KB_WIRE_MAX (HID_KEYBOARD_REPORT_LEN + 1U)
#define HID_CONS_WIRE_MAX (HID_CONSUMER_REPORT_LEN + 1U)
#define HID_VCP_HID_EP_SIZE HID_KB_WIRE_MAX

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
static uint8_t HidKeyboard_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
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
static uint8_t s_tx[HID_KB_WIRE_MAX];
static uint8_t s_last[HID_KB_REPORT_LEN];
static uint8_t s_out[HID_KB_REPORT_LEN];
static uint8_t s_cons_tx[HID_CONS_WIRE_MAX];
static uint8_t s_ep1_kind;
static uint8_t s_cons_last[HID_CONS_REPORT_LEN];
static volatile bool s_have_sent;
static volatile bool s_pending;
static volatile bool s_cons_have_sent;
static volatile bool s_cons_pending;
static uint32_t s_last_send_ms;
static uint32_t s_cons_last_send_ms;
static uint8_t s_ep0_kind;
static uint8_t s_release_phase;
static uint8_t s_feature_rx[HID_KB_FEATURE_LEN];
static uint8_t s_feature_tx[HID_KB_FEATURE_LEN];
static uint8_t s_feature_id;
static uint16_t s_feature_len;

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
  0x06, 0x00, 0xFF, /*   Usage Page (Vendor 0xFF00) */
  0x09, 0x01,       /*   Usage (Vendor 1) */
  0x15, 0x00,       /*   Logical Minimum (0) */
  0x26, 0xFF, 0x00, /*   Logical Maximum (255) */
  0x75, 0x08,       /*   Report Size (8) */
  0x95, LED_FEATURE_LEN, /* Report Count (6): command, enable, R, G, B, brightness */
  0xB1, 0x02,       /*   Feature (Data, Var, Abs) */
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

/* Keyboard report ID 1 and consumer report ID 2 share endpoint 0x81 while
   the COM port is up. OTG FS has no fourth IN endpoint for a separate
   consumer interface beside CDC. */
__ALIGN_BEGIN static uint8_t s_vcp_report_desc[] __ALIGN_END =
{
  0x05, 0x01, /* Usage Page (Generic Desktop) */
  0x09, 0x06, /* Usage (Keyboard) */
  0xA1, 0x01, /* Collection (Application) */
  0x85, HID_RID_KEYBOARD, /* Report ID (1) */
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
  0xC0,       /* End Collection */
  0x05, 0x0C,       /* Usage Page (Consumer) */
  0x09, 0x01,       /* Usage (Consumer Control) */
  0xA1, 0x01,       /* Collection (Application) */
  0x85, HID_RID_CONSUMER, /* Report ID (2) */
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
  0x85, LED_FEATURE_REPORT_ID, /* Report ID (3) */
  0x06, 0x00, 0xFF, /*   Usage Page (Vendor 0xFF00) */
  0x09, 0x01,       /*   Usage (Vendor 1) */
  0x15, 0x00,       /*   Logical Minimum (0) */
  0x26, 0xFF, 0x00, /*   Logical Maximum (255) */
  0x75, 0x08,       /*   Report Size (8) */
  0x95, LED_FEATURE_LEN, /* Report Count (6): command, enable, R, G, B, brightness */
  0xB1, 0x02,       /*   Feature (Data, Var, Abs) */
  0xC0              /* End Collection */
};

__ALIGN_BEGIN static uint8_t s_vcp_cfg[] __ALIGN_END =
{
  0x09,
  USB_DESC_TYPE_CONFIGURATION,
  (uint8_t)(HID_VCP_CONFIG_DESC_SIZE & 0xFFU),
  (uint8_t)(HID_VCP_CONFIG_DESC_SIZE >> 8),
  0x03,
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
  (uint8_t)sizeof(s_vcp_report_desc),
  (uint8_t)(sizeof(s_vcp_report_desc) >> 8),

  0x07,
  USB_DESC_TYPE_ENDPOINT,
  HID_KB_EPIN_ADDR,
  0x03,
  HID_VCP_HID_EP_SIZE,
  0x00,
  HID_KB_BINTERVAL,

  0x08,
  USB_DESC_TYPE_IAD,
  VCP_COMM_INTERFACE,
  0x02,
  0x02,
  0x02,
  0x01,
  0x00,

  0x09,
  USB_DESC_TYPE_INTERFACE,
  VCP_COMM_INTERFACE,
  0x00,
  0x01,
  0x02,
  0x02,
  0x01,
  0x00,

  0x05,
  0x24,
  0x00,
  0x10,
  0x01,

  0x05,
  0x24,
  0x01,
  0x00,
  VCP_DATA_INTERFACE,

  0x04,
  0x24,
  0x02,
  0x02,

  0x05,
  0x24,
  0x06,
  VCP_COMM_INTERFACE,
  VCP_DATA_INTERFACE,

  0x07,
  USB_DESC_TYPE_ENDPOINT,
  VCP_CMD_EP,
  0x03,
  VCP_CMD_PACKET,
  0x00,
  0x10,

  0x09,
  USB_DESC_TYPE_INTERFACE,
  VCP_DATA_INTERFACE,
  0x00,
  0x02,
  0x0A,
  0x00,
  0x00,
  0x00,

  0x07,
  USB_DESC_TYPE_ENDPOINT,
  VCP_OUT_EP,
  0x02,
  VCP_DATA_PACKET,
  0x00,
  0x00,

  0x07,
  USB_DESC_TYPE_ENDPOINT,
  VCP_IN_EP,
  0x02,
  VCP_DATA_PACKET,
  0x00,
  0x00,
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

__ALIGN_BEGIN static uint8_t s_vcp_hid_desc[HID_KB_HID_DESC_SIZE] __ALIGN_END =
{
  0x09,
  HID_KB_DESCRIPTOR_TYPE,
  0x11,
  0x01,
  0x00,
  0x01,
  HID_KB_REPORT_DESC_TYPE,
  (uint8_t)sizeof(s_vcp_report_desc),
  (uint8_t)(sizeof(s_vcp_report_desc) >> 8),
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
_Static_assert(sizeof(s_vcp_cfg) == HID_VCP_CONFIG_DESC_SIZE, "vcp config descriptor size");
_Static_assert(sizeof(s_vcp_report_desc) < 256U, "vcp report descriptor length");
_Static_assert(sizeof(s_cons_report_desc) < 256U, "consumer report descriptor length");
_Static_assert(LED_FEATURE_LEN == 6U, "led feature payload");
_Static_assert(HID_KB_FEATURE_LEN >= (LED_FEATURE_LEN + 1U), "feature buffer holds a report id");
_Static_assert(HID_KB_EPIN_SIZE == 8U, "boot keyboard report is 8 bytes");
_Static_assert(HID_VCP_HID_EP_SIZE == 9U, "shared hid packet holds a report id plus 8 bytes");
_Static_assert(HID_CONS_EPIN_SIZE == 1U, "consumer report is 1 byte");
_Static_assert(VCP_CMD_EP == HID_CONS_EPIN_ADDR, "CDC notification reuses the consumer endpoint");

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
  if (Vcp_ConfigIsCom())
  {
    HidKeyboard_PatchInterval(s_vcp_cfg);
    *length = (uint16_t)sizeof(s_vcp_cfg);
    return s_vcp_cfg;
  }
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
  if (Vcp_ConfigIsCom())
  {
    s_qualifier_desc[4] = 0xEFU;
    s_qualifier_desc[5] = 0x02U;
    s_qualifier_desc[6] = 0x01U;
  }
  else
  {
    s_qualifier_desc[4] = 0x00U;
    s_qualifier_desc[5] = 0x00U;
    s_qualifier_desc[6] = 0x00U;
  }
  *length = (uint16_t)sizeof(s_qualifier_desc);
  return s_qualifier_desc;
}

static uint8_t HidKeyboard_Interface(const USBD_SetupReqTypedef *req)
{
  return (uint8_t)(req->wIndex & 0xFFU);
}

static bool HidKeyboard_IsVendorFeature(const USBD_SetupReqTypedef *req)
{
  uint8_t interface_num = HidKeyboard_Interface(req);
  uint8_t report_type = (uint8_t)(req->wValue >> 8);
  uint8_t report_id = (uint8_t)(req->wValue & 0xFFU);

  if (report_type != 0x03U)
  {
    return false;
  }
  if ((interface_num == 1U) && (Vcp_ConfigIsCom() == false) && (report_id == 0U))
  {
    return true;
  }
  return (interface_num == 0U) && (Vcp_ConfigIsCom() != false) && (report_id == LED_FEATURE_REPORT_ID);
}

static void HidKeyboard_LoadFeature(uint8_t report_id)
{
  uint8_t enabled = 0U;
  uint8_t red = 0U;
  uint8_t green = 0U;
  uint8_t blue = 0U;
  uint8_t brightness = 0U;
  uint8_t index = 0U;

  Led_GetSolid(&enabled, &red, &green, &blue, &brightness);
  if (report_id != 0U)
  {
    s_feature_tx[index++] = report_id;
  }
  s_feature_tx[index++] = Vcp_IsEnabled() ? 1U : 0U;
  s_feature_tx[index++] = enabled;
  s_feature_tx[index++] = red;
  s_feature_tx[index++] = green;
  s_feature_tx[index++] = blue;
  s_feature_tx[index++] = brightness;
  while (index < sizeof(s_feature_tx))
  {
    s_feature_tx[index++] = 0U;
  }
}

static void HidKeyboard_ApplyFeature(void)
{
  const uint8_t *payload = s_feature_rx;
  uint16_t len = s_feature_len;

  if ((s_feature_id != 0U) && (len > 0U) && (payload[0] == s_feature_id))
  {
    payload++;
    len--;
  }
  if (len == 0U)
  {
    return;
  }
  if ((payload[0] == LED_CMD_SOLID) && (len >= LED_FEATURE_LEN))
  {
    Led_SetSolid(payload[1], payload[2], payload[3], payload[4], payload[5]);
    return;
  }
  if (payload[0] != LED_CMD_SOLID)
  {
    Vcp_SetEnabled(payload[0] != 0U);
  }
}

static void HidKeyboard_PrepareFifos(USBD_HandleTypeDef *pdev)
{
  PCD_HandleTypeDef *pcd = (PCD_HandleTypeDef *)pdev->pData;

  /* OTG FS has 320 FIFO words. An unused higher IN FIFO is set to 0.
     A used IN FIFO stays at least 16 words. */
  (void)HAL_PCDEx_SetRxFiFo(pcd, 0x80);
  (void)HAL_PCDEx_SetTxFiFo(pcd, 0, 0x40);
  if (Vcp_ConfigIsCom())
  {
    (void)HAL_PCDEx_SetTxFiFo(pcd, 1, 0x10);
    (void)HAL_PCDEx_SetTxFiFo(pcd, 2, 0x10);
    (void)HAL_PCDEx_SetTxFiFo(pcd, 3, 0x40);
  }
  else
  {
    (void)HAL_PCDEx_SetTxFiFo(pcd, 1, 0x40);
    (void)HAL_PCDEx_SetTxFiFo(pcd, 2, 0x40);
    (void)HAL_PCDEx_SetTxFiFo(pcd, 3, 0x00);
  }
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
  s_ep0_kind = HID_KB_EP0_NONE;
  s_release_phase = 0U;

  pdev->pClassDataCmsit[pdev->classId] = &s_class;
  pdev->pClassData = &s_class;

  HidKeyboard_PrepareFifos(pdev);
  pdev->ep_in[HID_KB_EPIN_ADDR & 0xFU].bInterval = HID_KB_BINTERVAL;
  (void)USBD_LL_OpenEP(pdev, HID_KB_EPIN_ADDR, USBD_EP_TYPE_INTR,
                       Vcp_ConfigIsCom() ? HID_VCP_HID_EP_SIZE : HID_KB_EPIN_SIZE);
  pdev->ep_in[HID_KB_EPIN_ADDR & 0xFU].is_used = 1U;

  if (Vcp_ConfigIsCom())
  {
    pdev->ep_in[VCP_CMD_EP & 0xFU].bInterval = 0x10U;
    (void)USBD_LL_OpenEP(pdev, VCP_CMD_EP, USBD_EP_TYPE_INTR, VCP_CMD_PACKET);
    pdev->ep_in[VCP_CMD_EP & 0xFU].is_used = 1U;
    (void)USBD_LL_OpenEP(pdev, VCP_IN_EP, USBD_EP_TYPE_BULK, VCP_DATA_PACKET);
    pdev->ep_in[VCP_IN_EP & 0xFU].is_used = 1U;
    (void)USBD_LL_OpenEP(pdev, VCP_OUT_EP, USBD_EP_TYPE_BULK, VCP_DATA_PACKET);
    pdev->ep_out[VCP_OUT_EP & 0xFU].is_used = 1U;
    Vcp_UsbStart(pdev);
  }
  else
  {
    pdev->ep_in[HID_CONS_EPIN_ADDR & 0xFU].bInterval = HID_CONS_BINTERVAL;
    (void)USBD_LL_OpenEP(pdev, HID_CONS_EPIN_ADDR, USBD_EP_TYPE_INTR, HID_CONS_EPIN_SIZE);
    pdev->ep_in[HID_CONS_EPIN_ADDR & 0xFU].is_used = 1U;
    Vcp_UsbStop();
  }
  return (uint8_t)USBD_OK;
}

static uint8_t HidKeyboard_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

  Vcp_UsbStop();
  (void)USBD_LL_CloseEP(pdev, HID_KB_EPIN_ADDR);
  (void)USBD_LL_CloseEP(pdev, HID_CONS_EPIN_ADDR);
  (void)USBD_LL_CloseEP(pdev, VCP_IN_EP);
  (void)USBD_LL_CloseEP(pdev, VCP_OUT_EP);
  pdev->ep_in[HID_KB_EPIN_ADDR & 0xFU].is_used = 0U;
  pdev->ep_in[HID_KB_EPIN_ADDR & 0xFU].bInterval = 0U;
  pdev->ep_in[HID_CONS_EPIN_ADDR & 0xFU].is_used = 0U;
  pdev->ep_in[HID_CONS_EPIN_ADDR & 0xFU].bInterval = 0U;
  pdev->ep_in[VCP_IN_EP & 0xFU].is_used = 0U;
  pdev->ep_out[VCP_OUT_EP & 0xFU].is_used = 0U;
  pdev->pClassDataCmsit[pdev->classId] = NULL;
  pdev->pClassData = NULL;
  s_pdev = NULL;
  s_release_phase = 0U;
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

  s_ep0_kind = HID_KB_EP0_NONE;
  Vcp_Ep0Cancel();

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_CLASS:
    {
      uint8_t vcp_setup = Vcp_ClassSetup(pdev, req);

      if (vcp_setup == VCP_SETUP_OK)
      {
        return (uint8_t)USBD_OK;
      }
      if (vcp_setup == VCP_SETUP_FAIL)
      {
        return (uint8_t)USBD_FAIL;
      }
    }
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
          if (HidKeyboard_IsVendorFeature(req))
          {
            uint8_t report_id = (uint8_t)(req->wValue & 0xFFU);
            uint16_t len = (report_id == 0U) ? LED_FEATURE_LEN : (uint16_t)(LED_FEATURE_LEN + 1U);

            HidKeyboard_LoadFeature(report_id);
            (void)USBD_CtlSendData(pdev, s_feature_tx, (uint16_t)MIN(len, req->wLength));
            break;
          }
          if ((HidKeyboard_Interface(req) == 1U) && (Vcp_ConfigIsCom() == false))
          {
            (void)USBD_CtlSendData(pdev, s_cons_last, (uint16_t)MIN(HID_CONS_REPORT_LEN, req->wLength));
          }
          else if (HidKeyboard_Interface(req) == 0U)
          {
            if ((Vcp_ConfigIsCom() != false) && ((req->wValue & 0xFFU) == HID_RID_CONSUMER))
            {
              (void)USBD_CtlSendData(pdev, s_cons_last, (uint16_t)MIN(HID_CONS_REPORT_LEN, req->wLength));
            }
            else
            {
              (void)USBD_CtlSendData(pdev, s_last, (uint16_t)MIN(HID_KB_REPORT_LEN, req->wLength));
            }
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case HID_KB_REQ_SET_REPORT:
          if (HidKeyboard_IsVendorFeature(req))
          {
            if ((req->wLength == 0U) || (req->wLength > sizeof(s_feature_rx)))
            {
              USBD_CtlError(pdev, req);
              ret = USBD_FAIL;
              break;
            }
            s_ep0_kind = HID_KB_EP0_FEATURE;
            s_feature_id = (uint8_t)(req->wValue & 0xFFU);
            s_feature_len = req->wLength;
            (void)USBD_CtlPrepareRx(pdev, s_feature_rx, req->wLength);
            break;
          }
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
          if ((HidKeyboard_Interface(req) == 1U) && (Vcp_ConfigIsCom() == false))
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
          else if ((HidKeyboard_Interface(req) == 0U) &&
                   ((req->wValue >> 8) == HID_KB_REPORT_DESC_TYPE))
          {
            if (Vcp_ConfigIsCom() != false)
            {
              len = (uint16_t)MIN(sizeof(s_vcp_report_desc), req->wLength);
              buf = s_vcp_report_desc;
            }
            else
            {
              len = (uint16_t)MIN(sizeof(s_report_desc), req->wLength);
              buf = s_report_desc;
            }
          }
          else if ((HidKeyboard_Interface(req) == 0U) &&
                   ((req->wValue >> 8) == HID_KB_DESCRIPTOR_TYPE))
          {
            if (Vcp_ConfigIsCom() != false)
            {
              len = (uint16_t)MIN(sizeof(s_vcp_hid_desc), req->wLength);
              buf = s_vcp_hid_desc;
            }
            else
            {
              len = (uint16_t)MIN(sizeof(s_hid_desc), req->wLength);
              buf = s_hid_desc;
            }
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
  if (s_ep0_kind == HID_KB_EP0_FEATURE)
  {
    s_ep0_kind = HID_KB_EP0_NONE;
    HidKeyboard_ApplyFeature();
  }
  Vcp_Ep0RxReady();
  return (uint8_t)USBD_OK;
}

static uint8_t HidKeyboard_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  HidKeyboard_ClassData *data;

  data = (HidKeyboard_ClassData *)pdev->pClassDataCmsit[pdev->classId];
  if (data != NULL)
  {
    if ((epnum & 0x7FU) == (HID_KB_EPIN_ADDR & 0x7FU))
    {
      if ((Vcp_ConfigIsCom() != false) && (s_ep1_kind == 1U))
      {
        data->cons_state = HID_KB_IDLE;
      }
      else
      {
        data->state = HID_KB_IDLE;
      }
    }
    else if ((Vcp_ConfigIsCom() == false) &&
             ((epnum & 0x7FU) == (HID_CONS_EPIN_ADDR & 0x7FU)))
    {
      data->cons_state = HID_KB_IDLE;
    }
    else
    {
      Vcp_OnIn(epnum);
    }
  }
  return (uint8_t)USBD_OK;
}

static uint8_t HidKeyboard_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  if (Vcp_ConfigIsCom())
  {
    Vcp_OnOut(pdev, epnum);
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
      (*state != HID_KB_IDLE) ||
      ((Vcp_ConfigIsCom() != false) &&
       ((s_class.state != HID_KB_IDLE) || (s_class.cons_state != HID_KB_IDLE))))
  {
    __set_PRIMASK(primask);
    return (uint8_t)USBD_BUSY;
  }
  *state = HID_KB_BUSY;
  if ((ep & 0x7FU) == (HID_KB_EPIN_ADDR & 0x7FU))
  {
    s_ep1_kind = (state == &s_class.cons_state) ? 1U : 0U;
  }
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
  uint8_t wire[HID_KB_WIRE_MAX];
  const uint8_t *src = report;
  uint16_t len = HID_KB_REPORT_LEN;
  uint8_t index;

  if (Vcp_ConfigIsCom() != false)
  {
    wire[0] = HID_RID_KEYBOARD;
    for (index = 0U; index < HID_KB_REPORT_LEN; index++)
    {
      wire[index + 1U] = report[index];
    }
    src = wire;
    len = HID_KB_WIRE_MAX;
  }
  return HidKeyboard_TransmitEp(HID_KB_EPIN_ADDR, &s_class.state, s_tx, src, len);
}

static uint8_t HidKeyboard_TransmitConsumer(const uint8_t report[HID_CONS_REPORT_LEN])
{
  uint8_t wire[HID_CONS_WIRE_MAX];
  uint8_t ep = HID_CONS_EPIN_ADDR;
  const uint8_t *src = report;
  uint16_t len = HID_CONS_REPORT_LEN;

  if (Vcp_ConfigIsCom() != false)
  {
    wire[0] = HID_RID_CONSUMER;
    wire[1] = report[0];
    src = wire;
    ep = HID_KB_EPIN_ADDR;
    len = HID_CONS_WIRE_MAX;
  }
  return HidKeyboard_TransmitEp(ep, &s_class.cons_state, s_cons_tx, src, len);
}

void HidKeyboard_SendIfChanged(const uint8_t report[HID_KB_REPORT_LEN])
{
  uint8_t index;
  bool different = (s_have_sent == false);

  if (Vcp_SwitchPending())
  {
    return;
  }

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

  if (Vcp_SwitchPending())
  {
    return;
  }

  if ((different == false) && (report[0] != s_cons_last[0]))
  {
    different = true;
  }

  if (different || s_cons_pending)
  {
    if (HidKeyboard_TransmitConsumer(report) == (uint8_t)USBD_OK)
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
    if (HidKeyboard_TransmitConsumer(s_cons_last) == (uint8_t)USBD_OK)
    {
      s_cons_last_send_ms = HAL_GetTick();
    }
  }
}

bool HidKeyboard_ReleaseAll(void)
{
  static const uint8_t key_zero[HID_KB_REPORT_LEN] = {0};
  static const uint8_t cons_zero[HID_CONS_REPORT_LEN] = {0};
  uint8_t index;

  if ((s_pdev == NULL) || (s_pdev->dev_state != USBD_STATE_CONFIGURED))
  {
    s_release_phase = 0U;
    return true;
  }

  if ((s_class.state != HID_KB_IDLE) || (s_class.cons_state != HID_KB_IDLE))
  {
    return false;
  }

  if (s_release_phase == 0U)
  {
    if (HidKeyboard_Transmit(key_zero) != (uint8_t)USBD_OK)
    {
      return false;
    }
    for (index = 0U; index < HID_KB_REPORT_LEN; index++)
    {
      s_last[index] = 0U;
    }
    s_have_sent = true;
    s_pending = false;
    s_release_phase = 1U;
    return false;
  }

  if (s_release_phase == 1U)
  {
    if (HidKeyboard_TransmitConsumer(cons_zero) != (uint8_t)USBD_OK)
    {
      return false;
    }
    s_cons_last[0] = 0U;
    s_cons_have_sent = true;
    s_cons_pending = false;
    s_release_phase = 2U;
    return false;
  }

  s_release_phase = 0U;
  return true;
}

USBD_ClassTypeDef USBD_HID_Keyboard =
{
  HidKeyboard_Init,
  HidKeyboard_DeInit,
  HidKeyboard_Setup,
  NULL,
  HidKeyboard_EP0_RxReady,
  HidKeyboard_DataIn,
  HidKeyboard_DataOut,
  NULL,
  NULL,
  NULL,
  HidKeyboard_GetHSCfgDesc,
  HidKeyboard_GetFSCfgDesc,
  HidKeyboard_GetOtherSpeedCfgDesc,
  HidKeyboard_GetDeviceQualifierDesc,
};
