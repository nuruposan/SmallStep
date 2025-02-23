#include "AppUI.h"

AppUI::AppUI() {
  sprite.setColorDepth(8);        // 8 bit/pixel
  sprite.createSprite(320, 240);  // 8 bit/pixel x (320 x 240) pixels = 76KB
  sprite.fillScreen(BLACK);

  appIcon = NULL;
  appTitle = "MyApp";
  strncpy(appHint[0], "", APP_HINT_LEN);
  strncpy(appHint[1], "", APP_HINT_LEN);
  btIcon = {true, false};
  sdIcon = {true, false};

  buttons[0] = &M5.BtnA;
  buttons[1] = &M5.BtnB;
  buttons[2] = &M5.BtnC;
}

AppUI::~AppUI() {
  sprite.deleteSprite();
}

void AppUI::drawBitmap(const uint8_t *iconData, int16_t x, int16_t y) {
  if (iconData == NULL) return;

  uint16_t iconColor = getBitmapColor(iconData);
  drawBitmap(iconData, x, y, iconColor);
}

void AppUI::drawBitmap(const uint8_t *iconData, int16_t x, int16_t y, int16_t color) {
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
      sprite.drawPixel(px, py, color);
    }

    // move to the next byte if the current byte is processed
    if ((i % 8) == 7) iconData++;
  }
}

void AppUI::drawDialogProgress(int32_t current, int32_t max) {
  // calculate the progress rate
  int32_t progRate = (max == 0) ? 0 : ((float)current / max) * 100;

  // limit the progress value to 0-100
  if (progRate < 0) progRate = 0;
  if (progRate > 100) progRate = 100;

  uint8_t barHeight = 16;
  uisize_t border = {8, 8};
  uiarea_t barArea = {(int16_t)(DIALOG_AREA.w - (border.w * 2)),  //
                      barHeight,                                  //
                      (int16_t)(DIALOG_AREA.x + border.w),        //
                      (int16_t)(DIALOG_AREA.y + CLIENT_AREA.h - (barHeight * 2.2))};
  uint16_t colorBg = COLOR16(160, 160, 160);
  uint16_t colorFg = NAVY;
  uint16_t fgWidth = (barArea.w * (progRate / 100.0));

  sprite.createSprite(barArea.w, barArea.h);

  // draw a progress bar
  sprite.fillSprite(colorBg);
  sprite.fillRect(0, 0, fgWidth, barArea.h, colorFg);
  sprite.drawRect(0, 0, barArea.w, barArea.h, colorFg);

  // draw text (format: "current/max (rate%)")
  char buf[32];
  sprintf(buf, "%d/%d(%d%%)", current, max, progRate);
  sprite.setTextSize(1);
  if (progRate < 50) {
    sprite.setTextColor(colorFg);
    sprite.drawString(buf, (fgWidth + 2), 5, 1);
  } else {
    sprite.setTextColor(colorBg);
    sprite.drawRightString(buf, (fgWidth - 2), 5, 1);
  }

  sprite.pushSprite(barArea.x, barArea.y);
  sprite.deleteSprite();
}

void AppUI::drawDialogText(int16_t color, int8_t row, String line) {
  uisize_t border = {8, 34};
  uint8_t lineHeight = 18;
  uipos_t msgPos0 = {(int16_t)(DIALOG_AREA.x + border.w), (int16_t)(DIALOG_AREA.y + border.h)};
  uiarea_t msgArea = {(int16_t)(DIALOG_AREA.w - (border.w * 2)), (int16_t)(lineHeight),  //
                      msgPos0.x, (int16_t)(msgPos0.y + (row * lineHeight))};

  sprite.createSprite(msgArea.w, msgArea.h);
  sprite.fillSprite(LIGHTGREY);

  // print the message text
  sprite.setTextFont(2);
  sprite.setTextSize(1);
  sprite.setTextColor(color);
  sprite.setCursor(0, 0);
  sprite.print(line);

  sprite.pushSprite(msgArea.x, msgArea.y);
  sprite.deleteSprite();
}

