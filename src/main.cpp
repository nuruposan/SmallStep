#include <BTScan.h>
#include <BluetoothSerial.h>
#include <M5Stack.h>
#include <SD.h>

// #include <stack>

#include "MyObject.hpp"

#define SD_ACCESS_SPEED 15000000
#define COLOR16(r, g, b) (int16_t)((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

typedef enum { BID_BTN_A = 0, BID_BTN_B = 1, BID_BTN_C = 2 } btnid_t;

typedef struct _naviitem {
  const char *caption;
  bool disabled;
} naviitem_t;

typedef struct _navimenu {
  naviitem_t items[3];
  void (*onButtonPress)(btnid_t);
} navimenu_t;

typedef struct _menuitem {
  const char *caption;
  int8_t *iconData;
  bool disabled;
  void (*onSelected)();
} menuitem_t;

typedef struct _mainmenu {
  menuitem_t items[6];
  int8_t selectedIndex;
} mainmenu_t;

typedef struct _loggerinfo {
  int8_t found;
  char remoteName[20];
  uint8_t address[6];
} loggerinfo_t;

typedef struct _appstatus {
  navimenu_t *navi;
  navimenu_t *prevNavi;
  mainmenu_t *menu;
  Button *buttons[3];
  loggerinfo_t loggerSelected;
  loggerinfo_t loggerDiscovered;
} appstatus_t;

// ******** resource data ********

const uint8_t ICON_APP[] = {
    16,             // Width
    12,             // Height
    0,          0,  // 16bit color
    0b00000110, 0b00110000, 0b00001111, 0b01111000, 0b00001111, 0b01111000,
    0b00110110, 0b00110110, 0b01111001, 0b11001111, 0b01111011, 0b11101111,
    0b00110111, 0b11110110, 0b00001111, 0b11111000, 0b00011111, 0b11111100,
    0b00011111, 0b11111100, 0b00011111, 0b11111100, 0b00000111, 0b11110000};
const uint8_t ICON_BT_BG[] = {
    16,         19,         0,          0,  // Width, Height, Color
    0b00001111, 0b11110000, 0b00111111, 0b11111100, 0b01111111, 0b11111110,
    0b01111111, 0b11111110, 0b01111111, 0b11111110, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b01111111, 0b11111110,
    0b01111111, 0b11111110, 0b01111111, 0b11111110, 0b00111111, 0b11111100,
    0b00001111, 0b11110000};
const uint8_t ICON_BT_FG[] = {
    16,         19,         0,          0,  // Width, Height, Color
    0b00000001, 0b10000000, 0b00000001, 0b11000000, 0b00000001, 0b11100000,
    0b00000001, 0b10110000, 0b00100001, 0b10011000, 0b00110001, 0b10001100,
    0b00011001, 0b10011000, 0b00001101, 0b10110000, 0b00000111, 0b11100000,
    0b00000011, 0b11000000, 0b00000111, 0b11100000, 0b00001101, 0b10110000,
    0b00011001, 0b10011000, 0b00110001, 0b10001100, 0b00100001, 0b10011000,
    0b00000001, 0b10110000, 0b00000001, 0b11100000, 0b00000001, 0b11000000,
    0b00000001, 0b10000000};
const uint8_t ICON_DOWNLOAD[] = {
    48,         43,         0,          0,  // Width, Height, Color
    0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000011, 0b11000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b00000000,
    0b00000000, 0b00000011, 0b11111111, 0b11111111, 0b11000000, 0b00000000,
    0b00111100, 0b00000001, 0b11111111, 0b11111111, 0b10000000, 0b00111100,
    0b01111110, 0b00000000, 0b11111111, 0b11111111, 0b00000000, 0b01111110,
    0b01111110, 0b00000000, 0b01111111, 0b11111110, 0b00000000, 0b01111110,
    0b01111110, 0b00000000, 0b00111111, 0b11111100, 0b00000000, 0b01111110,
    0b01111110, 0b00000000, 0b00011111, 0b11111000, 0b00000000, 0b01111110,
    0b01111110, 0b00000000, 0b00001111, 0b11110000, 0b00000000, 0b01111110,
    0b01111110, 0b00000000, 0b00000111, 0b11100000, 0b00000000, 0b01111110,
    0b01111110, 0b00000000, 0b00000011, 0b11000000, 0b00000000, 0b01111110,
    0b01111110, 0b00000000, 0b00000001, 0b10000000, 0b00000000, 0b01111110,
    0b01111110, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b01111110,
    0b01111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111110,
    0b01111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111110,
    0b01111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111110,
    0b01111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111110,
    0b01111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111110,
    0b00111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111100};

