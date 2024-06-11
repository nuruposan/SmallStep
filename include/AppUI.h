#pragma once

#include <M5Stack.h>

const uint8_t ICON_BT_BG[] = {16,         19,          // Width, Height
                              0,          0,           // Color
                              0b00001111, 0b11110000,  //
                              0b00111111, 0b11111100,  //
                              0b01111111, 0b11111110,  //
                              0b01111111, 0b11111110,  //
                              0b01111111, 0b11111110,  //
                              0b11111111, 0b11111111,  //
                              0b11111111, 0b11111111,  //
                              0b11111111, 0b11111111,  //
                              0b11111111, 0b11111111,  //
                              0b11111111, 0b11111111,  //
                              0b11111111, 0b11111111,  //
                              0b11111111, 0b11111111,  //
                              0b11111111, 0b11111111,  //
                              0b11111111, 0b11111111,  //
                              0b01111111, 0b11111110,  //
                              0b01111111, 0b11111110,  //
                              0b01111111, 0b11111110,  //
                              0b00111111, 0b11111100,  //
                              0b00001111, 0b11110000};
const uint8_t ICON_BT_FG[] = {16,         19,          // Width, Height
                              0,          0,           // Color
                              0b00000001, 0b10000000,  //
                              0b00000001, 0b11000000,  //
                              0b00000001, 0b11100000,  //
                              0b00000001, 0b10110000,  //
                              0b00100001, 0b10011000,  //
                              0b00110001, 0b10001100,  //
                              0b00011001, 0b10011000,  //
                              0b00001101, 0b10110000,  //
                              0b00000111, 0b11100000,  //
                              0b00000011, 0b11000000,  //
                              0b00000111, 0b11100000,  //
                              0b00001101, 0b10110000,  //
                              0b00011001, 0b10011000,  //
                              0b00110001, 0b10001100,  //
                              0b00100001, 0b10011000,  //
                              0b00000001, 0b10110000,  //
                              0b00000001, 0b11100000,  //
                              0b00000001, 0b11000000,  //
                              0b00000001, 0b10000000};

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

typedef struct {
  const char *caption;
  bool enabled;
} navitem_t;

typedef struct {
  navitem_t items[3];
  void (*onButtonPress)(btnid_t);
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
  TFT_eSprite sprite = TFT_eSprite(&M5.Lcd);
  Button *buttons[3];

  const uint8_t *appIcon;
  const uint8_t *btBgIcon;
  const uint8_t *btFgIcon;
  const char *appTitle;
  char appHint[2][16];

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
  void drawTitleBar(bool sdAvail, bool btActive);
  void drawNavBar(navmenu_t *nav);
  void setAppHints(const char *into1, const char *info2);
  void setAppTitle(const char *title);
  void setAppIcon(const uint8_t *icon);
  btnid_t waitForButtonInput(navmenu_t *nav, bool idleShutdown);
  btnid_t waitForInputOk(bool idleShutdown);
  btnid_t waitForInputOkCancel(bool idleShutdown);
  void openConfigMenu(const char *title, cfgitem_t *menu, int8_t itemCount, bool idleShutdown);
  void openMainMenu(menuitem_t *menu, int8_t itemCount, bool idleShutdown);
};
