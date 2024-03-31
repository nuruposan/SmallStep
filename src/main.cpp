#include <BluetoothSerial.h>
#include <M5Stack.h>
#include <SdFat.h>

#include "LoggerManager.hpp"
#include "MtkParser.hpp"
#include "Resources.h"

#define SD_ACCESS_SPEED 15000000  // 20MHz may cause SD card error
#define COLOR16(r, g, b) (int16_t)((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

#define POWER_OFF_TIME 100000  // unit: msec
#define LOOP_INTERVAL 50       // uint: msec

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
  const uint8_t *iconData;
  bool disabled;
  void (*onSelected)();
} menuitem_t;

typedef struct _mainmenu {
  menuitem_t items[6];
  int8_t selectedIndex;
} mainmenu_t;

typedef struct _appstatus {
  navimenu_t *navi;
  navimenu_t *prevNavi;
  mainmenu_t menu;
  Button *buttons[3];
  uint32_t idleTimer;
  bool sppConnected;
  uint8_t sppClientAddr[6];
} appstatus_t;

typedef struct appconfig {
  uint8_t length;            // app - sizeof(appconfig_t)
  uint32_t release;          // app - release number
  uint32_t sdAccessSpeed;    // app - SD card access speed (Hz)
  uint8_t loggerAddress[6];  // app - address of paired logger
  uint8_t loggerModel;       // logger model ID of pairded logger
  logformat_t logFormat;     // logger - what to record
  uint16_t logByDistance;  // logger - auto log by distance (meter, 0: disable)
  uint16_t logByTime;      // logger - auto log by time (seconds, 0: disable)
  uint16_t logBySpeed;     // logger - auto log by speed (meter/sec, 0:disabled)
  recordmode_t reccordMode;  // logger - log record mode
  trackmode_t trackMode;     // parser - how to divide/put tracks
  float timeOffset;  // parser - timezone offset in hours (-12.0 to 12.0)
} appconfig_t;

// ******** function prototypes ********
void saveAppConfig();
bool loadAppConfig();
void onBluetoothDiscoverCallback(esp_spp_cb_event_t, esp_spp_cb_param_t *);
void downloadProgress(int32_t progress);
void onDownloadMenuSelected();
void onFixRTCMenuSelected();
void onSetPresetConfigMenuSelected();
void onPairToLoggerMenuSelected();
void onEraseLogMenuSelected();
void onAppSettingsMenuSelected();
void drawProgressBar(uint8_t);
void drawDialog(const char *);
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
const char APP_VERSION[] = "v0.01";
const char LCD_BRIGHTNESS = 10;

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

TFT_eSprite sprite = TFT_eSprite(&M5.Lcd);
SdFat SDcard;
LoggerManager logger = LoggerManager();
MtkParser parser = MtkParser();
appstatus_t appStatus;
appconfig_t appConfig;

void bluetoothCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  switch (event) {
    case ESP_SPP_OPEN_EVT:  // Bluetooth connection established
      // store the client address
      memcpy(appStatus.sppClientAddr, param->open.rem_bda, 6);

      // store the connection status and update the title bar
      appStatus.sppConnected = true;
      drawTitleBar();

      break;
    case ESP_SPP_CLOSE_EVT:  // Bluetooth connection closed
      // store the connection status and update the title bar
      appStatus.sppConnected = false;
      drawTitleBar();

      break;
  }
}

void drawProgressBar(uint8_t progress) {
  // 進捗率の指定のレンジ補正
  if (progress > 100) progress = 100;

  M5.Lcd.progressBar(10, 180, 300, 16, progress);
}