// ******** function prototypes ********

uint8_t calcNmeaChecksum(char *, uint16_t);
bool sendNmeaCommand(char *, uint16_t);
void sendDownloadCommand(int, int);
void onBluetoothDiscoverCallback(esp_spp_cb_event_t, esp_spp_cb_param_t *);
void onDownloadLogMenuSelected();
void onFixRTCMenuSelected();
void onSetPresetConfigMenuSelected();
void onShowLocationMenuSelected();
void onEraseLogMenuSelected();
void onAppSettingsMenuSelected();
void drawProgressBar(char *, uint8_t);
void drawDialog(const char *, const char *);
void draw1bitBitmap(TFT_eSprite *, const uint8_t *, int16_t, int16_t, int32_t);
void drawApplicationIcon(TFT_eSprite *, int16_t, int16_t);
void drawBluetoothIcon(TFT_eSprite *, int16_t, int16_t);
void drawSDcardIcon(TFT_eSprite *, int16_t, int16_t);
void drawBatteryIcon(TFT_eSprite *, int16_t, int16_t);
void drawTitleBar();
void drawMainMenu();
void drawNaviBar();
void onNaviPrevButtonClick();
void onNaviNextButtonClick();
void onNaviEnterButtonClick();
void onMainNaviButtonPress(btnid_t);
void onDialogOKButtonClick();
void onDialogCancelButtonClick();
void onDialogNaviButtonPress(btnid_t);

const char APP_NAME[] = "SmallStepM5S";
const char BT_NAME[] = "ESP32";
const char APP_VERSION[] = "v0.01";

const int16_t COLOR_SCREEN = BLACK;
const int16_t COLOR_TITLE_BACK = LIGHTGREY;
const int16_t COLOR_TITLE_TEXT = BLACK;
const int16_t COLOR_ICON_BODY = BLACK;
const int16_t COLOR_ICON_TEXT = LIGHTGREY;
const int16_t COLOR_ICON_ERROR = RED;
const int16_t COLOR_ICON_SD_PIN = OLIVE;
const int16_t COLOR_MENU_BACK = DARKGREY;
const int16_t COLOR_MENU_TEXT = BLACK;
const int16_t COLOR_MENU_SEL_BORDER = BLUE;
const int16_t COLOR_MENU_SEL_BACK = LIGHTGREY;
const int16_t COLOR_NAVI_BACK = LIGHTGREY;
const int16_t COLOR_NAVI_TEXT = BLACK;

typedef struct phandler {
  void (*run)();
} phandler_t;

BluetoothSerial gpsSerial;
TFT_eSprite sprite = TFT_eSprite(&M5.Lcd);
appstatus_t app;
// std::stack<int> handlerStack;
MyObject *obj;

/**
 * NMEAコマンドのチェックサム計算を行う
 * (printfのフォーマット%Xで送る値を生成)
 */
uint8_t calcNmeaChecksum(char *cmd, uint16_t len) {
  uint8_t chk = 0;
  for (uint16_t i = 0; (i < len) && (cmd[i] != 0); i++) {
    chk ^= (byte)(cmd[i]);
  }
  return chk;
}

/**
 * NMEAコマンドにチェックサムをつけてSerialBTに書き出す
 */
bool sendNmeaCommand(char *cmd, uint16_t len) {
  if (gpsSerial.connected() == false) return false;

  uint16_t len2 = len + 8;
  char buf[len2];

  byte chk = calcNmeaChecksum(cmd, len);
  memset(buf, 0, len2);
  sprintf(buf, "$%s*%X\r\n", cmd, chk);

  Serial.print("[DEBUG] sendNmeaCommand: >");

  for (short i = 0; (i < len2) && (buf[i] != 0); i++) {
    gpsSerial.write(buf[i]);
    Serial.print(buf[i]);
  }

  return true;
}

