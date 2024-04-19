#include <BluetoothSerial.h>
#include <M5Stack.h>
#include <SdFat.h>

#include "AppUI.h"
#include "LoggerManager.h"
#include "MtkParser.h"
#include "Resources.h"

#define SD_ACCESS_SPEED 15000000  // 20MHz may cause SD card error

#define POWER_OFF_TIME 100000  // unit: msec
#define LOOP_INTERVAL 50       // uint: msec

typedef struct appstatus {
  navmenu_t nav;
  mainmenu_t menu;
  uint32_t idleTimer;
  bool sdCardAvailable;
  bool sppConnected;
  uint8_t sppClientAddr[6];
} appstatus_t;

typedef struct _appconfig {
  uint16_t length;           // sizeof(appconfig_t)
  uint8_t loggerAddress[6];  // app - address of paired logger
  char loggerModel[24];      // logger model ID of pairded logger
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
bool runDownloadLog();
bool runFixRTCtime();
bool runPairWithLogger();
bool runClearFlash();
void onDownloadLogSelected();
void onFixRTCtimeSelected();
void onSetPresetSelected();
void onPairWithLoggerSelected();
void onClearFlashSelected();
void onAppSettingSelected();
void putBitmapIcon(TFT_eSprite *, const uint8_t *, int16_t, int16_t, int32_t);
void drawApplicationIcon(TFT_eSprite *, int16_t, int16_t);
void drawBluetoothIcon(TFT_eSprite *, int16_t, int16_t);
void putSDcardIcon(TFT_eSprite *, int16_t, int16_t);
void putBatteryIcon(TFT_eSprite *, int16_t, int16_t);
void onPrevButtonClick();
void onNextButtonClick();
void onSelectButtonClick();
void onNavButtonPress(btnid_t);
void onDialogOKButtonClick();
void onDialogCancelButtonClick();
void onDialogNaviButtonPress(btnid_t);

const char APP_NAME[] = "SmallStep";
const char LCD_BRIGHTNESS = 10;

AppUI ui = AppUI();
SdFat SDcard;
LoggerManager logger = LoggerManager();
appstatus_t app;
appconfig_t config;

void bluetoothCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  switch (event) {
    case ESP_SPP_OPEN_EVT:  // SPP connection established
      memcpy(app.sppClientAddr, param->open.rem_bda, 6);
      app.sppConnected = true;
      break;

    case ESP_SPP_CLOSE_EVT:
      app.sppConnected = false;
      //      ui.drawTitleBar(APP_NAME);
      break;
  }
}

void progressCallback(int32_t progRate) { ui.drawDialogProgress(progRate); }

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

 change format: PMTK182,1,2,80060E7F
 */

bool validAddress(uint8_t *addr) {
  uint16_t addrsum = 0;
  for (int i = 0; i < 6; i++) {
    addrsum += addr[i];
  }
  return (addrsum > 0);
}

