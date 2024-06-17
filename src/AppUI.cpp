#include "AppUI.h"

AppUI::AppUI() {
  sprite.setColorDepth(8);

  appTitle = "MyApp";
  memset(appHint[0], 0, sizeof(appHint[0]));
  memset(appHint[1], 0, sizeof(appHint[1]));

  appIcon = NULL;
  btBgIcon = ICON_BT_BG;
  btFgIcon = ICON_BT_FG;

  buttons[0] = &M5.BtnA;
  buttons[1] = &M5.BtnB;
  buttons[2] = &M5.BtnC;

  int16_t screenWidth = 320;   // M5.Lcd.width returns 240??
  int16_t screenHeight = 240;  // M5.Lcd.width returns 320??

  titleBarWidth = screenWidth;
  titleBarHeight = 24;
  titleBarLeft = 0;
  titleBarTop = 0;
  navBarWidth = screenWidth;
  navBarHeight = 24;
  navBarLeft = 0;
  navBarTop = screenHeight - navBarHeight;
  menuAreaWidth = screenWidth;
  menuAreaHeight = screenHeight - (titleBarHeight + navBarHeight);
  menuAreaLeft = 0;
  menuAreaTop = titleBarHeight;
  dlgFrameWidth = menuAreaWidth - 8;
  dlgFrameHeight = menuAreaHeight - 8;
  dlgTitleHeight = 26;
  dlgClientHeight = dlgFrameHeight - dlgTitleHeight - 8;
  dlgClientHeight = dlgFrameWidth - 8;
  dlgFrameLeft = 4;
  dlgFrameTop = titleBarHeight + 4;
  dlgClientTop = dlgFrameTop + dlgTitleHeight + 4;
  dlgClientLeft = dlgFrameLeft + 4;
}

void AppUI::putBitmap(TFT_eSprite *spr, const uint8_t *iconData, int16_t x, int16_t y, int16_t color) {
  // icon data format:
  // - 1st byte: uint8_t width
  // - 2nd byte: uint8_t height
  // - 3rd-4th byte: uint16_t color
  // - 5th byte to the end: uint8_t pixelBits[]

  if (iconData == NULL) return;

  uint8_t iconW = *iconData++;             // icon width
  uint8_t iconH = *iconData++;             // icon height
  iconData += 2;                           // icon color (unused)
  uint16_t pixelsTotal = (iconW * iconH);  // total number of pixels

  // draw the icon pixel by pixel
  for (int32_t i = 0; i < pixelsTotal; i++) {
    // draw the pixel if the bit is set
    if (*iconData & (0b10000000 >> (i % 8))) {
      int px = x + (i % iconW);
      int py = y + (i / iconW);
      spr->drawPixel(px, py, color);
    }

    // move to the next byte if the current byte is processed
    if ((i % 8) == 7) iconData++;
  }
}

void AppUI::drawDialogProgress(int32_t progRate) {
  const int16_t BAR_X = 10;
  const int16_t BAR_Y = 180;
  const int16_t BAR_W = 300;
  const int16_t BAR_H = 18;

  // limit the progress value to 0-100
  if (progRate < 0) progRate = 0;
  if (progRate > 100) progRate = 100;

  M5.Lcd.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, NAVY);
  if (progRate == 0) {
    M5.Lcd.fillRect((BAR_X + 1), (BAR_Y + 1), (BAR_W - 2), (BAR_H - 2), LIGHTGREY);
  } else {
    M5.Lcd.fillRect(BAR_X, BAR_Y, (BAR_W * (progRate / 100.0)), BAR_H, NAVY);
  }
}

void AppUI::drawDialogMessage(int16_t color, int8_t line, String msg) {
  const int16_t MSG_LEFT = 10;
  const int16_t MSG_TOP = 60;
  const int16_t LINE_HEIGHT = 18;

  // set the text font and size (16px ASCII)
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(1);

  // print the message text
  M5.Lcd.setTextColor(color);
  M5.Lcd.setCursor(MSG_LEFT, (MSG_TOP + (LINE_HEIGHT * line)));
  M5.Lcd.print(msg);
}