void drawDialog(const char *title) {
  const int16_t MARGIN = 4;
  const int16_t DIALOG_X = MARGIN;
  const int16_t DIALOG_Y = 24 + MARGIN;
  const int16_t DIALOG_W = M5.Lcd.width() - (MARGIN * 2);
  const int16_t DIALOG_H = (M5.Lcd.height() - 48) - (MARGIN * 2);
  const int16_t TITLE_X = 2;
  const int16_t TITLE_Y = 2;

  // initialize the sprite
  sprite.createSprite(DIALOG_W, DIALOG_H);

  // draw the dialog frame
  sprite.fillRect(0, 0, sprite.width(), sprite.height(), LIGHTGREY);
  sprite.fillRect(0, 0, sprite.width(), 26, COLOR16(0, 128, 255));
  sprite.drawRect(0, 0, sprite.width(), sprite.height(), DARKGREY);

  // print the title
  sprite.setTextSize(1);
  sprite.setTextColor(BLACK);
  sprite.drawString(title, TITLE_X, TITLE_Y, 4);

  // transfer the sprite to the LCD
  sprite.pushSprite(DIALOG_X, DIALOG_Y);
  sprite.deleteSprite();

  // set the default font and color for the message text
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(1);
}

/**
    private static final String QUERY_LOG_BY_TIME_COMMAND  = "PMTK182,2,3,0";
    private static final String QUERY_LOG_BY_TIME_RESPONSE =
 "^\\$(PMTK182,3,3,(\\w+))\\*(\\w+)$"; private static final String
 QUERY_LOG_BY_DIST_COMMAND  = "PMTK182,2,4,0"; private static final String
 QUERY_LOG_BY_DIST_RESPONSE = "^\\$(PMTK182,3,4,(\\w+))\\*(\\w+)$"; private
 static final String QUERY_LOG_BY_SPD_COMMAND   = "PMTK182,2,5,0"; private
 static final String QUERY_LOG_BY_SPD_RESPONSE  =
 "^\\$(PMTK182,3,5,(\\w+))\\*(\\w+)$"; private static final String
 static final String QUERY_LOG_STATUS_COMMAND  = "PMTK182,2,7"; private static
 final String QUERY_LOG_STATUS_RESPONSE = "^\\$(PMTK182,3,7,(\\w+))\\*(\\w+)$";
        private static final String QUERY_RCD_ADDR_COMMAND    = "PMTK182,2,8";
        private static final String QUERY_RCD_ADDR_RESPONSE   =
 "^\\$(PMTK182,3,8,(\\w+))\\*(\\w+)$"; private static final String
 QUERY_RCD_RCNT_COMMAND    = "PMTK182,2,10"; private static final String
 QUERY_RCD_RCNT_RESPONSE   = "^\\$(PMTK182,3,10,(\\w+))\\*(\\w+)$"; private
 CLEAR_MEMORY_COMMAND   = "PMTK182,6,1"; private static final String
 CLEAR_MEMORY_RESPONSE  = "^\\$(PMTK001,182,6,(3))\\*(\\w+)$"; private static
 final String FACTORY_RESET_COMMAND  = "PMTK104"; private static final String
 RESTART_RESPONSE       = "^\\$(PMTK010,(001))\\*(\\w+)$";
 */

void downloadProgress(int32_t progress) { drawProgressBar(progress); }

bool validAddress(uint8_t *addr) {
  uint16_t addrsum = 0;
  for (int i = 0; i < 6; i++) {
    addrsum += addr[i];
  }
  return (addrsum > 0);
}

/**
 *
 */