bool runDownloadLog() {
  const char TEMP_BIN[] = "download.bin";
  const char TEMP_GPX[] = "download.gpx";
  const char GPX_PREFIX[] = "gpslog_";
  const char DATETIME_FMT[] = "%04d%02d%02d-%02d%02d%02d";

  // draw dialog frame
  ui.drawDialogFrame("Download Log");
  ui.drawNavBar(NULL);

  if (!validAddress(config.loggerAddress)) {
    String msgs[] = {"Pairing has not been done yet.",
                     "- Pair with your logger first"};
    ui.drawDialogMessage(RED, 0, msgs, 2);

    return false;
  }

  File32 binFileW = SDcard.open(TEMP_BIN, (O_CREAT | O_WRITE | O_TRUNC));
  if (!binFileW) {
    String msgs[] = {"Could not open file on the SD card.",
                     "- Make sure SD card is available.",
                     "  It must be a <16GB, FAT32 formatted card."};
    ui.drawDialogMessage(RED, 0, msgs, 3);

    return false;
  }

  // print the progress message
  ui.drawDialogMessage(BLUE, 0, "Connecting to GPS logger...");
  if (!logger.connect(config.loggerAddress)) {
    binFileW.close();

    String msgs[] = {"Connecting to GPS logger... failed.",
                     "- Make sure Bluetooth on your logger is enabled",
                     "  and keep it close to this device",
                     "- Power cycling may fix this problem"};
    ui.drawDialogMessage(RED, 0, msgs, 4);

    return false;
  }

  // print the progress message
  ui.drawDialogMessage(BLACK, 0, "Connecting to GPS logger... done.");
  ui.drawDialogMessage(BLUE, 1, "Downloading log data...");
  if (!logger.downloadLog(&binFileW, &progressCallback)) {
    logger.disconnect();
    binFileW.close();

    String msgs[] = {"Downloading log data... failed.",
                     "- Keep GPS logger close to this device",
                     "- Power cycling may fix this problem"};
    ui.drawDialogMessage(RED, 1, msgs, 3);

    return false;
  }

  // disconnect from the logger and close the downloaded file
  logger.disconnect();
  binFileW.close();

  // print the progress message
  ui.drawDialogMessage(BLACK, 1, "Downloading log data... done.");
  ui.drawDialogMessage(BLUE, 2, "Converting data to GPX file...");

  // open the downloaded binary file for reading and
  // create a new GPS file for writing
  File32 binFileR = SDcard.open(TEMP_BIN, (O_READ));
  File32 gpxFileW = SDcard.open(TEMP_GPX, (O_CREAT | O_WRITE | O_TRUNC));

  if ((!binFileR) || (!gpxFileW)) {  // BIN or GPX file open failed
    // print the error message
    String msgs[] = {"Converting data to GPX file... failed.",
                     "Cannot open the downloaded BIN file or",
                     "create a new GPX file."};
    ui.drawDialogMessage(RED, 2, msgs, 3);

    return false;
  }

  // convert the binary file to GPX file and get the summary
  MtkParser *parser = new MtkParser();
  parser->setTimeOffset(9.0);
  parser->convert(&binFileR, &gpxFileW, &progressCallback);
  uint32_t tracks = parser->getTrackCount();
  uint32_t trkpts = parser->getTrkptCount();
  time_t time1st = parser->getFirstTrkpt().time;
  delete parser;

  // close the binary and GPX files
  binFileR.close();
  gpxFileW.close();

  if (trkpts > 0) {
    // rename the GPX file (download.gpx -> gpslog_YYYYMMDD-HHMMSS.gpx)
    struct tm *ltime = localtime(&time1st);
    char timestr[16], gpxName[32];
    sprintf(timestr, DATETIME_FMT, (ltime->tm_year + 1900), (ltime->tm_mon + 1),
            ltime->tm_mday, ltime->tm_hour, ltime->tm_min, ltime->tm_sec);
    sprintf(gpxName, "%s%s.gpx", GPX_PREFIX, timestr);
    for (uint16_t i = 2; SDcard.exists(gpxName) && (i < 65535); i++) {
      sprintf(gpxName, "%s%s_%02d.gpx", GPX_PREFIX, timestr, i);
    }
    SDcard.rename(TEMP_GPX, gpxName);

    // print the result message
    char outputstr[48], summarystr[48];
    sprintf(outputstr, "Output file : %s", gpxName);
    sprintf(summarystr, "Log summary : %d tracks, %d trkpts", tracks, trkpts);
    ui.drawDialogMessage(BLACK, 2, "Converting data to GPX file... done.");
    ui.drawDialogMessage(BLUE, 3, outputstr);
    ui.drawDialogMessage(BLUE, 4, summarystr);
  } else {
    // print the error message
    SD.remove(TEMP_GPX);

    String msgs[] = {"Converting data to GPX file... failed.",
                     "No output file created because of no valid",
                     "record found in the downloaded log data."};
    ui.drawDialogMessage(RED, 2, msgs, 3);
  }

  return true;
}