void AppUI::drawDialogFrame(const char *title) {
  uisize_t titleBarSize = {DIALOG_AREA.w, 28};
  uipos_t titlePos = {4, 3};

  // initialize the sprite
  sprite.createSprite(DIALOG_AREA.w, DIALOG_AREA.h);
  sprite.fillSprite(LIGHTGREY);

  // draw a dialog frame with a title bar
  sprite.fillRect(0, 0, DIALOG_AREA.w, titleBarSize.h, COLOR16(0, 128, 255));
  sprite.drawRect(0, 0, DIALOG_AREA.w, DIALOG_AREA.h, DARKGREY);

  // draw the title on the title bar
  sprite.setTextSize(1);
  sprite.setTextColor(BLACK);
  sprite.drawString(title, titlePos.x, titlePos.y, 4);

  sprite.pushSprite(DIALOG_AREA.x, DIALOG_AREA.y);
  sprite.deleteSprite();
}

/**
 * Draw the main menu to the client area of the screen.
 * @param menu menu data (array of menuitem_t)
 * @param itemCount number of menu items
 * @param top top item index
 * @param select selected item index
 */
void AppUI::drawMainMenu(mainmenuitem_t *menu, int8_t itemCount, int8_t top, int8_t select) {
  uisize_t border = {8, 8};
  uisize_t margin = {6, 8};
  uipos_t btnPos0 = {(int16_t)(CLIENT_AREA.x + border.w), border.h};
  uisize_t btnSize = {(int16_t)((CLIENT_AREA.w - (border.w * 2) - (margin.w * 2)) / 3),
                      (int16_t)((CLIENT_AREA.h - (border.h * 2) - (margin.h)) / 2)};
  int16_t textOffset = 12;

  // メニュー項目の表示位置を補正
  top = (top < 0) ? 0 : (top < itemCount) ? top : itemCount;
  select = (select < top) ? top : (select < itemCount) ? select : itemCount;

  // スプライト領域の初期化
  sprite.createSprite(CLIENT_AREA.w, CLIENT_AREA.h);
  sprite.fillSprite(BLACK);

  for (int8_t i = 0; i < 6; i++) {
    int8_t idx = top + i;
    if (idx >= itemCount) break;

    mainmenuitem_t *mi = menu + idx;
    uipos_t btnPos = {(int16_t)(btnPos0.x + ((btnSize.w + margin.w) * (i % 3))),
                      (int16_t)(btnPos0.y + ((btnSize.h + margin.h) * (i / 3)))};
    uipos_t btnTextPos = {(int16_t)(btnPos.x + (btnSize.w / 2)), (int16_t)(btnPos.y + (btnSize.h - textOffset))};

    // ボタンの背景を描画
    uint16_t colorBg = (idx == select) ? LIGHTGREY : DARKGREY;
    sprite.fillRoundRect(btnPos.x, btnPos.y, btnSize.w, btnSize.h, 4, colorBg);

    // ボタンのアイコンを描画
    if (mi->iconData != NULL) {
      uisize_t iconSize = getBitmapSize(mi->iconData);
      uint16_t iconColor = COLOR16(0, 32, 32);
      drawBitmap(mi->iconData, (btnPos.x + ((btnSize.w - iconSize.w) / 2)),
                 (btnPos.y + ((btnSize.h - iconSize.h - textOffset) / 2)), iconColor);
    }

    // ボタンのキャプションを描画
    sprite.setTextSize(1);
    sprite.setTextColor(BLACK);
    sprite.drawCentreString(mi->caption, btnTextPos.x, btnTextPos.y, 1);
  }

  sprite.pushSprite(CLIENT_AREA.x, CLIENT_AREA.y);
  sprite.deleteSprite();
}

/**
 * Draw the navigation bar at the bottom of the screen.
 * @param navi Navigation menu data
 */
