#include <BluetoothSerial.h>
#include <M5Stack.h>
#include <SD.h>

#define SD_ACCESS_SPEED 15000000
#define COLOR16(r, g, b) (uint16_t)((r>>3)<<11)|((g>>2)<<5)|(b>>3)

// BluetoothSerial gpsSerial;

const char APP_NAME[] = "SmallStepM5S";
const char APP_VERSION[] = "v0.01";

const int16_t COLOR_SCR_BACK = BLACK;
const int16_t COLOR_BAR_BACK = COLOR16(64, 255, 192);
const int16_t COLOR_BAR_TEXT = BLACK;
const int16_t COLOR_ICON_BACK = BLACK;
const int16_t COLOR_ICON_METER = LIGHTGREY;
const int16_t COLOR_ICON_ERROR = RED;
const int16_t COLOR_ICON_SD_PIN = OLIVE;
const int16_t COLOR_MENU_BODY = DARKGREY;
const int16_t COLOR_MENU_SEL_BORDER = RED;
const int16_t COLOR_MENU_SEL_BODY = LIGHTGREY;
const int16_t COLOR_MENU_TEXT = BLACK;
const int16_t COLOR_NAVI_BODY = LIGHTGREY;
const int16_t COLOR_NAVI_TEXT = BLACK;

typedef enum _appstatus {
  APP_IN_MENU  = 0x000000,
  APP_DO_MENU1 = 0x001000,
  APP_DO_MENU2 = 0x002000,
  APP_DO_MENU3 = 0x003000,
  APP_DO_MENU4 = 0x004000,
  APP_DO_MENU5 = 0x005000,
  APP_DO_MENU6 = 0x006000
} appstatus_t;

typedef struct _naviitem {
  const char *caption;
  bool disabled;
  uint8_t (*wasPressed)();
  void (*onClick)();
} naviitem_t;

typedef struct _menuitem {
  const char *caption;
  uint16_t *iconData;
  bool disabled;
  void (*onSelected)();
} menuitem_t;

typedef struct _mainmenu {
  menuitem_t items[6];
  int8_t selectedIndex;
} mainmenu_t;

//appstatus_t status;

naviitem_t naviButtons[3];
mainmenu_t mainMenu;

/**
 * M5.BtnA.wasPressedの戻り値をそのまま返す。
 * インスタンスのメンバ関数を関数ポインタから呼ぶためのラッパー関数
 */
uint8_t M5BtnA_wasPressed() {
  return (M5.BtnA.wasPressed());
}

/**
 * M5.BtnB.wasPressedの戻り値をそのまま返す。
 * インスタンスのメンバ関数を関数ポインタから呼ぶためのラッパー関数
 */
uint8_t M5BtnB_wasPressed() {
  return (M5.BtnB.wasPressed());
}

/**
 * M5.BtnC.wasPressedの戻り値をそのまま返す。
 * インスタンスのメンバ関数を関数ポインタから呼ぶためのラッパー関数
 */
uint8_t M5BtnC_wasPressed() {
  return (M5.BtnC.wasPressed());
}

/**
 * 
 */
void onDownloadLogMenuSelected() {
  Serial.printf("[DEBUG] onDownloadLogMenuSelected\n");
}

/**
 * 
 */
