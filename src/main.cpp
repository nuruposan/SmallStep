// #include <BluetoothSerial.h>
#include <BluetoothSerial.h>
#include <M5Stack.h>
#include <SD.h>

#define SD_ACCESS_SPEED 15000000
#define COLOR16(r, g, b) (uint16_t)((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

// BluetoothSerial gpsSerial;

const int16_t COLOR_SCR_BACK = BLACK;
const int16_t COLOR_BAR_BACK = LIGHTGREY;
const int16_t COLOR_BAR_TEXT = BLACK;
const int16_t COLOR_OBJ_BODY = BLACK;
const int16_t COLOR_OBJ_TEXT = LIGHTGREY;
const int16_t COLOR_OBJ_ERROR = RED;
const int16_t COLOR_SD_PIN = OLIVE;
const int16_t COLOR_NAVI_BODY = LIGHTGREY;
const int16_t COLOR_NAVI_TEXT = COLOR16(0, 32, 64);
const int16_t COLOR_MENU_BODY = DARKGREY;
const int16_t COLOR_MENU_SEL_BORDER = RED;
const int16_t COLOR_MENU_SEL_BODY = LIGHTGREY;
const int16_t COLOR_MENU_TEXT = COLOR16(0, 32, 64);

/**
 * タイトルバー表示
 */
void drawTitleBar() {
  int16_t TITLE_BAR_W = 320;
  int16_t TITLE_BAR_H = 24;
  int16_t TITLE_BAR_X = 0;
  int16_t TITLE_BAR_Y = 0;
  int16_t SDCARD_ICON_W = 17;
  int16_t SDCARD_ICON_H = TITLE_BAR_H - 4;
  int16_t SDCARD_ICON_X = 276;
  int16_t SDCARD_ICON_Y = TITLE_BAR_Y + 2;
  int16_t SDCARD_PIN_W = 2;
  int16_t SDCARD_PIN_H = 4;
  int16_t BATT_ICON_W = 21;
  int16_t BATT_ICON_H = TITLE_BAR_H - 4;
  int16_t BATT_ICON_X = 297;
  int16_t BATT_ICON_Y = TITLE_BAR_Y + 2;

  // タイトルバークリア
  M5.Lcd.fillRect(0, 0, TITLE_BAR_W, TITLE_BAR_H, COLOR_BAR_BACK);

  {  // タイトル文字の表示
    M5.Lcd.setTextFont(4);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(COLOR_BAR_TEXT);
    M5.Lcd.setCursor(2, 2);
    M5.Lcd.printf("Nanika on M5Stack");
  }

  {  // SDカード状態の表示
    // SDカードの外枠を描画
    M5.Lcd.fillRoundRect((SDCARD_ICON_X + 2), SDCARD_ICON_Y,
                         (SDCARD_ICON_W - 2), 8, 2, COLOR_OBJ_BODY);  // 本体
    M5.Lcd.fillRoundRect(SDCARD_ICON_X, (SDCARD_ICON_Y + 6), SDCARD_ICON_W,
                         (SDCARD_ICON_H - 6), 2, COLOR_OBJ_BODY);  // 本体
    for (int i = 0; i < 4; i++) {                                  // 端子部
      M5.Lcd.fillRect((SDCARD_ICON_X + 4) + ((SDCARD_PIN_W + 1) * i),
                      (SDCARD_ICON_Y + 1), SDCARD_PIN_W, SDCARD_PIN_H,
                      COLOR_SD_PIN);
    }

    File root = SD.open("/");
    if (root) {  // SDカードが使用可能 (ルートディレクトリがオープンできる)
      // テスト用に開いたファイルを閉じる
      root.close();

      // 状態表示
      M5.Lcd.setTextFont(1);
      M5.Lcd.setTextSize(1);
      M5.Lcd.setTextColor(COLOR_OBJ_TEXT);
      M5.Lcd.drawCentreString("SD", (SDCARD_ICON_X + 9), (SDCARD_ICON_Y + 9),
                              1);
    } else {  // SDカードが使用不能
      // 状態表示 ○
      M5.Lcd.drawEllipse((SDCARD_ICON_X + 8), (SDCARD_ICON_Y + 12), 5, 5,
                         COLOR_OBJ_ERROR);
      M5.Lcd.drawLine((SDCARD_ICON_X + 11), (SDCARD_ICON_Y + 9),
                      (SDCARD_ICON_X + 5), (SDCARD_ICON_Y + 15),
                      COLOR_OBJ_ERROR);
    }
  }

  {  // バッテリー状態の表示
    // バッテリーの外枠を描画
    M5.Lcd.fillRoundRect(BATT_ICON_X, BATT_ICON_Y, (BATT_ICON_W - 2),
                         BATT_ICON_H, 2, COLOR_OBJ_BODY);  // 本体
    M5.Lcd.fillRect((BATT_ICON_X + (BATT_ICON_W - 2)), (BATT_ICON_Y + 6), 2, 8,
                    COLOR_OBJ_BODY);  // 凸部

    if (M5.Power.canControl()) {
      // 4段階でバッテリーレベルを取得 15%以下、40%以下、65%以下、100%以下
      int8_t battBias = 15;
      int8_t battLevel = (battBias + M5.Power.getBatteryLevel()) / 25;

      // バッテリーレベルの表示
      for (int i = 0; i < battLevel; i++) {
        M5.Lcd.fillRect((BATT_ICON_X + 2) + (4 * i), (BATT_ICON_Y + 4), 3, 12,
                        COLOR_OBJ_TEXT);
      }
      if (battLevel == 0) {
        M5.Lcd.fillRect((BATT_ICON_X + 2), (BATT_ICON_Y + 4), 3, 12,
                        COLOR_OBJ_ERROR);
      }
    } else {  // バッテリーレベル不明の表示
      M5.Lcd.setTextFont(1);
      M5.Lcd.setTextSize(2);
      M5.Lcd.setTextColor(COLOR_OBJ_ERROR);
      M5.Lcd.setCursor(BATT_ICON_X + 4, 4);
      M5.Lcd.printf("?");
    }
  }
}