// PMTK182,7コマンドを送る (ログのダウンロードコマンド)
void sendDownloadCommand(int startPos, int reqSize) {
  char cmdstr[40];
  sprintf(cmdstr, "PMTK182,7,%08X,%08X", startPos, reqSize);
  sendNmeaCommand(cmdstr, sizeof(cmdstr));
}

void onBluetoothDiscoverCallback(esp_spp_cb_event_t event,
                                 esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_OPEN_EVT) {
    app.loggerDiscovered.found = true;
    memcpy(&(app.loggerDiscovered.address), &(param->open.rem_bda),
           sizeof(app.loggerDiscovered.address));
  }
}

void drawProgressBar(char *caption, uint8_t progress) {
  // 進捗率の指定のレンジ補正
  if (progress > 100) progress = 100;

  sprite.createSprite(300, 40);

  sprite.setTextColor(LIGHTGREY);
  sprite.drawString(caption, 8, 154, 4);
  sprite.fillRect((progress / 100) * 300, 25, 300, 16, LIGHTGREY);
  sprite.fillRect(0, 25, (progress / 100) * 300, 16, BLUE);
  sprite.drawRect(0, 25, 300, 40, DARKGREY);

  sprite.pushSprite(10, 180);
  sprite.deleteSprite();
}

void drawDialog(const char *title, const char *msg) {
  const int16_t MARGIN = 4;
  const int16_t DIALOG_X = MARGIN;
  const int16_t DIALOG_Y = 24 + MARGIN;
  const int16_t DIALOG_W = M5.Lcd.width() - (MARGIN * 2);
  const int16_t DIALOG_H = (M5.Lcd.height() - 48) - (MARGIN * 2);
  const int16_t TITLE_X = 2;
  const int16_t TITLE_Y = 2;
  const int16_t MSG_X = 8;
  const int16_t MSG_Y = 30;

  sprite.setColorDepth(8);
  sprite.createSprite(DIALOG_W, DIALOG_H);

  sprite.fillRect(0, 0, sprite.width(), sprite.height(), LIGHTGREY);
  sprite.fillRect(0, 0, sprite.width(), 26, COLOR16(0, 128, 255));
  sprite.drawRect(0, 0, sprite.width(), sprite.height(), BLUE);

  sprite.setTextSize(1);
  sprite.setTextColor(BLACK);
  sprite.drawString(title, TITLE_X, TITLE_Y, 4);
  sprite.drawString(msg, MSG_X, MSG_Y, 4);

  sprite.pushSprite(DIALOG_X, DIALOG_Y);
  sprite.deleteSprite();
}

/**
 *
 */
void onDownloadLogMenuSelected() {
  {
    navimenu_t *nm = (navimenu_t *)malloc(sizeof(navimenu_t));
    memset(nm, 0, sizeof(navimenu_t));

    nm->onButtonPress = onDialogNaviButtonPress;
    nm->items[0].caption = "OK";
    nm->items[1].disabled = true;
    nm->items[2].disabled = "Cancel";

    app.prevNavi = app.navi;
    app.navi = nm;

    drawNaviBar();
  }

  const char gpsPinCode[] = "0000";  // SPP接続時のPinコード。通常は"0000"で固定

  // BT初期化
  gpsSerial.register_callback(onBluetoothDiscoverCallback);
  gpsSerial.setTimeout(5000);
  gpsSerial.setPin(gpsPinCode);  // Pinコードをセット。これがないと繋がらない

  // char deviceNames[][] = {"747PRO GPS", "Holux_M-241"};

  drawDialog("Please wait", "Finding GPS logger\"...");
  M5.Lcd.setTextColor(LIGHTGREY);
  for (int i = 0; i < 31; i++) {
    char buf[20];
    sprintf(buf, "%d\%", (100 * i / 30));

    drawProgressBar(buf, (100 * i / 30));
    delay(1000);
  }
  BTScanResults *devList = gpsSerial.discover(30000);
  if (devList) {
    Serial.printf("Bluetooth scan result: %d devices\n", devList->getCount());

    for (int16_t i = 0; i < devList->getCount(); i++) {
      BTAdvertisedDevice *device = devList->getDevice(i);

      Serial.printf("device[%d] name=%s, addr=%s\n", i,
                    device->getName().c_str(),
                    device->getAddress().toString().c_str());
    }
  } else {
    Serial.println("No Bluetooth device found");
  }
}