void onDownloadMenuSelected() {
  const char TEMP_BIN[] = "download.bin";
  const char TEMP_GPX[] = "download.gpx";
  const char GPX_PREFIX[] = "gpslog_";
  const char DATETIME_FMT[] = "%04d%02d%02d-%02d%02d%02d";

  // draw a dialog frame
  drawDialog("Download Log");

  if (!validAddress(appConfig.loggerAddress)) {
    M5.Lcd.setTextColor(RED);
    M5.Lcd.drawString("Pairing has not been done yet.", 10, (60));
    M5.Lcd.drawString("Please pair with your logger first.", 10, (60 + 18));
    return;
  }

  // open a file for download
  File32 binFileW = SDcard.open(TEMP_BIN, (O_CREAT | O_WRITE | O_TRUNC));
  if (!binFileW) {
    M5.Lcd.setTextColor(RED);
    M5.Lcd.drawString("Could not open files on the SD card.", 10, (60));
    M5.Lcd.drawString("- Make sure SD card is available.", 10, (60 + 18));
    M5.Lcd.drawString("  It must be a <16GB, FAT32 formatted card.", 10,
                      (60 + 36));
    return;
  }

  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.drawString("Connecting to GPS logger...", 10, 60);

  if (!logger.connect(appConfig.loggerAddress)) {
    M5.Lcd.setTextColor(RED);
    M5.Lcd.drawString("Connecting to GPS logger... failed.", 10, 60);
    M5.Lcd.drawString("- Make sure Bluetooth on logger is enabled", 10,
                      (60 + 18));
    M5.Lcd.drawString("- Power cycling may fix this problem", 10, (60 + 36));

    binFileW.close();
    return;
  }

  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.drawString("Connecting to GPS logger... done.", 10, 60);
  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.drawString("Downloading log data...", 10, (60 + 18));

  if (!logger.downloadLogData(&binFileW, &downloadProgress)) {
    M5.Lcd.setTextColor(RED);
    M5.Lcd.drawString("Downloading log data... failed.", 10, (60 + 18));
    M5.Lcd.drawString("- Keep GPS logger close to this device", 10, (60 + 36));
    M5.Lcd.drawString("- Power cycling may fix this problem", 10, (60 + 54));

    logger.disconnect();
    binFileW.close();
    return;
  }
  logger.disconnect();
  binFileW.close();

  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.drawString("Downloading log data... done.", 10, (60 + 18));
  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.drawString("Converting data to GPX file...", 10, (60 + 36));
  M5.Lcd.fillRect(11, 181, 299, 14, LIGHTGREY);

  // open the downloaded binary file and create a new GPX file
  File32 binFileR = SDcard.open(TEMP_BIN, (O_READ));
  File32 gpxFileW = SDcard.open(TEMP_GPX, (O_CREAT | O_WRITE | O_TRUNC));

  //
  if ((!binFileR) || (binFileR.size() == 0) || (!gpxFileW)) {
    M5.Lcd.setTextColor(RED);
    M5.Lcd.drawString("Converting data to GPX file... failed.", 10, (60 + 36));
    M5.Lcd.drawString("- Reformat SD card may fix problem", 10, (60 + 54));
    M5.Lcd.drawString("  (Maybe filesystem is corrupted?)", 10, (60 + 72));

    if (!binFileR) binFileR.close();
    if (!gpxFileW) gpxFileW.close();
    return;
  }

  parser.setTimeOffset(9.0);
  parser.convert(&binFileR, &gpxFileW, &downloadProgress);

  binFileR.close();
  gpxFileW.close();

  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.drawString("Converting data to GPX file... done.", 10, (60 + 36));

  time_t tt1 = parser.getFirstRecordTime();
  struct tm *tm1 = localtime(&tt1);
  char tm1str[16], gpxName[40];

  // make a unique name for the output GPX file
  sprintf(tm1str, DATETIME_FMT, (tm1->tm_year + 1900), (tm1->tm_mon + 1),
          tm1->tm_mday, tm1->tm_hour, tm1->tm_min, tm1->tm_sec);
  sprintf(gpxName, "%s%s.gpx", GPX_PREFIX, tm1str);
  for (uint16_t i = 2; SDcard.exists(gpxName) && (i < 65535); i++) {
    sprintf(gpxName, "%s%s_%02d.gpx", GPX_PREFIX, tm1str, i);
  }
  SDcard.rename(TEMP_GPX, gpxName);

  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.drawString("The downloaded GPS log is saved to", 10, (60 + 54));
  M5.Lcd.drawString(gpxName, 10, (60 + 72));
}

/**
 *
 */
