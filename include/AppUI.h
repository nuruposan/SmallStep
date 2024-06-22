#pragma once

#include <M5Stack.h>

#define APP_HINT_LEN 24

#define COLOR16(r, g, b) (int16_t)((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

typedef enum {
  BID_NONE = -1,
  BID_BTN_A = 0,
  BID_BTN_B = 1,
  BID_BTN_C = 2,
  BID_OK = 10,
  BID_CANCEL = 11,
  BID_YES = 12,
  BID_NO = 13
} btnid_t;

typedef struct _navitem {
  const char *caption;
  bool enabled;
} navitem_t;

typedef struct _navmenu {
  navitem_t items[3];
  void (*onSelect)(_navmenu *);
} navmenu_t;

typedef struct _menuitem {
  const char *caption;
  const uint8_t *iconData;
  bool enabled;
  void (*onSelect)(_menuitem *);
} menuitem_t;

typedef struct _cfgitem {
  const char *caption;
  const char *description;
  char valueDescr[16];
  void (*onSelect)(_cfgitem *);
  void (*updateValueDescr)(_cfgitem *);
} cfgitem_t;

class AppUI {
 private:
  const static int16_t LOOP_WAIT = 70;

  TFT_eSprite sprite = TFT_eSprite(&M5.Lcd);
  Button *buttons[3];

  const uint8_t *appIcon;
  const char *appTitle;
  char appHint[2][APP_HINT_LEN];
  void (*idleCallback)();
  uint32_t idleTimeout;
  uint32_t idleStart;
  bool sdcardVisible;
  bool sdcardMounted;
  bool bluetoothVisible;
  bool bluetoothActive;

  int16_t titleBarHeight;
  int16_t titleBarWidth;
  int16_t titleBarTop;
  int16_t titleBarLeft;
  int16_t menuAreaWidth;
  int16_t menuAreaHeight;
  int16_t menuAreaTop;
  int16_t menuAreaLeft;
  int16_t dlgFrameWidth;
  int16_t dlgFrameHeight;
  int16_t dlgTitleHeight;
  int16_t dlgFrameTop;
  int16_t dlgFrameLeft;
  int16_t dlgClientWidth;
  int16_t dlgClientHeight;
  int16_t dlgClientTop;
  int16_t dlgClientLeft;
  int16_t navBarHeight;
  int16_t navBarWidth;
  int16_t navBarTop;
  int16_t navBarLeft;

  btnid_t checkButtonInput(navmenu_t *nav);
  void drawConfigMenu(const char *title, cfgitem_t *menu, int8_t itemCount, int8_t top, int8_t select);
  void drawMainMenu(menuitem_t *menu, int8_t itemCount, int8_t top, int8_t select);
  void putAppIcon(TFT_eSprite *spr, int16_t x, int16_t y);
  void putBitmap(TFT_eSprite *spr, const uint8_t *iconData, int16_t x, int16_t y, int16_t color);
  void putBatteryIcon(TFT_eSprite *spr, int16_t x, int16_t y, int8_t level);
  void putBluetoothIcon(TFT_eSprite *spr, int16_t x, int16_t y, bool connected);
  void putSDcardIcon(TFT_eSprite *spr, int16_t x, int16_t y, bool mounted);

 public:
  AppUI();

  void drawDialogFrame(const char *title);
  void drawDialogMessage(int16_t color, int8_t line, String msg);
  void drawDialogProgress(int32_t progress);
  void drawTitleBar();
  void drawNavBar(navmenu_t *nav);
  void setAppHints(const char *into1, const char *info2);
  void setAppTitle(const char *title);
  void setSDcardStatus(bool iconVisible, bool mounted);
  void setBluetoothStatus(bool iconVisible, bool active);
  void setAppIcon(const uint8_t *icon);
  void setIdleCallback(void (*callback)(), uint32_t timeout);
  btnid_t waitForButtonInput(navmenu_t *nav);
  btnid_t waitForInputOk();
  btnid_t waitForInputOkCancel();
  void openConfigMenu(const char *title, cfgitem_t *menu, int8_t itemCount);
  void openMainMenu(menuitem_t *menu, int8_t itemCount);
};