void AppUI::drawDialogFrame(const char *title) {
  const int16_t MARGIN = 4;
  const int16_t DIALOG_X = MARGIN;
  const int16_t DIALOG_Y = 24 + MARGIN;
  const int16_t DIALOG_W = M5.Lcd.width() - (MARGIN * 2);
  const int16_t DIALOG_H = (M5.Lcd.height() - 48) - (MARGIN * 2);
  const int16_t TITLE_X = 2;
  const int16_t TITLE_Y = 2;

  // initialize the sprite
  sprite.createSprite(dlgFrameWidth, dlgFrameHeight);
  sprite.fillScreen(LIGHTGREY);

  // draw the dialog frame
  sprite.fillRect(0, 0, sprite.width(), dlgTitleHeight, COLOR16(0, 128, 255));
  sprite.drawRect(0, 0, sprite.width(), sprite.height(), DARKGREY);

  // print the title
  sprite.setTextSize(1);
  sprite.setTextColor(BLACK);
  sprite.drawString(title, TITLE_X, TITLE_Y, 4);

  // transfer the sprite to the LCD
  sprite.pushSprite(dlgFrameLeft, dlgFrameTop);
  sprite.deleteSprite();
}

/**
 * Draw the main menu to the client area of the screen.
 * @param menu menu data (array of menuitem_t)
 * @param top top item index
 * @param select selected item index
 */
void AppUI::drawMainMenu(menuitem_t *menu, int8_t itemCount, int8_t top, int8_t select) {
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

  // メニュー項目の表示位置を補正
  top = (top < 0) ? 0 : (top < itemCount) ? top : itemCount;
  select = (select < top) ? top : (select < itemCount) ? select : itemCount;

  // スプライト領域の初期化
  sprite.createSprite(menuAreaWidth, menuAreaHeight);
  sprite.fillScreen(BLACK);

  for (int8_t i = 0; i < 6; i++) {
    int8_t idx = top + i;
    if (idx >= itemCount) break;

    menuitem_t *mi = menu + idx;

    int16_t x = MENUBTN1_X + ((MENUBTN_W + MARGIN_X) * (i % 3));
    int16_t y = MENUBTN1_Y + ((MENUBTN_H + MARGIN_Y) * (i / 3));
    int16_t color = (idx == select) ? LIGHTGREY : DARKGREY;

    // ボタンの背景を描画
    sprite.fillRoundRect(x, y, MENUBTN_W, MENUBTN_H, 4, color);

    // ボタンのアイコンを描画
    if (mi->iconData != NULL) {
      putBitmap(&sprite, mi->iconData, (x + (MENUBTN_W / 2) - (48 / 2)), (y + 12), COLOR16(0, 32, 32));
    }

    // ボタンのキャプションを描画
    sprite.setTextSize(1);
    sprite.setTextColor(BLACK);
    sprite.drawCentreString(mi->caption, x + (MENUBTN_W / 2), y + (MENUBTN_H - 12), 1);
  }

  // transfer the sprite to the LCD and release the sprite
  sprite.pushSprite(menuAreaLeft, menuAreaTop);
  sprite.deleteSprite();
}

/**
 * Draw the navigation bar at the bottom of the screen.
 * @param navi Navigation menu data
 */
void AppUI::drawNavBar(navmenu_t *nav) {
  const int16_t NAVIBTN_W = 86;
  const int16_t NAVIBTN_H = navBarHeight;
  const int16_t NAVIBTN1_X = 65 - (NAVIBTN_W / 2);
  const int16_t MARGIN_X = 8;

  if (nav == NULL) {
    navmenu_t nm;
    memset(&nm, 0, sizeof(navmenu_t));
    nm.items[0].caption = "";
    nm.items[1].caption = "";
    nm.items[2].caption = "";
    drawNavBar(&nm);
    return;
  }

  // create a sprite area for the navigation bar
  sprite.createSprite(navBarWidth, navBarHeight);
  sprite.fillScreen(BLACK);

  sprite.setTextFont(2);
  sprite.setTextSize(1);

  // 物理ボタンに対応する操作ナビゲーションボタンを3つ左から描画
  for (int16_t i = 0; i < 3; i++) {
    // 対象のボタンアイテム情報を取得
    navitem_t *ni = &(nav->items[i]);

    // ボタンの描画位置を計算
    int16_t x = NAVIBTN1_X + ((NAVIBTN_W + MARGIN_X) * i);
    int16_t y = 0;
    int16_t color = (ni->enabled) ? LIGHTGREY : DARKGREY;

    // draw the button frame
    sprite.fillRoundRect(x, y, NAVIBTN_W, NAVIBTN_H, 2, color);

    // draw the button caption
    sprite.setTextColor(BLACK);
    sprite.drawCentreString(ni->caption, (x + (NAVIBTN_W / 2)), (y + 2), 4);
  }

  // transfer the sprite to the LCD and release the sprite
  sprite.pushSprite(navBarLeft, navBarTop);
  sprite.deleteSprite();
}