void AppUI::drawNavBar(navmenu_t *nav) {
  int16_t marginX = 8;
  uisize_t navSize = {84, NAV_AREA.h};
  uipos_t navPos0 = {(int16_t)((NAV_AREA.w / 2) - ((navSize.w * 1.5) + marginX)), 0};

  if (nav == NULL) {
    navmenu_t nm;
    nm.items[0] = {"", false};
    nm.items[1] = {"", false};
    nm.items[2] = {"", false};

    drawNavBar(&nm);
    return;
  }

  // create a sprite area for the navigation bar
  sprite.createSprite(NAV_AREA.w, NAV_AREA.h);
  sprite.fillSprite(BLACK);

  // 物理ボタンに対応する操作ナビゲーションボタンを3つ左から描画
  for (int16_t i = 0; i < 3; i++) {
    // 対象のボタンアイテム情報を取得
    navitem_t *ni = &(nav->items[i]);

    // ボタンの描画位置を計算
    uipos_t navPos = {(int16_t)(navPos0.x + ((marginX + navSize.w) * i)), navPos0.y};
    uipos_t textPos = {(int16_t)(navPos.x + (navSize.w / 2)), (int16_t)(navPos.y + 2)};
    int16_t colorBg = (ni->enabled) ? LIGHTGREY : DARKGREY;
    int16_t colorFg = BLACK;

    // draw a nav button
    sprite.fillRoundRect(navPos.x, navPos.y, navSize.w, navSize.h, 2, colorBg);

    // draw the button caption
    sprite.setTextSize(1);
    sprite.setTextColor(colorFg);
    sprite.drawCentreString(ni->caption, textPos.x, textPos.y, 4);
  }

  sprite.pushSprite(NAV_AREA.x, NAV_AREA.y);
  sprite.deleteSprite();
}

uint16_t AppUI::getBitmapColor(const uint8_t *iconData) {
  if (iconData == NULL) return 0;

  iconData += 2;  // skip the icon size
  uint16_t iconColor = (iconData[0] << 8) | iconData[1];

  return iconColor;
}

uisize_t AppUI::getBitmapSize(const uint8_t *iconData) {
  uisize_t iconSize = {0, 0};

  if (iconData == NULL) return iconSize;

  iconSize.w = *iconData++;
  iconSize.h = *iconData++;

  return iconSize;
}

void AppUI::drawTitleBar() {
  int8_t iconTop = TITLE_AREA.y + 2;
  uipos_t titlePos = {(int16_t)((appIcon == NULL) ? 2 : 32), iconTop};
  uipos_t appIconPos = {2, iconTop};
  uipos_t batIconPos = {(int16_t)(TITLE_AREA.w - 24), iconTop};
  uipos_t sdIconPos = {(int16_t)((sdIcon.visible) ? (batIconPos.x - 20) : batIconPos.x), iconTop};
  uipos_t btIconPos = {(int16_t)((btIcon.visible) ? (sdIconPos.x - 18) : sdIconPos.x), iconTop};
  uipos_t hintPos = {(int16_t)(btIconPos.x - 3), 4};

  // draw the title bar background
  sprite.createSprite(TITLE_AREA.w, TITLE_AREA.h);
  sprite.fillSprite(LIGHTGREY);

  // draw the application icon and title at the left side
  if (appIcon != NULL) drawAppIcon(appIconPos.x, appIconPos.y);
  sprite.setTextSize(1);
  sprite.setTextColor(BLACK);
  sprite.drawString(appTitle, titlePos.x, titlePos.y, 4);

  // draw the status icons and hints at the right side
  drawBatteryIcon(batIconPos.x, batIconPos.y);
  if (sdIcon.visible) drawSDcardIcon(sdIconPos.x, sdIconPos.y);
  if (btIcon.visible) drawBluetoothIcon(btIconPos.x, btIconPos.y);
  sprite.setTextSize(1);
  sprite.setTextColor(BLACK);
  sprite.drawRightString(appHint[0], hintPos.x, hintPos.y, 1);
  sprite.drawRightString(appHint[1], hintPos.x, (hintPos.y + 10), 1);

  // display the title bar on the top of the screen
  sprite.pushSprite(TITLE_AREA.x, TITLE_AREA.y);
  sprite.deleteSprite();
}

void AppUI::drawAppIcon(int16_t x, int16_t y) {
  // draw the app icon
  drawBitmap(appIcon, x, y, appIconColor);
}