/**
 *
 */
void onFixRTCMenuSelected() { Serial.printf("[DEBUG] onFixRTCMenuSelected\n"); }

/**
 *
 */
void onSetPresetConfigMenuSelected() {
  Serial.printf("[DEBUG] onSetPresetConfigMenuSelected\n");
}

/**
 *
 */
void onShowLocationMenuSelected() {
  Serial.printf("[DEBUG] onShowLocationMenuSelected\n");
}

/**
 *
 */
void onEraseLogMenuSelected() {
  Serial.printf("[DEBUG] onEraseLogMenuSelected\n");
}

/**
 *
 */
void onAppSettingsMenuSelected() {
  Serial.printf("[DEBUG] onAppSettingsMenuSelected\n");
}

void draw1bitBitmap(TFT_eSprite *spr, const uint8_t *iconData, int16_t x0,
                    int16_t y0, int32_t color) {
  // iconDataはuint8_tの配列へのポインタ。想定データフォーマットは以下の通り
  // - 1st byte: uint8_t width
  // - 2nd byte: uint8_t height
  // - 3rd-4th byte: uint16_t color (**currently unused**)
  // - 5th byte to the end: uint8_t pixelBits[]
  //
  // データフォーマットに関する備考
  // - 標準フォーマットのビットマップデータではない
  // - 画像サイズは255x255まで
  // - アイコンの1行の区切りをbyte単位に揃える必要はない

  uint8_t iconW = *iconData++;  // 1バイト目は幅
  uint8_t iconH = *iconData++;  // 2バイト目は高さ
  iconData += 2;  // 3-4バイト目を無視。色の指定は引数で行う
  uint16_t pixelsTotal = (iconW * iconH);  // ピクセル数を幅×高さで計算

  // 左上からピクセルデータを読み込んでいき、1pxずつ、対応するビットが1なら指定の色で塗る
  for (int32_t i = 0; i < pixelsTotal; i++) {
    if (*iconData & (0b10000000 >> (i % 8))) {  // 対応するビットが1なら
      int px = x0 + (i % iconW);
      int py = y0 + (i / iconW);
      spr->drawPixel(px, py, color);
    }

    // 8ビット目まで見たら参照先を次の1バイトへずらす
    if ((i % 8) == 7) iconData++;
  }
}

void drawApplicationIcon(TFT_eSprite *spr, int16_t x0, int16_t y0) {
  // アイコンのデータの描画色を決定
  uint16_t color = COLOR16(0, 64, 255);

  // アイコンを描画 (2つセット)
  draw1bitBitmap(spr, ICON_APP, x0, y0, color);
  draw1bitBitmap(spr, ICON_APP, (x0 + 12), (y0 + 8), color);
}

void drawBluetoothIcon(TFT_eSprite *spr, int16_t x0, int16_t y0) {
  // 描画色の決定。Bluetoothが未接続の場合はアイコンを灰色背景にする
  uint16_t colorBG = BLUE;
  uint16_t colorFG = WHITE;
  if (gpsSerial.connected() == false) {
    colorBG = DARKGREY;
    colorFG = WHITE;
  }

  // アイコンの背景・前景を指定位置に描画
  draw1bitBitmap(spr, ICON_BT_BG, x0, y0, colorBG);
  draw1bitBitmap(spr, ICON_BT_FG, x0, y0, colorFG);
}

/**
 * SDカードの状態表示アイコンの描画
 */
void drawSDcardIcon(TFT_eSprite *spr, int16_t x0, int16_t y0) {
  const int16_t ICON_W = 17;
  const int16_t ICON_H = 20;
  const int16_t PIN_W = 2;
  const int16_t PIN_H = 4;

  // SDカードの外枠を描画
  spr->fillRoundRect((x0 + 2), y0, (ICON_W - 2), 8, 2,
                     COLOR_ICON_BODY);  // 本体
  spr->fillRoundRect(x0, (y0 + 6), ICON_W, (ICON_H - 6), 2,
                     COLOR_ICON_BODY);  // 本体
  for (int16_t i = 0; i < 4; i++) {     // 端子部
    spr->fillRect((x0 + 4) + ((PIN_W + 1) * i), (y0 + 1), PIN_W, PIN_H,
                  COLOR_ICON_SD_PIN);
  }

  File root = SD.open("/");
  if (root) {  // SDカードが使用可能な場合。SDの文字をつける
    // テスト用に開いたディレクトリを閉じる
    root.close();

    {  // 状態表示
      spr->setTextSize(1);
      spr->setTextColor(COLOR_ICON_TEXT);
      spr->drawCentreString("SD", (x0 + 9), (y0 + 9), 1);
    }
  } else {  // SDカードが使用不能な場合。赤丸に斜線マーク
    spr->drawEllipse((x0 + 8), (y0 + 12), 5, 5, COLOR_ICON_ERROR);
    spr->drawLine((x0 + 11), (y0 + 9), (x0 + 5), (y0 + 15), COLOR_ICON_ERROR);
  }
}