/**
 *
 */
void onDownloadLogSelected() {
  runDownloadLog();
  ui.waitForOk();
}

bool runFixRTCtime() {
  ui.drawDialogFrame("Fix RTC time");
  ui.drawNavBar(NULL);

  if (!validAddress(config.loggerAddress)) {
    String msgs[] = {"Pairing has not been done yet.",
                     "- Pair with your logger first"};
    ui.drawDialogMessage(RED, 0, msgs, 2);
    return false;
  }

  // print the progress message
  ui.drawDialogMessage(BLUE, 0, "Connecting to GPS logger...");

  // connect to the logger
  if (!logger.connect(config.loggerAddress)) {
    String msgs[] = {"Connecting to GPS logger... failed.",
                     "- Make sure Bluetooth is turned on",
                     "  on your GPS logger",
                     "- Power cycling may fix this problem"};
    ui.drawDialogMessage(RED, 0, msgs, 3);
    return false;
  }

  // print the progress message
  ui.drawDialogMessage(BLACK, 0, "Connecting to GPS logger... done.");
  ui.drawDialogMessage(BLUE, 1, "Fixing RTC datetime...");

  if (!logger.fixRTCdatetime()) {
    String msgs[] = {"Fixing RTC time... failed.",
                     "- Keep GPS logger close to this device",
                     "- Power cycling may fix this problem"};
    ui.drawDialogMessage(RED, 1, msgs, 3);
  }

  // disconnect from the logger
  logger.disconnect();

  // print the result message
  ui.drawDialogMessage(BLACK, 1, "Fixing RTC datetime... done.");
  ui.drawDialogMessage(BLUE, 2, "The GPS week number rollover is fixed.");

  return true;
}

/**
 *
 */
void onFixRTCtimeSelected() {
  runFixRTCtime();
  ui.waitForOk();
}

/**
 *
 */
void onSetPresetSelected() {
  Serial.printf("[DEBUG] onSetPresetConfigMenuSelected\n");
}

bool runPairWithLogger() {
  const String DEVICE_NAMES[] = {"747PRO GPS", "HOLUX_M-241"};
  const int8_t DEVICE_COUNT = 2;

  ui.drawDialogFrame("Pair with Logger");
  ui.drawNavBar(NULL);  // disable the navigation bar

  ui.drawDialogMessage(BLACK, 0, "Discovering GPS logger...");

  for (int i = 0; i < DEVICE_COUNT; i++) {
    // print the device name trying to connect
    char msgbuf1[48], msgbuf2[48];
    sprintf(msgbuf1, "- %s", DEVICE_NAMES[i].c_str());
    ui.drawDialogMessage(BLUE, (1 + i), msgbuf1);

    if (logger.connect(DEVICE_NAMES[i])) {  // successfully connected
      logger.disconnect();                  // disconnect

      // get the address of the connected device
      uint8_t addr[6];
      memcpy(addr, app.sppClientAddr, 6);

      // print the success message
      sprintf(msgbuf1, "- %s : found.", DEVICE_NAMES[i].c_str());
      sprintf(msgbuf2, "Logger address : %02X%02X-%02X%02X-%02X%02X", addr[0],
              addr[1], addr[2], addr[3], addr[4], addr[5]);
      ui.drawDialogMessage(BLACK, (1 + i), msgbuf1);
      ui.drawDialogMessage(BLUE, (2 + i),
                           "Successfully paired with the discovered logger.");
      ui.drawDialogMessage(BLUE, (3 + i), msgbuf2);

      // copy the address and model name to the configuration
      memcpy(config.loggerAddress, addr, 6);
      strncpy(config.loggerModel, DEVICE_NAMES[i].c_str(), 23);
      saveAppConfig();

      return true;
    } else {  // failed to connect to the device
      sprintf(msgbuf1, "- %s : not found.", DEVICE_NAMES[i].c_str());
      ui.drawDialogMessage(BLACK, (1 + i), msgbuf1);
      continue;
    }
  }

  // print the failure message
  String msgs[] = {"Failed to discover any supported logger.",
                   "- Make sure Bluetooth is enabled on your logger",
                   "- Power cycling may fix this problem"};
  ui.drawDialogMessage(RED, (1 + DEVICE_COUNT), msgs, 3);

  return false;
}