void AppUI::drawBluetoothIcon(int16_t x, int16_t y) {
  // define the drawing color for the Bluetooth icon
  uint16_t colorBg = (btIcon.active) ? BLUE : DARKGREY;
  uint16_t colorFg = WHITE;

  uipos_t pt[][2] = {
      {{(int16_t)(x + 6), (int16_t)(y + 1)}, {(int16_t)(x + 6), (int16_t)(y + 18)}},  // vertical bar
      {{(int16_t)(x + 7), (int16_t)(y + 1)}, {(int16_t)(x + 7), (int16_t)(y + 18)}},
      {{(int16_t)(x + 2), (int16_t)(y + 5)}, {(int16_t)(x + 7), (int16_t)(y + 10)}},  // left side
      {{(int16_t)(x + 2), (int16_t)(y + 6)}, {(int16_t)(x + 7), (int16_t)(y + 11)}},
      {{(int16_t)(x + 2), (int16_t)(y + 13)}, {(int16_t)(x + 7), (int16_t)(y + 8)}},
      {{(int16_t)(x + 2), (int16_t)(y + 14)}, {(int16_t)(x + 7), (int16_t)(y + 9)}},
      {{(int16_t)(x + 6), (int16_t)(y + 1)}, {(int16_t)(x + 11), (int16_t)(y + 6)}},  // upper right side
      {{(int16_t)(x + 7), (int16_t)(y + 1)}, {(int16_t)(x + 12), (int16_t)(y + 6)}},
      {{(int16_t)(x + 11), (int16_t)(y + 6)}, {(int16_t)(x + 6), (int16_t)(y + 11)}},
      {{(int16_t)(x + 12), (int16_t)(y + 6)}, {(int16_t)(x + 6), (int16_t)(y + 12)}},
      {{(int16_t)(x + 6), (int16_t)(y + 8)}, {(int16_t)(x + 11), (int16_t)(y + 13)}},  // lower right side
      {{(int16_t)(x + 6), (int16_t)(y + 7)}, {(int16_t)(x + 12), (int16_t)(y + 13)}},
      {{(int16_t)(x + 11), (int16_t)(y + 13)}, {(int16_t)(x + 6), (int16_t)(y + 18)}},
      {{(int16_t)(x + 12), (int16_t)(y + 13)}, {(int16_t)(x + 7), (int16_t)(y + 18)}},
  };

  // draw the Bluetooth icon
  sprite.fillRoundRect(x, y, 15, 20, 5, colorBg);  // background circle
  for (int8_t i = 0; i < (sizeof(pt) / sizeof(pt[0])); i++) {
    sprite.drawLine(pt[i][0].x, pt[i][0].y, pt[i][1].x, pt[i][1].y, colorFg);
  }
}

/**
 * SDカードの状態表示アイコンの描画
 */
void AppUI::drawSDcardIcon(int16_t x, int16_t y) {
  uisize_t iconSize = {17, 20};
  uisize_t pinSize = {2, 4};

  // draw the outer frame of the SD card symbol
  sprite.fillRoundRect((x + 2), y, (iconSize.w - 2), (iconSize.h - 12), 2, BLACK);
  sprite.fillRoundRect(x, (y + 6), iconSize.w, (iconSize.h - 6), 2, BLACK);
  for (int16_t i = 0; i < 4; i++) {
    sprite.fillRect((x + 4) + ((pinSize.w + 1) * i), (y + 1), pinSize.w, pinSize.h, YELLOW);
  }

  // draw "SD" or "**" mark
  if (sdIcon.active) {  // mounted
    // put "SD" text
    sprite.setTextSize(1);
    sprite.setTextColor(LIGHTGREY);
    sprite.drawCentreString("SD", (x + (iconSize.w / 2) + 1), (y + (iconSize.h / 2) - 1), 1);
  } else {  // not mounted
    sprite.drawEllipse((x + 8), (y + 12), 5, 5, RED);
    sprite.drawLine((x + 5), (y + 9), (x + 11), (y + 15), RED);
  }
}

/**
 * バッテリーアイコンの描画
 */