/**
 * バッテリーアイコンの描画
 */
void drawBatteryIcon(TFT_eSprite *spr, int16_t x0, int16_t y0) {
  const int16_t ICON_W = 21;
  const int16_t ICON_H = 20;
  const int16_t METER_W = 3;
  const int16_t METER_H = 12;

  // バッテリーの外枠を描画
  spr->fillRoundRect(x0, y0, (ICON_W - 2), ICON_H, 2,
                     COLOR_ICON_BODY);  // 本体
  spr->fillRect((x0 + (ICON_W - 2)), (y0 + 6), 2, 8,
                COLOR_ICON_BODY);  // 凸部

  if (M5.Power.canControl()) {
    // 4段階でバッテリーレベルを取得
    // 0-bias%以下、2540%以下、65%以下、100%以下
    int8_t battBias = 15;
    int8_t battLevel = (battBias + M5.Power.getBatteryLevel()) / 25;

    // バッテリーレベルの表示
    for (int16_t i = 0; i < battLevel; i++) {
      spr->fillRect((x0 + 2) + ((METER_W + 1) * i), (y0 + 4), METER_W, METER_H,
                    COLOR_ICON_TEXT);
    }
    if (battLevel == 0) {
      spr->fillRect((x0 + 2), (y0 + 4), METER_W, METER_H, COLOR_ICON_TEXT);
    }
  } else {  // バッテリーレベル不明の表示
    spr->setTextSize(2);
    spr->setTextColor(COLOR_ICON_ERROR);
    spr->drawCentreString("?", x0 + (ICON_W / 2), y0 + 3, 1);
  }
}

/**
 * タイトルバー表示
 */
void drawTitleBar() {
  const int16_t TITLEBAR_X = 0;
  const int16_t TITLEBAR_Y = 0;
  const int16_t TITLEBAR_W = M5.Lcd.width();
  const int16_t TITLEBAR_H = 24;
  const int16_t CAPTION_X = 32;
  const int16_t CAPTION_Y = 2;
  const int16_t APP_ICON_X = (TITLEBAR_X + 2);
  const int16_t BAT_ICON_X = (TITLEBAR_W - 24);
  const int16_t SD_ICON_X = (BAT_ICON_X - 20);
  const int16_t BT_ICON_X = (SD_ICON_X - 20);
  const int16_t ICON_Y = (TITLEBAR_Y + 2);

  // スプライト領域の初期化
  sprite.setColorDepth(8);
  sprite.createSprite(TITLEBAR_W, TITLEBAR_H);
  sprite.fillScreen(COLOR_TITLE_BACK);

  //
  drawApplicationIcon(&sprite, 2, ICON_Y);

  {  // タイトル文字の表示
    sprite.setTextFont(4);
    sprite.setTextSize(1);
    sprite.setTextColor(COLOR_TITLE_TEXT);
    sprite.drawString(APP_NAME, CAPTION_X, CAPTION_Y);
  }

  // 状態表示アイコンの表示
  drawBatteryIcon(&sprite, BAT_ICON_X, ICON_Y);
  drawSDcardIcon(&sprite, SD_ICON_X, ICON_Y);
  drawBluetoothIcon(&sprite, BT_ICON_X, ICON_Y);

  // 描画した内容を画面に反映し、スプライト領域を開放
  sprite.pushSprite(TITLEBAR_X, TITLEBAR_Y);
  sprite.deleteSprite();
}