void AppUI::drawTitleBar(bool sdAvail, bool btActive) {
  const int16_t TITLE_X = (appIcon == NULL) ? 2 : 32;
  const int16_t TITLE_Y = 2;
  const int16_t APP_ICON_X = 2;
  const int16_t BAT_ICON_X = (titleBarWidth - 24);
  const int16_t SD_ICON_X = (BAT_ICON_X - 20);
  const int16_t BT_ICON_X = (SD_ICON_X - 19);
  const int16_t ICON_Y = 2;

  // スプライト領域の初期化
  sprite.createSprite(titleBarWidth, titleBarHeight);
  sprite.fillScreen(LIGHTGREY);

  // draw the title text
  sprite.setTextSize(1);
  sprite.setTextColor(BLACK);
  sprite.drawString(appTitle, TITLE_X, TITLE_Y, 4);

  // draw the icons
  if (appIcon != NULL) putAppIcon(&sprite, APP_ICON_X, ICON_Y);
  putBatteryIcon(&sprite, BAT_ICON_X, ICON_Y, M5.Power.getBatteryLevel());
  putSDcardIcon(&sprite, SD_ICON_X, ICON_Y, sdAvail);
  putBluetoothIcon(&sprite, BT_ICON_X, ICON_Y, btActive);

  // draw the hint text
  sprite.setTextSize(1);
  sprite.setTextColor(BLACK);
  sprite.drawString(appHint[0], 168, 4, 1);
  sprite.drawString(appHint[1], 168, 14, 1);

  // transfer the sprite to the LCD
  sprite.pushSprite(titleBarLeft, titleBarTop);
  sprite.deleteSprite();
}

void AppUI::putAppIcon(TFT_eSprite *spr, int16_t x, int16_t y) {
  // アイコンのデータの描画色を決定
  uint16_t color = COLOR16(0, 64, 255);

  // アイコンを描画 (2つセット)
  putBitmap(spr, appIcon, x, y, color);
}

void AppUI::putBluetoothIcon(TFT_eSprite *spr, int16_t x, int16_t y, bool connected) {
  // 描画色の決定。Bluetoothが未接続の場合はアイコンを灰色背景にする
  int16_t colorBg = (connected) ? BLUE : DARKGREY;

  // アイコンの背景・前景を指定位置に描画
  putBitmap(spr, btBgIcon, x, y, colorBg);
  putBitmap(spr, btFgIcon, x, y, WHITE);
}

/**
 * SDカードの状態表示アイコンの描画
 */
void AppUI::putSDcardIcon(TFT_eSprite *spr, int16_t x, int16_t y, bool mounted) {
  const int16_t ICON_W = 17;
  const int16_t ICON_H = 20;
  const int16_t PIN_W = 2;
  const int16_t PIN_H = 4;

  // draw the outer frame of the SD card symbol
  spr->fillRoundRect((x + 2), y, (ICON_W - 2), 8, 2, BLACK);
  spr->fillRoundRect(x, (y + 6), ICON_W, (ICON_H - 6), 2, BLACK);
  for (int16_t i = 0; i < 4; i++) {
    spr->fillRect((x + 4) + ((PIN_W + 1) * i), (y + 1), PIN_W, PIN_H, YELLOW);
  }

  // draw "SD" or "**" mark
  if (mounted) {  // mounted
    // put "SD" text
    spr->setTextSize(1);
    spr->setTextColor(LIGHTGREY);
    spr->drawCentreString("SD", (x + 9), (y + 9), 1);
  } else {  // not mounted
    spr->drawEllipse((x + 8), (y + 12), 5, 5, RED);
    spr->drawLine((x + 11), (y + 9), (x + 5), (y + 15), RED);
  }
}

/**
 * バッテリーアイコンの描画
 */
