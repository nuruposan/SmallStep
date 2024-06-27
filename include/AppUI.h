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

typedef struct _uiarea {
  int16_t w;
  int16_t h;
  int16_t x;
  int16_t y;
} uiarea_t;

typedef struct _uisize {
  int16_t w;
  int16_t h;
} uisize_t;

typedef struct _uipos {
  int16_t x;
  int16_t y;
} uipos_t;

typedef struct _iconstate {
  bool visible;
  bool active;
} iconstate_t;

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
  iconstate_t btIcon;
  iconstate_t sdIcon;

  const uiarea_t TITLE_AREA = {320, 24, 0, 0};
  const uiarea_t NAV_AREA = {320, 24, 0, (240 - 24)};
  const uiarea_t CLIENT_AREA = {320, (240 - 48), 0, 24};
  const uiarea_t DIALOG_AREA = {(int16_t)(CLIENT_AREA.w - 8), (int16_t)(CLIENT_AREA.h - 8),
                                (int16_t)(CLIENT_AREA.x + 4), (int16_t)(CLIENT_AREA.y + 4)};

  btnid_t checkButtonInput(navmenu_t *nav);
  void drawConfigMenu(const char *title, cfgitem_t *menu, int8_t itemCount, int8_t top, int8_t select);
  void drawMainMenu(menuitem_t *menu, int8_t itemCount, int8_t top, int8_t select);
  void drawAppIcon(int16_t x, int16_t y);
  void drawBatteryIcon(int16_t x, int16_t y);
  void drawBitmap(const uint8_t *iconData, int16_t x, int16_t y, int16_t color);
  void drawBitmap(const uint8_t *iconData, int16_t x, int16_t y);
  void drawBluetoothIcon(int16_t x, int16_t y);
  void drawSDcardIcon(int16_t x, int16_t y);
  uisize_t getBitmapSize(const uint8_t *iconData);
  uint16_t getBitmapColor(const uint8_t *iconData);

 public:
  AppUI();
  ~AppUI();

  void drawDialogFrame(const char *title);
  void drawDialogMessage(int16_t color, int8_t line, String msg);
  void drawDialogProgress(int32_t progress);
  void drawTitleBar();
  void drawNavBar(navmenu_t *nav);
  void setAppHints(const char *into1, const char *info2);
  void setAppTitle(const char *title);
  void setSDcardIconStatus(bool iconVisible, bool mounted);
  void setBluetoothIconStatus(bool iconVisible, bool active);
  void setAppIcon(const uint8_t *icon);
  void setIdleCallback(void (*callback)(), uint32_t timeout);
  btnid_t waitForButtonInput(navmenu_t *nav);
  btnid_t waitForInputOk();
  btnid_t waitForInputOkCancel();
  void openConfigMenu(const char *title, cfgitem_t *menu, int8_t itemCount);
  void openMainMenu(menuitem_t *menu, int8_t itemCount);
};