void drawMainMenu() {
  const int16_t MENUAREA_W = M5.Lcd.width();
  const int16_t MENUAREA_H = M5.Lcd.height() - (24 + 24);
  const int16_t MENUAREA_X = 0;
  const int16_t MENUAREA_Y = 24;
  const int16_t MENUBTN_W = 98;  // ボタンのサイズ指定
  const int16_t MENUBTN_H = 83;
  const int16_t MENUBTN1_X = 5;  // 配置の基準とする左上ボタンの位置
  const int16_t MENUBTN1_Y = 9;
  const int16_t MARGIN_X = 8;
  const int16_t MARGIN_Y = 7;

  // スプライト領域の初期化
  sprite.setColorDepth(8);
  sprite.createSprite(MENUAREA_W, MENUAREA_H);
  sprite.fillScreen(COLOR_SCREEN);

  for (int16_t i = 0; i < 6; i++) {
    int16_t POS_X = MENUBTN1_X + ((MENUBTN_W + MARGIN_X) * (i % 3));
    int16_t POS_Y = MENUBTN1_Y + ((MENUBTN_H + MARGIN_Y) * (i / 3));

    // メインメニューのボタンの背景を描画
    if (i == app.menu->selectedIndex) {  // 選択状態メニュー
      sprite.fillRoundRect(POS_X, POS_Y, MENUBTN_W, MENUBTN_H, 4,
                           COLOR_MENU_SEL_BACK);
      sprite.drawRoundRect(POS_X, POS_Y, MENUBTN_W, MENUBTN_H, 4,
                           COLOR_MENU_SEL_BORDER);
    } else {  // 非選択状態メニュー
      sprite.fillRoundRect(POS_X, POS_Y, MENUBTN_W, MENUBTN_H, 4,
                           COLOR_MENU_BACK);
    }

    draw1bitBitmap(&sprite, ICON_DOWNLOAD, (POS_X + (MENUBTN_W / 2) - (48 / 2)),
                   (POS_Y + 12), COLOR16(0, 32, 32));

    {  // ボタンのキャプションを描画
      sprite.setTextSize(1);
      sprite.setTextColor(COLOR_MENU_TEXT);
      sprite.drawCentreString(app.menu->items[i].caption,
                              POS_X + (MENUBTN_W / 2), POS_Y + (MENUBTN_H - 12),
                              1);
    }
  }

  // 描画した内容を画面に反映し、スプライト領域を開放
  sprite.pushSprite(MENUAREA_X, MENUAREA_Y);
  sprite.deleteSprite();
}

/**
 * 物理ボタンの操作ナビゲーションボタンを描画する。
 * ボタンのキャプションや有効状態はグローバル変数のnaviitem_t
 * naviButtons[3]を参照
 */
void drawNaviBar() {
  const int16_t NAVIBAR_W = M5.Lcd.width();
  const int16_t NAVIBAR_H = 24;
  const int16_t NAVIBAR_X = 0;
  const int16_t NAVIBAR_Y = M5.Lcd.height() - NAVIBAR_H;
  const int16_t NAVIBTN_W = 86;
  const int16_t NAVIBTN_H = NAVIBAR_Y;
  const int16_t NAVIBTN1_X = 65 - (NAVIBTN_W / 2);
  const int16_t MARGIN_X = 8;

  // スプライト領域の初期化
  sprite.setColorDepth(8);
  sprite.createSprite(NAVIBAR_W, NAVIBAR_H);
  sprite.fillScreen(COLOR_SCREEN);

  // 物理ボタンに対応する操作ナビゲーションボタンを3つ左から描画
  for (int16_t i = 0; i < 3; i++) {
    // 対象のボタンアイテム情報を取得
    naviitem_t *ni = &(app.navi->items[i]);

    // 対象ボタンが無効なら描画しない
    if (ni->disabled) continue;

    // 対象ボタンの描画位置の決定
    int16_t POS_X = NAVIBTN1_X + ((NAVIBTN_W + MARGIN_X) * i);
    int16_t POS_Y = 0;

    // ボタンの枠を描画
    sprite.fillRoundRect(POS_X, POS_Y, NAVIBTN_W, NAVIBTN_H, 2,
                         COLOR_NAVI_BACK);

    // キャプションを描画
    sprite.setTextSize(1);
    sprite.setTextColor(COLOR_NAVI_TEXT);
    sprite.drawCentreString(ni->caption, (POS_X + (NAVIBTN_W / 2)), (POS_Y + 2),
                            4);
  }

  // 描画した内容を画面に反映し、スプライト領域を開放
  sprite.pushSprite(NAVIBAR_X, NAVIBAR_Y);
  sprite.deleteSprite();
}

