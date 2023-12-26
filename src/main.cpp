#include <BluetoothSerial.h>
#include <M5Stack.h>
#include <SD.h>

#define SD_ACCESS_SPEED 15000000
#define COLOR16(r, g, b) (uint16_t)((r>>3)<<11)|((g>>2)<<5)|(b>>3)

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
const int16_t COLOR_MENU_TEXT = BLACK;

typedef enum _appstatus {
  APP_IN_MENU  = 0x000000,
  APP_DO_MENU1 = 0x001000,
  APP_DO_MENU2 = 0x002000,
  APP_DO_MENU3 = 0x003000,
  APP_DO_MENU4 = 0x004000,
  APP_DO_MENU5 = 0x005000,
  APP_DO_MENU6 = 0x006000
} appstatus_t;


typedef struct _menuitem {
  const char *caption;
  int16_t *icon;
  bool disabled;
  void (*onSelected)();
} menuitem_t;

typedef struct _mainmenu {
  menuitem_t items[6];
  int8_t selectedIndex;
} mainmenu_t;

//appstatus_t status;
mainmenu_t menu;

/**
 * 
 */
void onDownloadLogMenuSelected() {

}

/**
 * 
 */
void onFixRTCMenuSelected() {

}

/**
 * 
 */
void onSetPresetConfigMenuSelected() {

}

/**
 * 
 */
void onShowLocationMenuSelected() {

}

/**
 * 
 */
void onEraseLogMenuSelected() {

}

/**
 * 
 */
void onAppSettingsMenuSelected() {

}

/**
 * SDカードの状態表示アイコンの描画
 */