void onFixRTCMenuSelected() {
  drawDialog("Fix RTC time");

  if (!validAddress(appConfig.loggerAddress)) {
    M5.Lcd.setTextColor(RED);
    M5.Lcd.drawString("The logger address is not set.", 10, 60);
    M5.Lcd.drawString("Please pair with the logger first.", 10, (60 + 18));

    Serial.printf("DEBUG: logger address is not set.\n");
    return;
  }

  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.drawString("Connecting to GPS logger...", 10, 60);

  // connect to the logger
  if (!logger.connect(appConfig.loggerAddress)) {
    M5.Lcd.setTextColor(RED);
    M5.Lcd.drawString("Connecting to GPS logger... failed.", 10, 60);
    M5.Lcd.drawString("- Make sure Bluetooth on logger is enabled", 10,
                      (60 + 18));
    M5.Lcd.drawString("- Power cycling may fix this problem", 10, (60 + 36));

    Serial.printf("DEBUG: failed to connect to the GPS logger.\n");
    return;
  }

  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.drawString("Connecting to GPS logger... done.", 10, 60);
  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.drawString("Fixing RTC time...", 10, 60 + 18);

  if (!logger.fixRTCdatetime()) {
    M5.Lcd.setTextColor(RED);
    M5.Lcd.drawString("Fixing RTC time... failed.", 10, 60 + 18);
    M5.Lcd.drawString("- Keep GPS logger close to this device", 10, (60 + 36));
    M5.Lcd.drawString("- Power cycling may fix this problem", 10, (60 + 54));

    logger.disconnect();
    return;
  }

  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.drawString("Fixing RTC time... done.", 10, 60 + 18);
  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.drawString("The correct time will be recorded", 10, 60 + 36);
  M5.Lcd.drawString("from the next record onward.", 10, 60 + 54);

  logger.disconnect();
}

/**
 *
 */
void onSetPresetConfigMenuSelected() {
  Serial.printf("[DEBUG] onSetPresetConfigMenuSelected\n");
}

/**
 *
 */