void onNaviPrevButtonClick() {
  app.menu->selectedIndex = (app.menu->selectedIndex + 5) % 6;
  drawMainMenu();
}

void onNaviNextButtonClick() {
  app.menu->selectedIndex = (app.menu->selectedIndex + 1) % 6;
  drawMainMenu();
}

void onNaviEnterButtonClick() {
  menuitem_t *mi = &(app.menu->items[app.menu->selectedIndex]);
  if (mi->onSelected != NULL) mi->onSelected();
}

void onMainNaviButtonPress(btnid_t bid) {
  switch (bid) {
    case BID_BTN_A:
      onNaviPrevButtonClick();
      break;
    case BID_BTN_B:
      onNaviNextButtonClick();
      break;
    case BID_BTN_C:
      onNaviEnterButtonClick();
      break;
  }
}

void onDialogOKButtonClick() {
  // TODO
  free(app.navi);
  app.navi = app.prevNavi;
  app.prevNavi = NULL;

  drawMainMenu();
  drawNaviBar();
}

void onDialogCancelButtonClick() {
  // TODO
  //
}

void onDialogNaviButtonPress(btnid_t bid) {
  switch (bid) {
    case BID_BTN_A:
      onDialogOKButtonClick();
      break;
    case BID_BTN_B:
      break;
    case BID_BTN_C:
      onDialogCancelButtonClick();
      break;
  }
}

void setup() {
  // デバッグ用シリアルポートの初期化
  Serial.begin(115200);

  // M5Stackの初期化
  M5.begin();
  M5.Power.begin();

  // 画面の初期化
  M5.Lcd.setBrightness(32);
  M5.Lcd.clearDisplay(BLACK);

  // スプライトの初期化
  sprite.setColorDepth(8);

  // BTの開始
  gpsSerial.begin("ESP32", true);  // start bluetooth serial as a master

  // SDカードの開始
  SD.begin(GPIO_NUM_4, SPI, SD_ACCESS_SPEED);

  app.buttons[0] = &M5.BtnA;
  app.buttons[1] = &M5.BtnB;
  app.buttons[2] = &M5.BtnC;

  app.navi = (navimenu_t *)malloc(sizeof(navimenu_t));
  memset(app.navi, 0, sizeof(navimenu_t));
  app.navi->onButtonPress = &(onMainNaviButtonPress);
  app.navi->items[0].caption = "Prev";
  app.navi->items[1].caption = "Next";
  app.navi->items[2].caption = "Select";
  app.prevNavi = NULL;

  app.menu = (mainmenu_t *)malloc(sizeof(mainmenu_t));
  memset(app.menu, 0, sizeof(mainmenu_t));
  app.menu->items[0].caption = "Download Log";
  app.menu->items[0].onSelected = &(onDownloadLogMenuSelected);
  app.menu->items[1].caption = "Fix RTC";
  app.menu->items[1].onSelected = &(onFixRTCMenuSelected);
  app.menu->items[2].caption = "Set Preset Cfg";
  app.menu->items[2].onSelected = &(onSetPresetConfigMenuSelected);
  app.menu->items[3].caption = "Show Location";
  app.menu->items[3].onSelected = &(onShowLocationMenuSelected);
  app.menu->items[4].caption = "Erase Log";
  app.menu->items[4].onSelected = &(onEraseLogMenuSelected);
  app.menu->items[5].caption = "App Settings";
  app.menu->items[5].onSelected = &(onAppSettingsMenuSelected);

  // タイトルバーを描画
  drawTitleBar();
  drawNaviBar();
  drawMainMenu();
}

void loop() {
  M5.update();  // M5Stackのステータス更新

  // 物理ボタンの処理 (ナビボタンを押したものとして扱う)
  for (int16_t i = 0; i < 3; i++) {
    Button *b = app.buttons[i];  // 判定するボタン
    if (b->wasPressed()) {
      app.navi->onButtonPress((btnid_t)(i));
      break;  // 左・中・右の順に1回だけ処理。同時に複数の処理はしない
    }
  }

  delay(50);
}