void AppUI::putBatteryIcon(TFT_eSprite *spr, int16_t x, int16_t y, int8_t level) {
  const int16_t ICON_W = 21;
  const int16_t ICON_H = 20;
  const int16_t BAR_W = 3;
  const int16_t BAR_H = 12;

  // draw the outer frame of the battery symbol
  spr->fillRoundRect(x, y, (ICON_W - 2), ICON_H, 2, BLACK);
  spr->fillRect((x + (ICON_W - 2)), (y + 6), 2, 8, BLACK);

  // draw "?" mark if the battery level is unknown
  if (level < 0) {
    spr->setTextSize(2);
    spr->setTextColor(RED);
    spr->drawCentreString("?", x + (ICON_W / 2), y + 3, 1);
    return;
  }

  // calculate the battery level (0-100) to the bar count (0-4)
  if (level > 100) level = 100;
  int8_t barCount = ((level + 15) / 25);

  // draw the battery level meter
  for (int16_t i = 0; i < barCount; i++) {
    spr->fillRect((x + 2) + ((BAR_W + 1) * i), (y + 4), BAR_W, BAR_H, LIGHTGREY);
  }
  if (barCount == 0) {
    spr->fillRect((x + 2), (y + 4), BAR_W, BAR_H, RED);
  }
}

btnid_t AppUI::checkButtonInput(navmenu_t *nav) {
  // update button status
  M5.update();
  for (int i = 0; i < 3; i++) {
    if ((nav->items[i].enabled) && (buttons[i]->wasPressed())) {
      return (btnid_t)i;
    }
  }

  return BID_NONE;
}

void AppUI::setAppTitle(const char *title) {
  appTitle = title;
}

void AppUI::setAppIcon(const uint8_t *icon) {
  appIcon = icon;
}

void AppUI::setAppHints(const char *hint1, const char *hint2) {
  memset(appHint[0], 0, sizeof(appHint[0]));
  memset(appHint[1], 0, sizeof(appHint[1]));

  if (hint1 != NULL) strncpy(appHint[0], hint1, 16);
  if (hint2 != NULL) strncpy(appHint[1], hint2, 16);
}

void AppUI::setIdleCallback(void (*callback)(), uint32_t timeout) {
  idleCallback = callback;
  idleTimeout = timeout;
}

btnid_t AppUI::waitForButtonInput(navmenu_t *nav) {
  drawNavBar(nav);

  uint32_t idleStart = millis();
  uint32_t idleTime = 0;

  btnid_t btn = BID_NONE;
  while (btn == BID_NONE) {
    idleTime = millis() - idleStart;
    if ((idleTimeout > 0) && (idleTime > idleTimeout)) {
      if (idleCallback != NULL) idleCallback();
      idleStart = millis();
    }

    btn = checkButtonInput(nav);
    delay(LOOP_WAIT);
  }

  return btn;
}

btnid_t AppUI::waitForInputOk() {
  navmenu_t nav;
  nav.items[0] = {"", false};
  nav.items[1] = {"", false};
  nav.items[2] = {"OK", true};

  waitForButtonInput(&nav);

  return BID_OK;
}

btnid_t AppUI::waitForInputOkCancel() {
  navmenu_t nav;
  nav.items[0] = {"", false};
  nav.items[1] = {"Cancel", true};
  nav.items[2] = {"OK", true};

  btnid_t btn = (waitForButtonInput(&nav) == BID_BTN_C) ? BID_OK : BID_CANCEL;

  return btn;
}