/**
 *
 */
void onPairWithLoggerSelected() {
  runPairWithLogger();
  ui.waitForOk();
}

bool runClearFlash() {
  ui.drawDialogFrame("Erase Log");
  ui.drawNavBar(NULL);

  if (!validAddress(config.loggerAddress)) {
    String msgs[] = {"Pairing has not been done yet.",
                     "- Pair with your logger first"};
    ui.drawDialogMessage(RED, 0, msgs, 2);
    return false;
  }

  ui.drawDialogMessage(BLUE, 0, "Are you sure to erase log data?");
  if (ui.waitForOkCancel() == BID_CANCEL) {
    ui.drawDialogMessage(BLACK, 0, "Are you sure to erase log data?  [Cancel]");
    ui.drawDialogMessage(RED, 1, "The operation was canceled by the user.");
    return false;
  }

  ui.drawDialogMessage(BLACK, 0, "Are you sure to erase log data?  [ OK ]");
  ui.drawNavBar(NULL);

  // print the progress message
  ui.drawDialogMessage(BLUE, 1, "Connecting to GPS logger...");

  // connect to the logger
  if (!logger.connect(config.loggerAddress)) {
    String msgs[] = {"Connecting to GPS logger... failed.",
                     "- Make sure Bluetooth is turned on",
                     "  on your GPS logger",
                     "- Power cycling may fix this problem"};
    ui.drawDialogMessage(RED, 1, msgs, 3);
    return false;
  }

  // print the progress message
  ui.drawDialogMessage(BLACK, 1, "Connecting to GPS logger... done.");
  ui.drawDialogMessage(BLUE, 2, "Erasing log data...");
  if (!logger.clearFlash(&progressCallback)) {
    logger.disconnect();

    String msgs[] = {"Erasing log data... failed.",
                     "- Keep GPS logger close to this device",
                     "- Power cycling may fix this problem"};
    ui.drawDialogMessage(RED, 2, msgs, 3);
  }
  logger.disconnect();

  // print the result message
  ui.drawDialogMessage(BLACK, 2, "Erasing log data... done.");
  ui.drawDialogMessage(BLUE, 3, "Hope you have a nice trip next time!");

  return true;
}

void onClearFlashSelected() {
  runClearFlash();
  ui.waitForOk();
}

void onAppSettingSelected() {}

void onPrevButtonClick() {
  app.menu.selectedIndex = (app.menu.selectedIndex + 5) % 6;
  ui.drawMainMenu(&app.menu);
}

void onNextButtonClick() {
  app.menu.selectedIndex = (app.menu.selectedIndex + 1) % 6;
  ui.drawMainMenu(&app.menu);
}

void onSelectButtonClick() {
  menuitem_t *mi = &(app.menu.items[app.menu.selectedIndex]);
  if (mi->onSelect != NULL) {
    mi->onSelect();
    ui.drawMainMenu(&app.menu);
  }
}

void onNavButtonPress(btnid_t bid) {
  switch (bid) {
    case BID_BTN_A:
      onPrevButtonClick();
      break;
    case BID_BTN_B:
      onNextButtonClick();
      break;
    case BID_BTN_C:
      onSelectButtonClick();
      break;
  }
}

void saveAppConfig() {
  uint8_t *pcfg = (uint8_t *)(&config);
  uint8_t chk = 0;

  // write configuration data to EEPROM
  for (int8_t i = 0; i < sizeof(appconfig_t); i++) {
    EEPROM.write(i, *pcfg);
    chk ^= *pcfg;
    pcfg += 1;
  }
  EEPROM.write(sizeof(appconfig_t), chk);
  EEPROM.commit();

  Serial.printf("SmallStep.saveConfig: configuration data saved\n");
}

