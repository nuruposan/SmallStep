#include <BluetoothSerial.h>
#include <M5Stack.h>
#include <SD.h>

#define SD_ACCESS_SPEED 15000000
#define COLOR16(r, g, b) (int16_t)((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

uint8_t ICON_APP[] = {  // 16x12
    16,         12,         0b00000110, 0b00110000, 0b00001111, 0b01111000,
    0b00001111, 0b01111000, 0b00110110, 0b00110110, 0b01111001, 0b11001111,
    0b01111011, 0b11101111, 0b00110111, 0b11110110, 0b00001111, 0b11111000,
    0b00011111, 0b11111100, 0b00011111, 0b11111100, 0b00011111, 0b11111100,
    0b00000111, 0b11110000};

uint8_t ICON_BT_BG[] = {  // 16x19
    16,         19,         0b00001111, 0b11110000, 0b00111111, 0b11111100,
    0b01111111, 0b11111110, 0b01111111, 0b11111110, 0b01111111, 0b11111110,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111,
    0b01111111, 0b11111110, 0b01111111, 0b11111110, 0b01111111, 0b11111110,
    0b00111111, 0b11111100, 0b00001111, 0b11110000};
uint8_t ICON_BT_FG[] = {  // 16x19
    16,         19,         0b00000001, 0b10000000, 0b00000001, 0b11000000,
    0b00000001, 0b11100000, 0b00000001, 0b10110000, 0b00100001, 0b10011000,
    0b00110001, 0b10001100, 0b00011001, 0b10011000, 0b00001101, 0b10110000,
    0b00000111, 0b11100000, 0b00000011, 0b11000000, 0b00000111, 0b11100000,
    0b00001101, 0b10110000, 0b00011001, 0b10011000, 0b00110001, 0b10001100,
    0b00100001, 0b10011000, 0b00000001, 0b10110000, 0b00000001, 0b11100000,
    0b00000001, 0b11000000, 0b00000001, 0b10000000};

BluetoothSerial gpsSerial;

const char APP_NAME[] = "SmallStepM5S";
const char APP_VERSION[] = "v0.01";

const int16_t COLOR_SCREEN = BLACK;
const int16_t COLOR_TITLE_BACK = LIGHTGREY;
// COLOR16(160, 192, 255);
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

typedef enum { BID_BTN_A = 0, BID_BTN_B = 1, BID_BTN_C = 2 } btnid_t;

typedef struct _naviitem {
  const char *caption;
  bool disabled;
  uint8_t (*wasPressed)();
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
  mainmenu_t *menu;
  loggerinfo_t loggerSelected;
  loggerinfo_t loggerDiscovered;
} appstatus_t;

appstatus_t app;

TFT_eSprite sprite = TFT_eSprite(&M5.Lcd);

/**
 * M5.BtnA.wasPressedの戻り値をそのまま返す。
 * インスタンスのメンバ関数を関数ポインタから呼ぶためのラッパー関数
 */
uint8_t M5BtnA_wasPressed() {
  // M5.BtnA.wasPressed()を実行して戻り値をそのまま返す
  return (M5.BtnA.wasPressed());
}

/**
 * M5.BtnB.wasPressedの戻り値をそのまま返す。
 * インスタンスのメンバ関数を関数ポインタから呼ぶためのラッパー関数
 */
uint8_t M5BtnB_wasPressed() {
  // M5.BtnB.wasPressed()を実行して戻り値をそのまま返す
  return (M5.BtnB.wasPressed());
}

/**
 * M5.BtnC.wasPressedの戻り値をそのまま返す。
 * インスタンスのメンバ関数を関数ポインタから呼ぶためのラッパー関数
 */
uint8_t M5BtnC_wasPressed() {
  // M5.BtnC.wasPressed()を実行して戻り値をそのまま返す
  return (M5.BtnC.wasPressed());
}

/**
 * NMEAコマンドのチェックサム計算を行う (printfのフォーマット%Xで送る値を生成)
 */
byte calcNmeaChecksum(char *cmd, byte len) {
  byte chk = 0;
  for (byte i = 0; (i < len) && (cmd[i] != 0); i++) {
    chk ^= (byte)(cmd[i]);
  }
  return chk;
}

/**
 * NMEAコマンドにチェックサムをつけてSerialBTに書き出す
 */