void AppUI::drawConfigMenu(const char *title, cfgitem_t *menu, int8_t itemCount, int8_t top, int8_t select) {
  const int16_t MENUAREA_W = M5.Lcd.width();
  const int16_t BTN_MGN_X = 8;
  const int16_t BTN_MGN_T = 8;
  const int16_t BTN_MGN_Y = 4;
  const int16_t BTN_W = MENUAREA_W - (BTN_MGN_X * 2);  // ボタンのサイズ指定
  const int16_t BTN_H = 35;

  top = (top < 0) ? 0 : (top < itemCount) ? top : itemCount;
  select = (select < top) ? top : (select < itemCount) ? select : itemCount;

  // スプライト領域の初期化
  sprite.createSprite(menuAreaWidth, menuAreaHeight);
  sprite.fillScreen(BLACK);

  sprite.setTextSize(1);
  sprite.setTextColor(BLACK);
  sprite.setTextSize(1);

  sprite.fillRect(BTN_MGN_X, BTN_MGN_T, BTN_W, 18, COLOR16(0, 128, 255));
  sprite.setTextColor(BLACK);
  sprite.drawString(title, (BTN_MGN_X + 4), (BTN_MGN_T + 1), 2);

  for (int i = 0; i < 4; i++) {
    int8_t idx = top + i;
    if (idx >= itemCount) break;

    cfgitem_t *mi = menu + idx;

    int16_t x = BTN_MGN_X;
    int16_t y = (BTN_MGN_T + 18) + (BTN_MGN_Y * (i + 1)) + (BTN_H * i);
    int16_t color = (idx == select) ? LIGHTGREY : DARKGREY;

    sprite.fillRect(x, y, BTN_W, BTN_H, color);

    sprite.setTextColor(BLACK);
    sprite.drawString(mi->caption, (x + 4), (y + 3), 2);
    sprite.drawString(mi->description, (x + 4), (y + 23), 1);
    if (mi->valueDescr != NULL) {
      sprite.setTextColor(BLUE);
      sprite.drawRightString(mi->valueDescr, (x + (BTN_W - 4)), (y + 3), 2);
    }
  }

  // transfer the sprite to the LCD
  sprite.pushSprite(menuAreaLeft, menuAreaTop);
  sprite.deleteSprite();
}

void AppUI::openConfigMenu(const char *title, cfgitem_t *menu, int8_t itemCount) {
  navmenu_t nav;
  nav.items[0] = {"Prev", true};
  nav.items[1] = {"Next", true};
  nav.items[2] = {"Select", true};
  drawNavBar(&nav);

  int8_t top = 0;
  int8_t select = 0;

  for (int8_t i = 0; i < itemCount; i++) {
    cfgitem_t *ci = &menu[i];
    if (ci->updateValueDescr != NULL) ci->updateValueDescr(&menu[i]);
  }

  bool endFlag = false;
  bool needSave = false;
  while (!endFlag) {
    drawConfigMenu(title, menu, itemCount, top, select);

    // check the button input
    switch (waitForButtonInput(&nav)) {
    case BID_BTN_A:  // move the selection to the previous
      select = (select + (itemCount - 1)) % itemCount;

      if ((itemCount < 4) || (select <= 1)) {
        top = 0;
      } else if (select >= itemCount - 2) {
        top = itemCount - 4;
      } else {
        top = select - 1;
      }
      break;

    case BID_BTN_B:  // move the selection to the next
      select = (select + 1) % itemCount;

      if ((itemCount < 4) || (select <= 1)) {
        top = 0;
      } else if (select >= itemCount - 2) {
        top = itemCount - 4;
      } else {
        top = select - 2;
      }
      break;

    case BID_BTN_C:  // call the onSelect function of the selected item
      cfgitem_t *ci = &menu[select];
      if (ci->onSelect != NULL) {
        ci->onSelect(ci);
        if (ci->updateValueDescr != NULL) ci->updateValueDescr(ci);
      }

      endFlag = (ci->onSelect == NULL);
      break;
    }
  }
}

void AppUI::openMainMenu(menuitem_t *menu, int8_t itemCount) {
  navmenu_t nav;
  nav.items[0] = {"Prev", true};
  nav.items[1] = {"Next", true};
  nav.items[2] = {"Select", true};
  drawNavBar(&nav);

  int8_t top = 0;
  int8_t select = 0;

  while (true) {
    drawMainMenu(menu, itemCount, top, select);

    switch (waitForButtonInput(&nav)) {
    case BID_BTN_A:  // move the selection to the previous
      select = (select + (itemCount - 1)) % itemCount;
      top = (itemCount < 4) ? 0 : (((select - 3) / 3) * 3);
      break;

    case BID_BTN_B:  // move the selection to the next
      select = (select + 1) % itemCount;
      top = (itemCount < 6) ? 0 : (((select - 3) / 3) * 3);
      break;

    case BID_BTN_C:  // call the onSelect function of the selected item
      menuitem_t *mi = &menu[select];
      if (mi->onSelect != NULL) mi->onSelect(mi);
      break;
    }
  }
}