bool loadAppConfig() {
  uint8_t *pcfg = (uint8_t *)(&config);
  uint8_t chk = 0;

  for (int8_t i = 0; i < sizeof(appconfig_t); i++) {
    *pcfg = EEPROM.read(i);
    chk ^= *pcfg;
    pcfg += 1;
  }

  // if checksum is invalid, clear configuration
  if ((config.length != sizeof(config)) ||
      (EEPROM.read(sizeof(appconfig_t) != chk))) {
    memset(&config, 0, sizeof(appconfig_t));
    config.length = sizeof(appconfig_t);

    Serial.printf("SmallStep.loadConfig: default configuration loaded\n");

    return false;
  }

  Serial.printf("SmallStep.loadConfig: configuration data loaded\n");

  return true;
}

void setup() {
  // start the serial
  Serial.begin(115200);

  // initialize the M5Stack
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setBrightness(LCD_BRIGHTNESS);

  // zero-clear global variables (status & config)
  memset(&app, 0, sizeof(app));
  memset(&config, 0, sizeof(config));

  EEPROM.begin(sizeof(appconfig_t) + 1);
  loadAppConfig();

  // initialize the SD card
  if (!SDcard.begin(GPIO_NUM_4, SD_ACCESS_SPEED)) {
    Serial.printf("SmallStep.setup: failed to initialize the SD card.\n");
  }

  logger.setEventCallback(bluetoothCallback);

  // navigation menu
  app.nav.onButtonPress = &(onNavButtonPress);
  app.nav.items[0].caption = "Prev";
  app.nav.items[0].enabled = true;
  app.nav.items[1].caption = "Next";
  app.nav.items[1].enabled = true;
  app.nav.items[2].caption = "Select";
  app.nav.items[2].enabled = true;

  // main menu icons
  app.menu.items[0].caption = "Download Log";
  app.menu.items[0].iconData = ICON_DOWNLOAD_LOG;
  app.menu.items[0].onSelect = &(onDownloadLogSelected);
  app.menu.items[1].caption = "Fix RTC time";
  app.menu.items[1].iconData = ICON_FIX_RTC;
  app.menu.items[1].onSelect = &(onFixRTCtimeSelected);
  app.menu.items[2].caption = "Erase Log Data";
  app.menu.items[2].iconData = ICON_ERASE_LOG;
  app.menu.items[2].onSelect = &(onClearFlashSelected);
  app.menu.items[3].caption = "Show Location";
  app.menu.items[3].iconData = ICON_SET_PRESET;
  app.menu.items[3].onSelect = &(onSetPresetSelected);
  app.menu.items[4].caption = "Pair w/ Logger";
  app.menu.items[4].iconData = ICON_PAIR_LOGGER;
  app.menu.items[4].onSelect = &(onPairWithLoggerSelected);
  app.menu.items[5].caption = "App Settings";
  app.menu.items[5].iconData = ICON_APP_SETTINGS;
  app.menu.items[5].onSelect = &(onAppSettingSelected);

  // draw the initial screen
  ui.drawTitleBar(APP_NAME);
  ui.drawMainMenu(&app.menu);
  ui.drawNavBar(&app.nav);

  app.idleTimer = millis() + POWER_OFF_TIME;
}

void loop() {
  btnid_t bid = ui.checkButtonInput(&app.nav);
  if (bid != BID_NONE) {
    if (app.nav.onButtonPress != NULL) {
      app.nav.onButtonPress((btnid_t)bid);

      ui.drawNavBar(&app.nav);
      ui.drawMainMenu(&app.menu);
    }

    app.idleTimer = millis() + POWER_OFF_TIME;  // reset the idle timer
  }

  // if the idle timer is expired, shutdown the system
  if (millis() > app.idleTimer) {
    SDcard.end();         // stop SD card access
    M5.Power.powerOFF();  // power off
  }

  delay(LOOP_INTERVAL);
}