void onFixRTCMenuSelected() {
  Serial.printf("[DEBUG] onFixRTCMenuSelected\n");
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

/**
 * SDカードの状態表示アイコンの描画
 */
void drawSDcardIcon(const int16_t POS_X, const int16_t POS_Y) {
  const int16_t ICON_W = 17;
  const int16_t ICON_H = 20;
  const int16_t PIN_W = 2;
  const int16_t PIN_H = 4;

  // SDカードの外枠を描画
  M5.Lcd.fillRoundRect((POS_X + 2), POS_Y, (ICON_W - 2), 8, 2, COLOR_ICON_BACK);  // 本体
  M5.Lcd.fillRoundRect(POS_X, (POS_Y + 6), ICON_W, (ICON_H - 6), 2, COLOR_ICON_BACK);  // 本体
  for (int i = 0; i < 4; i++) {                                  // 端子部
    M5.Lcd.fillRect((POS_X + 4) + ((PIN_W + 1) * i), (POS_Y + 1), PIN_W, PIN_H, COLOR_ICON_SD_PIN);
  }

  File root = SD.open("/");
  if (root) {  // SDカードが使用可能 (ルートディレクトリがオープンできる)
    // テスト用に開いたファイルを閉じる
    root.close();

    { // 状態表示
      M5.Lcd.setTextSize(1);
      M5.Lcd.setTextColor(COLOR_ICON_METER);
      M5.Lcd.drawCentreString("SD", (POS_X + 9), (POS_Y + 9), 1);
    }
  } else {  // SDカードが使用不能
    M5.Lcd.drawEllipse((POS_X + 8), (POS_Y + 12), 5, 5, COLOR_ICON_ERROR);
    M5.Lcd.drawLine((POS_X + 11), (POS_Y + 9), (POS_X + 5), (POS_Y + 15), COLOR_ICON_ERROR);
  }
}

/**
 * バッテリーアイコンの描画
 */
void drawBatteryIcon(const int16_t POS_X, const int16_t POS_Y) {
  const int16_t ICON_W = 21;
  const int16_t ICON_H = 20;
  const int16_t METER_W = 3;
  const int16_t METER_H = 12;
  
  // バッテリーの外枠を描画
  M5.Lcd.fillRoundRect(POS_X, POS_Y, (ICON_W - 2), ICON_H, 2, COLOR_ICON_BACK);  // 本体
  M5.Lcd.fillRect((POS_X + (ICON_W - 2)), (POS_Y + 6), 2, 8, COLOR_ICON_BACK);  // 凸部

  if (M5.Power.canControl()) {
    // 4段階でバッテリーレベルを取得 15%以下、40%以下、65%以下、100%以下
    int8_t battBias = 15;
    int8_t battLevel = (battBias + M5.Power.getBatteryLevel()) / 25;

    // バッテリーレベルの表示
    for (int i = 0; i < battLevel; i++) {
      M5.Lcd.fillRect((POS_X + 2) + ((METER_W+1) * i), (POS_Y + 4), METER_W, METER_H, COLOR_ICON_METER);
    }
    if (battLevel == 0) {
      M5.Lcd.fillRect((POS_X + 2), (POS_Y + 4), METER_W, METER_H, COLOR_ICON_METER);
    }
  } else {  // バッテリーレベル不明の表示
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(COLOR_ICON_ERROR);
    M5.Lcd.drawCentreString("?", POS_X + (ICON_W / 2), POS_Y + 3, 1);
  }
}

/**
 * タイトルバー表示
 */
void drawTitleBar() {
  const int16_t TITLE_BAR_W = 320;
  const int16_t TITLE_BAR_H = 24;
  const int16_t CAPTION_X = 2;
  const int16_t CAPTION_Y = 2;
  const int16_t BATTERY_ICON_X = (TITLE_BAR_W - 24);
  const int16_t BATTERY_ICON_Y = 2;
  const int16_t SDCARD_ICON_X = (BATTERY_ICON_X - 20);
  const int16_t SDCARD_ICON_Y = 2;
  
  // タイトルバーの背景を描画
  M5.Lcd.fillRect(0, 0, TITLE_BAR_W, TITLE_BAR_H, COLOR_BAR_BACK);

  {  // タイトル文字の表示
    M5.Lcd.setTextFont(4);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(COLOR_BAR_TEXT);
    M5.Lcd.drawString(APP_NAME, CAPTION_X, CAPTION_Y);
  }

  // 状態表示アイコンの表示
  drawSDcardIcon(SDCARD_ICON_X, SDCARD_ICON_Y); // SDカードステータス
  drawBatteryIcon(BATTERY_ICON_X, BATTERY_ICON_Y); // バッテリーステータス
}

void drawMainMenu() {
  const int16_t MENUBTN_W = 98; // ボタンのサイズ指定
  const int16_t MENUBTN_H = 83;
  const int16_t MENUBTN1_X = 5; // 配置の基準とする左上ボタンの位置
  const int16_t MENUBTN1_Y = 33;

  for (int i = 0; i < 6; i++) {
    int16_t POS_X = MENUBTN1_X + ((MENUBTN_W + 8) * (i % 3));
    int16_t POS_Y = MENUBTN1_Y + ((MENUBTN_H + 7) * (i / 3));
    int16_t ICON_X = POS_X + (MENUBTN_W / 2) - (60 / 2);
    int16_t ICON_Y = POS_Y + (MENUBTN_H / 2) - (60 / 2);

    // メインメニューのボタンの背景を描画
    if (i == mainMenu.selectedIndex) {  // 選択状態メニュー
      M5.Lcd.fillRoundRect(POS_X, POS_Y, MENUBTN_W, MENUBTN_H, 4,
                           COLOR_MENU_SEL_BODY);
      M5.Lcd.drawRoundRect(POS_X, POS_Y, MENUBTN_W, MENUBTN_H, 4,
                           COLOR_MENU_SEL_BORDER);
    } else {  // 非選択状態メニュー
      M5.Lcd.fillRoundRect(POS_X, POS_Y, MENUBTN_W, MENUBTN_H, 4,
                           COLOR_MENU_BODY);
    }

    // ボタンのアイコンを描画
    //M5.Lcd.drawBitmap(ICON_X, ICON_Y, 60, 60, iconData);

    { // ボタンのキャプションを描画
      M5.Lcd.setTextSize(1);
      M5.Lcd.setTextColor(COLOR_MENU_TEXT);
      M5.Lcd.drawCentreString(mainMenu.items[i].caption, POS_X+(MENUBTN_W/2),
                              POS_Y+(MENUBTN_H - 12), 1);
    }
  }
}

/**
 * 物理ボタンの操作ナビゲーションボタンを描画する。
 * ボタンのキャプションや有効状態はグローバル変数のnaviitem_t naviButtons[3]を参照
 */
void drawNaviBar() {
  int16_t NAVIBTN_W = 86;
  int16_t NAVIBTN_H = 24;
  int16_t NAVIBTN1_X = 65 - (NAVIBTN_W / 2);
  int16_t NAVIBTN1_Y = 240 - NAVIBTN_H;

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(COLOR_NAVI_TEXT);

  // 物理ボタンに対応する操作ナビゲーションボタンを3つ左から描画
  for (int i = 0; i < 3; i++) {
    // ボタンが無効なら描画しない
    if (naviButtons[i].disabled) continue;

    // ボタンの描画位置の決定
    int16_t POS_X = NAVIBTN1_X + ((NAVIBTN_W + 8) * i);
    int16_t POS_Y = NAVIBTN1_Y;

    // ボタンの枠
    M5.Lcd.fillRoundRect(POS_X, POS_Y, NAVIBTN_W, NAVIBTN_H, 2,
                         COLOR_NAVI_BODY);

    // キャプション
    M5.Lcd.drawCentreString(naviButtons[i].caption, (POS_X + (NAVIBTN_W / 2)),
                            (POS_Y + 2), 4);
  }
}

void onNaviPrevButtonClick() {
  mainMenu.selectedIndex = (mainMenu.selectedIndex + 5) % 6;
  drawMainMenu();
}

void onNaviNextButtonClick() {
  mainMenu.selectedIndex = (mainMenu.selectedIndex + 1) % 6;
  drawMainMenu();

}

void onNaviEnterButtonClock() {
  menuitem_t *mi = &mainMenu.items[mainMenu.selectedIndex];
  if (mi->onSelected != NULL) mi->onSelected();
}

void setup() {
  // M5Stackの初期化
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setBrightness(32);

  // デバッグ用シリアルポートの初期化
  Serial.begin(115200);

  // SDカードの開始
  SD.begin(GPIO_NUM_4, SPI, SD_ACCESS_SPEED);

  memset(&naviButtons, 0, sizeof(naviButtons));
  naviButtons[0].caption = "Prev";
  naviButtons[0].wasPressed = &(M5BtnA_wasPressed);
  naviButtons[0].onClick = &(onNaviPrevButtonClick);
  naviButtons[1].caption = "Next";
  naviButtons[1].wasPressed = &(M5BtnB_wasPressed);
  naviButtons[1].onClick = &(onNaviNextButtonClick);
  naviButtons[2].caption = "Select";
  naviButtons[2].wasPressed = &(M5BtnC_wasPressed);
  naviButtons[2].onClick = &(onNaviEnterButtonClock);

  memset(&mainMenu, 0, sizeof(mainMenu));
  mainMenu.items[0].caption = "Download Log";
  mainMenu.items[0].onSelected = &onDownloadLogMenuSelected;
  mainMenu.items[1].caption = "Fix RTC";
  mainMenu.items[1].onSelected = &onFixRTCMenuSelected;
  mainMenu.items[2].caption = "Set Preset Cfg";
  mainMenu.items[2].onSelected = &onSetPresetConfigMenuSelected;
  mainMenu.items[3].caption = "Show Location";
  mainMenu.items[3].onSelected = &onShowLocationMenuSelected;
  mainMenu.items[4].caption = "Erase Log";
  mainMenu.items[4].onSelected = &onEraseLogMenuSelected;
  mainMenu.items[5].caption = "App Settings";
  mainMenu.items[5].onSelected = &onAppSettingsMenuSelected;

  // スクリーンを全域クリア
  M5.Lcd.clear(COLOR_SCR_BACK);

  // タイトルバーを描画
  drawTitleBar();
  drawNaviBar();
  drawMainMenu();
}

void loop() {
  M5.update(); // M5Stackのステータス更新

  // 物理ボタンの処理 (ナビボタンを押したものとして扱う)
  for (int i=0; i<3; i++) {
    naviitem_t *nb = &naviButtons[i]; // 判定対象のボタンを取得
    
    // 物理ボタンが押されていればイベントハンドラを呼び出す
    if ((nb->disabled == false) && (nb->wasPressed())) {
      if (nb->onClick != NULL) nb->onClick(); 
      break;  // 同時押しがあっても両方処理しない
    }
  }
  
  delay(50);
}
