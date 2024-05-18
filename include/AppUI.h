#pragma once

#include <M5Stack.h>

const uint8_t ICON_APP[] = {32,         20,         0,          0,           // Width, Height, Color
                            0b00001100, 0b01100000, 0b00000000, 0b00000000,  //
                            0b00011110, 0b11110000, 0b00000000, 0b00000000,  //
                            0b00011110, 0b11110000, 0b00000000, 0b00000000,  //
                            0b01101100, 0b01101100, 0b00000000, 0b00000000,  //
                            0b11110011, 0b10011110, 0b00000000, 0b00000000,  //
                            0b11110111, 0b11011110, 0b00000000, 0b00000000,  //
                            0b01101111, 0b11101100, 0b00000000, 0b00000000,  //
                            0b00011111, 0b11110000, 0b00000000, 0b00000000,  //
                            0b00111111, 0b11111000, 0b01100011, 0b00000000,  //
                            0b00111111, 0b11111000, 0b11110111, 0b10000000,  //
                            0b00001111, 0b11100000, 0b11110111, 0b10000000,  //
                            0b00000000, 0b00000011, 0b01100011, 0b01100000,  //
                            0b00000000, 0b00000111, 0b10011100, 0b11110000,  //
                            0b00000000, 0b00000111, 0b10111110, 0b11110000,  //
                            0b00000000, 0b00000011, 0b01111111, 0b01100000,  //
                            0b00000000, 0b00000000, 0b11111111, 0b10000000,  //
                            0b00000000, 0b00000001, 0b11111111, 0b11000000,  //
                            0b00000000, 0b00000001, 0b11111111, 0b11000000,  //
                            0b00000000, 0b00000001, 0b11111111, 0b11000000,  //
                            0b00000000, 0b00000000, 0b01111111, 0b00000000};

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

typedef struct {
  const char *caption;
  const uint8_t *iconData;
  bool enabled;
  void (*onSelect)();
} menuitem_t;

typedef struct {
  menuitem_t items[12];
  int8_t itemCount;
  int8_t selIndex;
  int8_t topIndex;
} mainmenu_t;

typedef struct {
  const char *caption;
  const char *description;
  char *valueDescr;
  void (*onSelect)();
} cfgitem_t;

typedef struct {
  cfgitem_t items[16];
  int8_t itemCount;
  int8_t selIndex;
  int8_t topIndex;
} cfgmenu_t;

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

  void putAppIcon(TFT_eSprite *spr, int16_t x, int16_t y);
  void putBitmap(TFT_eSprite *spr, const uint8_t *iconData, int16_t x, int16_t y, int16_t color);
  void putBatteryIcon(TFT_eSprite *spr, int16_t x, int16_t y, int8_t level);
  void putBluetoothIcon(TFT_eSprite *spr, int16_t x, int16_t y, bool connected);
  void putSDcardIcon(TFT_eSprite *spr, int16_t x, int16_t y, bool mounted);

 public:
  AppUI();

  void drawDialogFrame(const char *title);
  void drawDialogProgress(int32_t progress);
  void printDialogMessage(int16_t color, int8_t line, String msg);
  void drawMainMenu(mainmenu_t *menu);
  void drawConfigMenu(cfgmenu_t *menu);
  void drawNavBar(navmenu_t *menu);
  void drawTitleBar(bool sdAvail, bool btActive);
  btnid_t checkButtonInput(navmenu_t *menu);
  void setAppTitle(const char *title);
  void setAppHints(const char *into1, const char *info2);
  btnid_t waitForOk();
  btnid_t waitForOkCancel();
  void handleInputForConfigMenu(cfgmenu_t *menu);
};