/**
 * ボタンの操作ナビ表示
 */
void drawNavigation() {
  int16_t NAVI_X = 25;
  int16_t NAVI_Y = 216;
  int16_t NAVI_BTN_W = 80;
  int16_t NAVI_BTN_H = 24;
  int16_t NAVI_BDR_W = 15;

  const char* NAV_CAPTIONS[3] = {"Prev", "Next", "OK"};

  M5.Lcd.setTextFont(4);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(COLOR_NAVI_TEXT);

  for (int i = 0; i < 3; i++) {
    int16_t NAVI_BTN_X = NAVI_X + ((NAVI_BTN_W + NAVI_BDR_W) * i);
    int16_t NAVI_BTN_Y = NAVI_Y;

    M5.Lcd.fillRoundRect(NAVI_BTN_X, NAVI_BTN_Y, NAVI_BTN_W, NAVI_BTN_H, 2,
                         COLOR_NAVI_BODY);

    M5.Lcd.drawCentreString(NAV_CAPTIONS[i], (NAVI_BTN_X + (NAVI_BTN_W / 2)),
                            (NAVI_BTN_Y + 2), 4);
  }
}

void drawButton() {
  int16_t SCRN_X = 0;
  int16_t SCRN_Y = 30;
  int16_t SCRN_W = 320;
  int16_t SCRN_H = 180;
  int16_t BORDER_W = 4;
  int16_t BORDER_H = 5;
  int16_t BUTTON_W = SCRN_W / 3 - (BORDER_W * 2);
  int16_t BUTTON_H = SCRN_H / 2 - (BORDER_H * 2);

  for (int i = 0; i < 6; i++) {
    int16_t BUTTON_X =
        SCRN_X + BORDER_W + (BUTTON_W + (BORDER_W * 2)) * (i % 3);
    int16_t BUTTON_Y =
        SCRN_Y + BORDER_H + ((BUTTON_H + (BORDER_H * 2)) * (i / 3));

    if (i == 0) {  // 選択状態メニュー
      M5.Lcd.fillRoundRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, 4,
                           COLOR_MENU_SEL_BODY);
      M5.Lcd.drawRoundRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, 4,
                           COLOR_MENU_SEL_BORDER);
    } else {  // 非選択状態メニュー
      M5.Lcd.fillRoundRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, 4,
                           COLOR_MENU_BODY);
    }
  }
}

void setup() {
  // M5Stackの初期化
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setBrightness(64);

  // SDカードの開始
  SD.begin(GPIO_NUM_4, SPI, SD_ACCESS_SPEED);

  // スクリーンを全域クリア
  M5.Lcd.clear(COLOR_SCR_BACK);

  // タイトルバーを描画
  drawTitleBar();
  drawNavigation();
  drawButton();
}

void loop() {
  M5.update();

  
  delay(50);
}