void drawSDcardIcon(const int16_t POS_X, const int16_t POS_Y) {
  int16_t ICON_W = 17;
  int16_t ICON_H = 20;
  int16_t PIN_W = 2;
  int16_t PIN_H = 4;

  // SDカードの外枠を描画
  M5.Lcd.fillRoundRect((POS_X + 2), POS_Y, (ICON_W - 2), 8, 2, COLOR_OBJ_BODY);  // 本体
  M5.Lcd.fillRoundRect(POS_X, (POS_Y + 6), ICON_W, (ICON_H - 6), 2, COLOR_OBJ_BODY);  // 本体
  for (int i = 0; i < 4; i++) {                                  // 端子部
    M5.Lcd.fillRect((POS_X + 4) + ((PIN_W + 1) * i), (POS_Y + 1), PIN_W, PIN_H, COLOR_SD_PIN);
  }

  File root = SD.open("/");
  if (root) {  // SDカードが使用可能 (ルートディレクトリがオープンできる)
    // テスト用に開いたファイルを閉じる
    root.close();

    // 状態表示
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(COLOR_OBJ_TEXT);
    M5.Lcd.drawCentreString("SD", (POS_X + 9), (POS_Y + 9), 1);
  } else {  // SDカードが使用不能
    M5.Lcd.drawEllipse((POS_X + 8), (POS_Y + 12), 5, 5, COLOR_OBJ_ERROR);
    M5.Lcd.drawLine((POS_X + 11), (POS_Y + 9), (POS_X + 5), (POS_Y + 15), COLOR_OBJ_ERROR);
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
  M5.Lcd.fillRoundRect(POS_X, POS_Y, (ICON_W - 2), ICON_H, 2, COLOR_OBJ_BODY);  // 本体
  M5.Lcd.fillRect((POS_X + (ICON_W - 2)), (POS_Y + 6), 2, 8, COLOR_OBJ_BODY);  // 凸部

  if (M5.Power.canControl()) {
    // 4段階でバッテリーレベルを取得 15%以下、40%以下、65%以下、100%以下
    int8_t battBias = 15;
    int8_t battLevel = (battBias + M5.Power.getBatteryLevel()) / 25;

    // バッテリーレベルの表示
    for (int i = 0; i < battLevel; i++) {
      M5.Lcd.fillRect((POS_X + 2) + ((METER_W+1) * i), (POS_Y + 4), METER_W, METER_H, COLOR_OBJ_TEXT);
    }
    if (battLevel == 0) {
      M5.Lcd.fillRect((POS_X + 2), (POS_Y + 4), METER_W, METER_H, COLOR_OBJ_ERROR);
    }
  } else {  // バッテリーレベル不明の表示
    //M5.Lcd.setTextFont(1);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(COLOR_OBJ_ERROR);
    M5.Lcd.drawCentreString("?", POS_X + (ICON_W / 2), POS_Y + 3, 1);
  }
}

/**
 * タイトルバー表示
 */
void drawTitleBar() {
  int16_t TITLE_BAR_W = 320;
  int16_t TITLE_BAR_H = 24;
  int16_t TITLE_BAR_X = 0;
  int16_t TITLE_BAR_Y = 0;
//  status.menuIndex = 0;

  // タイトルバークリア
  M5.Lcd.fillRect(0, 0, TITLE_BAR_W, TITLE_BAR_H, COLOR_BAR_BACK);

  {  // タイトル文字の表示
    M5.Lcd.setTextFont(4);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(COLOR_BAR_TEXT);
    M5.Lcd.setCursor(2, 2);
    M5.Lcd.printf("SmallStepM5S");
  }

  drawSDcardIcon(276, 2);
  drawBatteryIcon(296, 2);
}

/**
 * ボタンの操作ナビ表示
 */
void drawNaviBar() {
  int16_t NAVI_X = 25;
  int16_t NAVI_Y = 216;
  int16_t NAVI_BTN_W = 80;
  int16_t NAVI_BTN_H = 24;
  int16_t NAVI_BDR_W = 15;

  const char* NAV_CAPTIONS[3] = {"Prev", "Next", "OK"};

  //M5.Lcd.setTextFont(4);
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

void drawMainMenu() {
  int16_t SCRN_X = 0;
  int16_t SCRN_Y = 30;
  int16_t SCRN_W = 320;
  int16_t SCRN_H = 180;
  int16_t BORDER_W = 4;
  int16_t BORDER_H = 5;
  int16_t BUTTON_W = SCRN_W / 3 - (BORDER_W * 2);
  int16_t BUTTON_H = SCRN_H / 2 - (BORDER_H * 2);

  //M5.Lcd.setTextFont(1);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(COLOR_MENU_TEXT);

  for (int i = 0; i < 6; i++) {
    int16_t BUTTON_X =
        SCRN_X + BORDER_W + (BUTTON_W + (BORDER_W * 2)) * (i % 3);
    int16_t BUTTON_Y =
        SCRN_Y + BORDER_H + ((BUTTON_H + (BORDER_H * 2)) * (i / 3));

    if (i == menu.selectedIndex) {  // 選択状態メニュー
      M5.Lcd.fillRoundRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, 4,
                           COLOR_MENU_SEL_BODY);
      M5.Lcd.drawRoundRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, 4,
                           COLOR_MENU_SEL_BORDER);
    } else {  // 非選択状態メニュー
      M5.Lcd.fillRoundRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, 4,
                           COLOR_MENU_BODY);
    }

    M5.Lcd.drawCentreString(menu.items[i].caption, BUTTON_X+(BUTTON_W/2),
                            BUTTON_Y+(BUTTON_H - 12), 1);
  }
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

  // スクリーンを全域クリア
  M5.Lcd.clear(COLOR_SCR_BACK);

  memset(&menu, 0, sizeof(menu));
  menu.items[0].caption = "Download Log";
  menu.items[0].onSelected = &onDownloadLogMenuSelected;
  menu.items[1].caption = "Fix RTC";
  menu.items[1].onSelected = &onFixRTCMenuSelected;
  menu.items[2].caption = "Set Preset Cfg";
  menu.items[2].onSelected = &onSetPresetConfigMenuSelected;
  menu.items[3].caption = "Show Location";
  menu.items[0].onSelected = &onShowLocationMenuSelected;
  menu.items[4].caption = "Erase Log";
  menu.items[4].onSelected = &onEraseLogMenuSelected;
  menu.items[5].caption = "App Settings";
  menu.items[5].onSelected = &onAppSettingsMenuSelected;

  // タイトルバーを描画
  drawTitleBar();
  drawNaviBar();
  drawMainMenu();
}

void loop() {
  M5.update(); // M5Stackのステータス更新

  if (M5.BtnA.wasPressed()) {
    menu.selectedIndex = (menu.selectedIndex + 5) % 6;
    drawMainMenu();
    
  } else if (M5.BtnB.wasPressed()) {
    menu.selectedIndex = (menu.selectedIndex + 1) % 6;
    drawMainMenu();

  } else if (M5.BtnC.wasPressed()) {
    menuitem_t *mi = &menu.items[menu.selectedIndex];
    if (mi->onSelected != NULL) mi->onSelected();
  }
  
  delay(50);
}