void AppUI::drawBatteryIcon(int16_t x, int16_t y) {
  const int16_t ICON_W = 21;
  const int16_t ICON_H = 20;
  uisize_t iconSize = {21, 20};
  uisize_t barSize = {3, 12};
  int8_t lowBatLevel = 20;

  // draw the outer frame of the battery symbol
  sprite.fillRoundRect(x, y, (iconSize.w - 2), iconSize.h, 2, BLACK);
  sprite.fillRect((x + (iconSize.w - 2)), (y + 6), 2, 8, BLACK);

  // calculate the battery level (0-100) to the bar count (0-4)
  int8_t batLevel = M5.Power.getBatteryLevel();
  int8_t barCount = (batLevel + lowBatLevel) / (100 / 4);
  uipos_t barPos0 = {(int16_t)(x + 2), (int16_t)(y + 4)};

  // draw the battery level meter
  for (int16_t i = 0; i < barCount; i++) {
    sprite.fillRect(barPos0.x + ((barSize.w + 1) * i), barPos0.y, barSize.w, barSize.h, LIGHTGREY);
  }
  if (barCount == 0) {
    sprite.fillRect(barPos0.x, barPos0.y, barSize.w, barSize.h, RED);
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

void AppUI::setAppIcon(const uint8_t *icon, uint16_t color) {
  appIcon = icon;
  appIconColor = color;
}

void AppUI::setAppHints(const char *hint1, const char *hint2) {
  strncpy(appHint[0], "", APP_HINT_LEN);
  strncpy(appHint[1], "", APP_HINT_LEN);

  if (hint1 != NULL) strncpy(appHint[0], hint1, APP_HINT_LEN);
  if (hint2 != NULL) strncpy(appHint[1], hint2, APP_HINT_LEN);
}

void AppUI::setIdleCallback(void (*callback)(), uint32_t timeout) {
  idleCallback = callback;
  idleTimeout = timeout;
}

void AppUI::setIconVisible(bool btVisible, bool sdVisible) {
  if ((btIcon.visible == btVisible) && (sdIcon.visible == sdVisible)) return;

  btIcon.visible = btVisible;
  sdIcon.visible = sdVisible;
  drawTitleBar();
}

void AppUI::setBluetoothStatus(bool active) {
  if (btIcon.active == active) return;

  btIcon.active = active;
  drawTitleBar();
}

void AppUI::setSDcardStatus(bool mounted) {
  if (sdIcon.active == mounted) return;

  sdIcon.active = mounted;
  drawTitleBar();
}

btnid_t AppUI::promptCustom(navmenu_t *nav) {
  drawNavBar(nav);

  uint32_t idleStart = millis();
  uint32_t idleTime = 0;

  btnid_t btn = BID_NONE;
  while (btn == BID_NONE) {
    idleTime = millis() - idleStart;
    if ((idleTimeout > 0) && (idleTime > idleTimeout)) {
      if (idleCallback != NULL) idleCallback();
    }

    btn = checkButtonInput(nav);
    delay(LOOP_WAIT);
  }

  return btn;
}

btnid_t AppUI::promptOk() {
  navmenu_t nav;
  nav.items[0] = {"", false};
  nav.items[1] = {"", false};
  nav.items[2] = {"OK", true};

  promptCustom(&nav);

  return BID_OK;
}

btnid_t AppUI::promptOkCancel() {
  navmenu_t nav;
  nav.items[0] = {"", false};
  nav.items[1] = {"Cancel", true};
  nav.items[2] = {"OK", true};

  btnid_t btn = (promptCustom(&nav) == BID_BTN_C) ? BID_OK : BID_CANCEL;

  return btn;
}

void AppUI::drawConfigMenu(const char *title, textmenuitem_t *menu, int8_t itemCount, int8_t top, int8_t select) {
  uisize_t border = {6, 6};
  uisize_t margin = {0, 4};
  uiarea_t titleArea = {(int16_t)(CLIENT_AREA.w - (border.w * 2)), 18,  //
                        (int16_t)(CLIENT_AREA.x + border.w), (int16_t)(border.h)};
  uipos_t titlePos = {(int16_t)(titleArea.x + 4), (int16_t)(titleArea.y + 1)};
  uisize_t menuSize = {(int16_t)(CLIENT_AREA.w - (border.w * 2)),
                       (int16_t)(((CLIENT_AREA.h - (border.h * 2) - titleArea.h - (margin.h * 4)) / 4))};
  uipos_t menuPos0 = {(int16_t)(CLIENT_AREA.x + border.w), (int16_t)(18 + border.h + margin.h)};

  top = (top < 0) ? 0 : (top < itemCount) ? top : itemCount;
  select = (select < top) ? top : (select < itemCount) ? select : itemCount;

  // スプライト領域の初期化
  sprite.createSprite(CLIENT_AREA.w, CLIENT_AREA.h);
  sprite.fillSprite(BLACK);

  sprite.setTextColor(BLACK);
  sprite.setTextSize(1);

  sprite.fillRect(titleArea.x, titleArea.y, titleArea.w, titleArea.h, COLOR16(0, 128, 255));
  sprite.drawString(title, titlePos.x, titlePos.y, 2);

  for (int i = 0; i < 4; i++) {
    int8_t idx = top + i;
    if (idx >= itemCount) break;

    textmenuitem_t *mi = menu + idx;

    uipos_t menuPos = {menuPos0.x, (int16_t)(menuPos0.y + ((menuSize.h + margin.h) * i))};
    uipos_t captionPos = {(int16_t)(menuPos.x + 4), (int16_t)(menuPos.y + 4)};
    uipos_t descrPos = {(int16_t)(menuPos.x + 4), (int16_t)(menuPos.y + 25)};
    uipos_t valuePos = {(int16_t)(menuPos.x + (menuSize.w - 4)), (int16_t)(menuPos.y + 4)};

    uint16_t colorBg = (idx == select) ? LIGHTGREY : DARKGREY;
    sprite.fillRect(menuPos.x, menuPos.y, menuSize.w, menuSize.h, colorBg);

    sprite.setTextColor(BLACK);
    sprite.drawString(mi->caption, captionPos.x, captionPos.y, 2);
    sprite.drawString(mi->hintText, descrPos.x, descrPos.y, 1);
    if (mi->valueDescr != NULL) {
      sprite.setTextColor(BLUE);
      sprite.drawRightString(mi->valueDescr, valuePos.x, valuePos.y, 2);
    }
  }

  sprite.pushSprite(CLIENT_AREA.x, CLIENT_AREA.y);
  sprite.deleteSprite();
}

void AppUI::openTextMenu(const char *title, textmenuitem_t *menu, int8_t itemCount) {
  navmenu_t nav;
  nav.items[0] = {"Prev", true};
  nav.items[1] = {"Next", true};
  nav.items[2] = {"Select", true};
  drawNavBar(&nav);

  int8_t top = 0;
  int8_t select = 0;

  for (int8_t i = 0; i < itemCount; i++) {
    textmenuitem_t *ci = &menu[i];
    if (ci->onUpdateDescr != NULL) ci->onUpdateDescr(&menu[i]);
  }

  bool endFlag = false;
  bool needSave = false;
  while (!endFlag) {
    drawTitleBar();
    drawConfigMenu(title, menu, itemCount, top, select);

    // check the button input
    switch (promptCustom(&nav)) {
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
      textmenuitem_t *ci = &menu[select];
      if (ci->enabled) {
        if (ci->onSelectItem != NULL) ci->onSelectItem(ci);
        if (ci->onUpdateDescr != NULL) ci->onUpdateDescr(ci);

        endFlag = (ci->onSelectItem == NULL);
      }
      break;
    }
  }
}

void AppUI::openMainMenu(mainmenuitem_t *menu, int8_t itemCount) {
  navmenu_t nav;
  nav.items[0] = {"Prev", true};
  nav.items[1] = {"Next", true};
  nav.items[2] = {"Select", true};
  drawNavBar(&nav);

  int8_t top = 0;
  int8_t select = 0;

  while (true) {
    drawTitleBar();
    drawMainMenu(menu, itemCount, top, select);

    switch (promptCustom(&nav)) {
    case BID_BTN_A:  // move the selection to the previous
      select = (select + (itemCount - 1)) % itemCount;
      top = (itemCount < 4) ? 0 : (((select - 3) / 3) * 3);
      break;

    case BID_BTN_B:  // move the selection to the next
      select = (select + 1) % itemCount;
      top = (itemCount < 6) ? 0 : (((select - 3) / 3) * 3);
      break;

    case BID_BTN_C:  // call the onSelect function of the selected item
      mainmenuitem_t *mi = &menu[select];
      if (mi->enabled) {
        if (mi->onSelect != NULL) mi->onSelect(mi);
      }
      break;
    }
  }
}