void onPairToLoggerMenuSelected() {
  const String DEVICE_NAMES[] = {"747PRO GPS", "HOLUX_M-241"};

  drawDialog("Pair with Logger");

  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.drawString("Discovering GPS logger...", 10, 60);

  for (int i = 0; i < 2; i++) {
    M5.Lcd.setTextColor(BLUE);
    M5.Lcd.setCursor(10, (60 + (18 * (i + 1))));
    M5.Lcd.printf("- %s", DEVICE_NAMES[i]);

    M5.Lcd.setCursor(10, (60 + (18 * (i + 1))));
    if (!logger.connect(DEVICE_NAMES[i])) {  // try to connect
      // failed to connect
      M5.Lcd.setTextColor(RED);
      M5.Lcd.printf("- %s :  not found.", DEVICE_NAMES[i]);

      // try to discover the next
      continue;
    }

    delay(500);
    logger.disconnect();

    uint8_t addr[6];
    memcpy(addr, appStatus.sppClientAddr, 6);

    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.printf("- %s : found.", DEVICE_NAMES[i]);
    M5.Lcd.setTextColor(BLUE);
    M5.Lcd.setCursor(10, (60 + (18 * (i + 2))));
    M5.Lcd.printf("Successfully paired with the discovered logger.");
    M5.Lcd.setCursor(10, (60 + (18 * (i + 3))));
    M5.Lcd.printf("The logger address is %02X:%02X:%02X:%02X:%02X:%02X.",
                  addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    // copy the logger address to the configuration
    memcpy(appConfig.loggerAddress, addr, 6);
    saveAppConfig();
    break;
  }
}

/**
 *
 */
void onEraseLogMenuSelected() {
  uint8_t addr[] = {0x00, 0x1C, 0x88, 0x22, 0x1E, 0x57};  // 747pro
  memcpy(appConfig.loggerAddress, addr, 6);
}

/**
 *
 */
void onAppSettingsMenuSelected() {
  Serial.printf("[DEBUG] onAppSettingsMenuSelected\n");
  uint8_t addr[] = {0x00, 0x1B, 0xC1, 0x07, 0xB3, 0xD5};  // M-241
  memcpy(appConfig.loggerAddress, addr, 6);
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
  uint16_t colorBG = (appStatus.sppConnected) ? BLUE : DARKGREY;
  uint16_t colorFG = WHITE;

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

  // SDカードの外枠と端子を描画
  spr->fillRoundRect((x0 + 2), y0, (ICON_W - 2), 8, 2,
                     COLOR_ICON_BODY);  // 本体
  spr->fillRoundRect(x0, (y0 + 6), ICON_W, (ICON_H - 6), 2,
                     COLOR_ICON_BODY);  // 本体
  for (int16_t i = 0; i < 4; i++) {     // 端子部
    spr->fillRect((x0 + 4) + ((PIN_W + 1) * i), (y0 + 1), PIN_W, PIN_H,
                  COLOR_ICON_SD_PIN);
  }

  if (SDcard.card()->cardSize() > 0) {  // SD card is available
    // put "SD" text
    spr->setTextSize(1);
    spr->setTextColor(COLOR_ICON_TEXT);
    spr->drawCentreString("SD", (x0 + 9), (y0 + 9), 1);
  } else {  // SD card is not available
    // put φ mark
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

  // アプリアイコンを描画
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
    if (i == appStatus.menu.selectedIndex) {  // 選択状態メニュー
      sprite.fillRoundRect(POS_X, POS_Y, MENUBTN_W, MENUBTN_H, 4,
                           COLOR_MENU_SEL_BACK);
      sprite.drawRoundRect(POS_X, POS_Y, MENUBTN_W, MENUBTN_H, 4,
                           COLOR_MENU_SEL_BORDER);
    } else {  // 非選択状態メニュー
      sprite.fillRoundRect(POS_X, POS_Y, MENUBTN_W, MENUBTN_H, 4,
                           COLOR_MENU_BACK);
    }

    draw1bitBitmap(&sprite, appStatus.menu.items[i].iconData,
                   (POS_X + (MENUBTN_W / 2) - (48 / 2)), (POS_Y + 12),
                   COLOR16(0, 32, 32));

    // ボタンのキャプションを描画
    sprite.setTextSize(1);
    sprite.setTextColor(COLOR_MENU_TEXT);
    sprite.drawCentreString(appStatus.menu.items[i].caption,
                            POS_X + (MENUBTN_W / 2), POS_Y + (MENUBTN_H - 12),
                            1);
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
    naviitem_t *ni = &(appStatus.navi->items[i]);

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
  appStatus.menu.selectedIndex = (appStatus.menu.selectedIndex + 5) % 6;
  drawMainMenu();
}

void onNaviNextButtonClick() {
  appStatus.menu.selectedIndex = (appStatus.menu.selectedIndex + 1) % 6;
  drawMainMenu();
}

void onNaviEnterButtonClick() {
  menuitem_t *mi = &(appStatus.menu.items[appStatus.menu.selectedIndex]);
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

void saveAppConfig() {
  uint8_t cfgdata[sizeof(appconfig_t)];
  uint8_t chk = 0;
  memcpy(cfgdata, &appConfig, sizeof(appconfig_t));

  // write configuration data to EEPROM
  for (int8_t i = 0; i < sizeof(appconfig_t); i++) {
    EEPROM.write(i, cfgdata[i]);
    chk ^= cfgdata[i];
  }
  EEPROM.write(sizeof(appconfig_t), chk);
  EEPROM.commit();

  Serial.printf("SmallStep.saveConfig: configuration data saved\n");

  return;
}

bool loadAppConfig() {
  uint8_t cfgdata[sizeof(appconfig_t)];
  uint8_t chk = 0;
  for (int8_t i = 0; i < sizeof(appconfig_t); i++) {
    cfgdata[i] = EEPROM.read(i);
    chk ^= cfgdata[i];
  }
  memcpy(&appConfig, cfgdata, sizeof(appconfig_t));

  // if checksum is invalid, clear configuration
  if ((appConfig.length != sizeof(appConfig)) ||
      (EEPROM.read(sizeof(appconfig_t) != chk))) {
    memset(&appConfig, 0, sizeof(appconfig_t));
    appConfig.length = sizeof(appconfig_t);
    saveAppConfig();

    return false;
  }

  Serial.printf("SmallStep.loadConfig: configuration restored\n");

  return true;
}

void setup() {
  // start the serial
  Serial.begin(115200);

  // initialize the M5Stack
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setBrightness(LCD_BRIGHTNESS);
  M5.Lcd.clearDisplay(BLACK);

  // zero-clear global variables (status & config)
  memset(&appStatus, 0, sizeof(appStatus));
  memset(&appConfig, 0, sizeof(appConfig));

  EEPROM.begin(sizeof(appconfig_t) + 1);
  loadAppConfig();

  // initialize the SD card
  if (!SDcard.begin(GPIO_NUM_4, SD_ACCESS_SPEED)) {
    Serial.printf("SmallStep.setup: failed to initialize the SD card.\n");
  }

  logger.setEventCallback(bluetoothCallback);

  // initialize the screen sprite
  sprite.setColorDepth(8);

  // instance array of buttons
  appStatus.buttons[0] = &M5.BtnA;
  appStatus.buttons[1] = &M5.BtnB;
  appStatus.buttons[2] = &M5.BtnC;

  // navigation menu
  appStatus.navi = (navimenu_t *)malloc(sizeof(navimenu_t));
  memset(appStatus.navi, 0, sizeof(navimenu_t));
  appStatus.navi->onButtonPress = &(onMainNaviButtonPress);
  appStatus.navi->items[0].caption = "Prev";
  appStatus.navi->items[1].caption = "Next";
  appStatus.navi->items[2].caption = "Select";
  appStatus.prevNavi = NULL;

  // main menu icons
  appStatus.menu.items[0].caption = "Download Log";
  appStatus.menu.items[0].iconData = ICON_DOWNLOAD_LOG;
  appStatus.menu.items[0].onSelected = &(onDownloadMenuSelected);
  appStatus.menu.items[1].caption = "Fix RTC time";
  appStatus.menu.items[1].iconData = ICON_FIX_RTC;
  appStatus.menu.items[1].onSelected = &(onFixRTCMenuSelected);
  appStatus.menu.items[2].caption = "Set Preset Cfg";
  appStatus.menu.items[2].iconData = ICON_SET_PRESET;
  appStatus.menu.items[2].onSelected = &(onSetPresetConfigMenuSelected);
  appStatus.menu.items[3].caption = "Erase Log";
  appStatus.menu.items[3].iconData = ICON_ERASE_LOG;
  appStatus.menu.items[3].onSelected = &(onEraseLogMenuSelected);
  appStatus.menu.items[4].caption = "Pair w/ Logger";
  appStatus.menu.items[4].iconData = ICON_PAIR_LOGGER;
  appStatus.menu.items[4].onSelected = &(onPairToLoggerMenuSelected);
  appStatus.menu.items[5].caption = "App Settings";
  appStatus.menu.items[5].iconData = ICON_APP_SETTINGS;
  appStatus.menu.items[5].onSelected = &(onAppSettingsMenuSelected);

  // draw the initial screen
  drawTitleBar();
  drawNaviBar();
  drawMainMenu();

  appStatus.idleTimer = millis() + POWER_OFF_TIME;
}

void loop() {
  M5.update();  // update button status

  // physical button event handling
  for (int16_t i = 0; i < 3; i++) {
    Button *b = appStatus.buttons[i];  // get a button object to check

    // call the event handler if the button is pressed
    if (b->wasPressed()) {
      appStatus.navi->onButtonPress((btnid_t)(i));
      appStatus.idleTimer = millis() + POWER_OFF_TIME;  // reset the idle timer

      // 1回押したら次のボタンの処理はしない
      break;
    }
  }

  // if the idle timer is expired, shutdown the system
  if (millis() > appStatus.idleTimer) {
    // debug message
    Serial.printf(
        "SmallStep.loop: "
        "powering off the system due to inactivity.\n");

    // shutdown the system
    SDcard.end();         // stop SD card access
    M5.Power.powerOFF();  // power off
  }

  delay(LOOP_INTERVAL);
}