bool sendNmeaCommand(char *cmd, byte len) {
  if (gpsSerial.connected() == false) return false;

  word len2 = (word)(len) + 8;
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
  char cmdstr[32];
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

void drawDialog(const char *title, const char *msg) {
  const int16_t MARGIN = 4;
  const int16_t DIALOG_X = MARGIN;
  const int16_t DIALOG_Y = 24 + MARGIN;
  const int16_t DIALOG_W = M5.Lcd.width() - (MARGIN * 2);
  const int16_t DIALOG_H = (M5.Lcd.height() - 48) - (MARGIN * 2);
  const int16_t TITLE_X = 2;
  const int16_t TITLE_Y = 2;
  const int16_t MSG_X = 8;
  const int16_t MSG_Y = 28;

  sprite.setColorDepth(8);
  sprite.createSprite(DIALOG_W, DIALOG_H);

  sprite.fillRect(0, 0, sprite.width(), sprite.height(), LIGHTGREY);
  sprite.fillRect(0, 0, sprite.width(), 24, COLOR16(0, 128, 255));
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
  const char gpsPinCode[] = "0000";  // SPP接続時のPinコード。通常は"0000"で固定

  gpsSerial.register_callback(onBluetoothDiscoverCallback);
  gpsSerial.setTimeout(5000);
  gpsSerial.setPin(gpsPinCode);  // Pinコードをセット。これがないと繋がらない

  // char deviceNames[][] = {"747PRO GPS", "Holux_M-241"};

  drawDialog("Discovering", "Trying to connect to\n \"747PRO GPS\"...");
  if (gpsSerial.connect("747PRO GPS")) {
    drawDialog("Success", "logger found!");

    //    for (int i = 0; i < 6; i++) {
    //      Serial.printf("%02x", loggerDiscovered.address[i]);
    //    }
    //    Serial.printf("\n");

    char cmd[] = "PMTK605";
    sendNmeaCommand(cmd, sizeof(cmd));

    int n = 0;
    sprite.setCursor(0, 20);
    while (n < 500) {
      while (gpsSerial.available()) {
        Serial.print((char)gpsSerial.read());
        n++;
      }
      delay(1);
    }
    gpsSerial.disconnect();
  } else {
    drawDialog("Failed", "No logger found.");
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

void draw1bppBitmap(TFT_eSprite *spr, uint8_t *iconData, int16_t x0, int16_t y0,
                    int32_t color) {
  // uint8_t width,
  // uint8_t height,

  uint8_t iconW = *iconData++;
  uint8_t iconH = *iconData++;
  uint16_t pixelsTotal = (iconW * iconH);

  for (int i = 0; i < pixelsTotal; i++) {
    if (*iconData & (0b10000000 >> (i % 8))) {
      int px = x0 + (i % iconW);
      int py = y0 + (i / iconW);
      spr->drawPixel(px, py, color);
    }

    // 8ビット目まで見たら参照を次の1バイトへずらす
    if ((i % 8) == 7) iconData++;
  }
}

void drawApplicationIcon(TFT_eSprite *spr, int16_t x0, int16_t y0) {
  // アイコンのデータの描画色を決定
  uint16_t color = COLOR16(0, 64, 255);

  // アイコンを描画 (2つセット)
  draw1bppBitmap(spr, ICON_APP, x0, y0, color);
  draw1bppBitmap(spr, ICON_APP, (x0 + 12), (y0 + 8), color);
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
  draw1bppBitmap(spr, ICON_BT_BG, x0, y0, colorBG);
  draw1bppBitmap(spr, ICON_BT_FG, x0, y0, colorFG);
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
  for (int i = 0; i < 4; i++) {         // 端子部
    spr->fillRect((x0 + 4) + ((PIN_W + 1) * i), (y0 + 1), PIN_W, PIN_H,
                  COLOR_ICON_SD_PIN);
  }

  File root = SD.open("/");
  if (root) {  // SDカードが使用可能な場合
               // (ルートディレクトリがオープンできる)
    // テスト用に開いたファイルを閉じる
    root.close();

    {  // 状態表示
      spr->setTextSize(1);
      spr->setTextColor(COLOR_ICON_TEXT);
      spr->drawCentreString("SD", (x0 + 9), (y0 + 9), 1);
    }
  } else {  // SDカードが使用不能な場合
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
    // 4段階でバッテリーレベルを取得 0-bias%以下、2540%以下、65%以下、100%以下
    int8_t battBias = 15;
    int8_t battLevel = (battBias + M5.Power.getBatteryLevel()) / 25;

    // バッテリーレベルの表示
    for (int i = 0; i < battLevel; i++) {
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

  for (int i = 0; i < 6; i++) {
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

    uint8_t iconData[] = {
        48,         43,         0b00000000, 0b00000000, 0b00000000, 0b00000000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000011, 0b11000000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b00000000, 0b00000000, 0b00000011, 0b11111111, 0b11111111,
        0b11000000, 0b00000000, 0b00111100, 0b00000001, 0b11111111, 0b11111111,
        0b10000000, 0b00111100, 0b01111110, 0b00000000, 0b11111111, 0b11111111,
        0b00000000, 0b01111110, 0b01111110, 0b00000000, 0b01111111, 0b11111110,
        0b00000000, 0b01111110, 0b01111110, 0b00000000, 0b00111111, 0b11111100,
        0b00000000, 0b01111110, 0b01111110, 0b00000000, 0b00011111, 0b11111000,
        0b00000000, 0b01111110, 0b01111110, 0b00000000, 0b00001111, 0b11110000,
        0b00000000, 0b01111110, 0b01111110, 0b00000000, 0b00000111, 0b11100000,
        0b00000000, 0b01111110, 0b01111110, 0b00000000, 0b00000011, 0b11000000,
        0b00000000, 0b01111110, 0b01111110, 0b00000000, 0b00000001, 0b10000000,
        0b00000000, 0b01111110, 0b01111110, 0b00000000, 0b00000000, 0b00000000,
        0b00000000, 0b01111110, 0b01111111, 0b11111111, 0b11111111, 0b11111111,
        0b11111111, 0b11111110, 0b01111111, 0b11111111, 0b11111111, 0b11111111,
        0b11111111, 0b11111110, 0b01111111, 0b11111111, 0b11111111, 0b11111111,
        0b11111111, 0b11111110, 0b01111111, 0b11111111, 0b11111111, 0b11111111,
        0b11111111, 0b11111110, 0b01111111, 0b11111111, 0b11111111, 0b11111111,
        0b11111111, 0b11111110, 0b00111111, 0b11111111, 0b11111111, 0b11111111,
        0b11111111, 0b11111100};
    draw1bppBitmap(&sprite, iconData, (POS_X + (MENUBTN_W / 2) - (48 / 2)),
                   (POS_Y + 12), BLACK);

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
  for (int i = 0; i < 3; i++) {
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

void onNaviButtonPress(btnid_t bid) {
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

  // Memo:
  // mainMenu.items[i]にイベントハンドラを含めればループでイベント
  // ハンドラ呼び出しもループとif文で処理できるのだが、画面切り替え時の
  // インスタンス置き換えが若干面倒くさいうえに直感的にわかりづらいコードになる
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

  // SDカードの開始
  SD.begin(GPIO_NUM_4, SPI, SD_ACCESS_SPEED);

  // BT初期化
  gpsSerial.begin(APP_NAME, true);  // start bluetooth serial as a master

  app.navi = (navimenu_t *)malloc(sizeof(navimenu_t));
  memset(app.navi, 0, sizeof(navimenu_t));
  app.navi->onButtonPress = &(onNaviButtonPress);
  app.navi->items[0].caption = "Prev";
  app.navi->items[0].wasPressed = &(M5BtnA_wasPressed);
  app.navi->items[1].caption = "Next";
  app.navi->items[1].wasPressed = &(M5BtnB_wasPressed);
  app.navi->items[2].caption = "Select";
  app.navi->items[2].wasPressed = &(M5BtnC_wasPressed);

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
  for (int i = 0; i < 3; i++) {
    naviitem_t *ni = &(app.navi->items[i]);  // 判定対象のボタンを取得

    // 物理ボタンが押されていればイベントハンドラを呼び出す
    if ((ni->disabled == false) && (ni->wasPressed())) {
      if (app.navi->onButtonPress != NULL) {
        app.navi->onButtonPress((btnid_t)(i));
      }
      break;  // 左・中・右の順に1回だけ処理。同時に複数の処理はしない
    }
  }

  delay(50);
}
