#include <BluetoothSerial.h>
#include <M5Stack.h>
#include <SdFat.h>

#include "AppUI.h"
#include "MtkLogger.h"
#include "MtkParser.h"
#include "Resources.h"

#define SD_ACCESS_SPEED 15000000  // 20MHz may cause SD card error

#define POWER_OFF_TIME 100000  // unit: msec
#define LOOP_INTERVAL 50       // uint: msec

typedef struct appstatus {
  uint32_t idleTimer;
  bool sdcAvail;
  bool sppActive;
  uint8_t loggerAddr[16];
} appstatus_t;

typedef struct _appconfig {
  uint16_t length;           // sizeof(appconfig_t)
  uint8_t loggerAddr[6];     // app - address of paired logger
  char loggerName[24];       // logger model ID of pairded logger
  logformat_t logFormat;     // logger - what to record
  uint8_t logDistIdx;        // logger - auto log by distance (meter, 0: disable)
  uint8_t logTimeIdx;        // logger - auto log by time (seconds, 0: disable)
  uint8_t logSpeedIdx;       // logger - auto log by speed (meter/sec, 0:disabled)
  recordmode_t reccordMode;  // logger - log record mode
  trackmode_t trackMode;     // parser - how to divide/put tracks
  uint8_t timeOffsetIdx;     // parser - timezone offset in hours (-12.0 to 12.0)
  bool putPOI;               // parser - treat manually recorded points as POIs
} appconfig_t;

// ******** function prototypes ********
bool isZeroedBytes(void *, uint16_t);
bool checkLoggerPaired();
void saveAppConfig();
bool loadAppConfig(bool);
void bluetoothCallback(esp_spp_cb_event_t, esp_spp_cb_param_t *);
void progressCallback(int32_t);
bool runDownloadLog();
bool runFixRTCtime();
bool runPairWithLogger();
bool runClearFlash();
void onDownloadLogSelected();
void onFixRTCtimeSelected();
void onShowLocationSelected();
void onPairWithLoggerSelected();
void onClearFlashSelected();
void onAppSettingSelected();
void onPrevButtonClick();
void onNextButtonClick();
void onSelectButtonClick();
void onNavButtonPress(btnid_t);
void onDialogOKButtonClick();
void onDialogCancelButtonClick();
void onDialogNaviButtonPress(btnid_t);
void onTimezoneOffsetChange();
void updateAppHint();
void updateConfigMenuDescr();

const char *APP_NAME = "SmallStep";
const char LCD_BRIGHTNESS = 10;

const float timeOffsetValues[] = {-12, -11, -10, -9, -8, -7, -6,  -5,  -4.5, -4,  -3.5,
                                  -3,  -2,  -1,  0,  1,  2,  3,   3.5, 4,    4.5, 5,
                                  5.5, 6,   6.5, 7,  8,  9,  9.5, 10,  12,   13};  // uint: hours
const int16_t logDistValues[] = {0, 10, 30, 50, 100, 200, 300, 500};               // uint: meters
const int16_t logTimeValues[] = {0, 1, 3, 5, 10, 15, 30, 60, 120, 180, 300};       // unit: seconds
const int16_t logSpeedValues[] = {0,  10, 20,  30,  40,  50,  60,  70,
                                  80, 90, 100, 120, 140, 160, 180, 200};  // uint: km/h

AppUI ui = AppUI();
SdFat SDcard;
MtkLogger logger = MtkLogger();
appstatus_t app;
appconfig_t config;
navmenu_t mainnav;
mainmenu_t mainmenu;
cfgmenu_t cfgmenu;
cfgmenu_t submenu;
uint32_t idleTimer;

void updateAppHint() {
  // set the Paied logger name and address as the application hint
  if (!isZeroedBytes(&config.loggerAddr, sizeof(config.loggerAddr))) {
    char addrStr[16];
    sprintf(addrStr, "%02X%02X-%02X%02X-%02X%02X",  // xxxx-xxxx-xxxx
            config.loggerAddr[0], config.loggerAddr[1], config.loggerAddr[2], config.loggerAddr[3],
            config.loggerAddr[4], config.loggerAddr[5]);
    ui.setAppHints(config.loggerName, addrStr);
  } else {
    ui.setAppHints("NO LOGGER", "0000-0000-0000");
  }
}

void bluetoothCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  switch (event) {
  case ESP_SPP_INIT_EVT:
    if (param == NULL) {
      app.sppActive = true;
      ui.drawTitleBar(app.sdcAvail, app.sppActive);
    }
    break;

  case ESP_SPP_OPEN_EVT:  // SPP connection established
    memcpy(app.loggerAddr, param->open.rem_bda, 6);
    break;

  case ESP_SPP_UNINIT_EVT:
    if (param == NULL) {
      app.sppActive = false;
      ui.drawTitleBar(app.sdcAvail, app.sppActive);
    }
    break;
  }
}

void progressCallback(int32_t progRate) {
  ui.drawDialogProgress(progRate);
}

bool isZeroedBytes(void *p, uint16_t size) {
  uint8_t *pb = (uint8_t *)p;

  bool allZero = true;
  for (int i = 0; i < size; i++) {
    allZero &= (pb[i] == 0);
  }

  return allZero;
}

bool checkLoggerPaired() {
  bool paired = (!isZeroedBytes(&config.loggerAddr, sizeof(6)));
  if (!paired) {
    ui.drawDialogFrame("Setup Required");
    ui.printDialogMessage(BLACK, 0, "Pairing has not been done yet.");
    ui.printDialogMessage(BLACK, 1, "Pair with your GPS logger now?");

    if (ui.waitForOkCancel() == BID_OK) {
      ui.printDialogMessage(BLACK, 1, "Pair with your GPS logger now? [ OK ]");
      paired = runPairWithLogger();
      if (paired) ui.waitForOk();
    } else {
      paired = false;
      ui.printDialogMessage(BLACK, 1, "Pair with your GPS logger now? [Cancel]");
      ui.printDialogMessage(BLUE, 2, "The Operation was canceled.");
    }
  }

  return paired;
}

bool checkSDcardAvailable() {
  if (SDcard.card()->sectorCount() == 0) {  // cannot get the sector count
    // print the error message
    ui.drawDialogFrame("SD Card Error");
    ui.printDialogMessage(RED, 0, "A SD card is not available.");
    ui.printDialogMessage(RED, 1, "- Make sure the SD card is inserted");
    ui.printDialogMessage(RED, 2, "- The card must be FAT32 formatted");

    return false;
  }

  return true;
}

bool runDownloadLog() {
  const char TEMP_BIN[] = "download.bin";
  const char TEMP_GPX[] = "download.gpx";
  const char GPX_PREFIX[] = "gpslog_";
  const char DATETIME_FMT[] = "%04d%02d%02d-%02d%02d%02d";

  // return false if the logger is not paired yet
  if (!checkLoggerPaired()) return false;
  if (!checkSDcardAvailable()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Download Log");
  ui.drawNavBar(NULL);

  // print the progress message
  ui.printDialogMessage(BLUE, 0, "Connecting to GPS logger...");
  {
    if (!logger.connect(config.loggerAddr)) {
      ui.printDialogMessage(RED, 0, "Connecting to GPS logger... failed.");
      ui.printDialogMessage(RED, 1, "- Make sure Bluetooth is enabled on your");
      ui.printDialogMessage(RED, 2, "  GPS logger");
      ui.printDialogMessage(RED, 3, "- Power cycling may fix this problem");

      return false;
    }
  }
  ui.printDialogMessage(BLACK, 0, "Connecting to GPS logger... done.");

  File32 binFileW = SDcard.open(TEMP_BIN, (O_CREAT | O_WRITE | O_TRUNC));
  if (!binFileW) {
    logger.disconnect();

    ui.printDialogMessage(RED, 1, "Cannot create a temporaly BIN file.");
    return false;
  }

  ui.printDialogMessage(BLUE, 1, "Downloading log data...");
  {
    if (!logger.downloadLog(&binFileW, &progressCallback)) {
      logger.disconnect();
      binFileW.close();

      ui.printDialogMessage(RED, 1, "Downloading log data... failed.");
      ui.printDialogMessage(RED, 2, "- Keep your logger close to this device");
      ui.printDialogMessage(RED, 3, "- Power cycling may fix this problem");
      return false;
    }

    // disconnect from the logger and close the downloaded file
    logger.disconnect();
    binFileW.close();
  }
  ui.printDialogMessage(BLACK, 1, "Downloading log data... done.");

  // open the downloaded binary file for reading and
  // create a new GPS file for writing
  File32 binFileR = SDcard.open(TEMP_BIN, (O_READ));
  File32 gpxFileW = SDcard.open(TEMP_GPX, (O_CREAT | O_WRITE | O_TRUNC));
  if ((!binFileR) || (!gpxFileW)) {  // BIN or GPX file open failed
    if (binFileR) binFileR.close();
    if (gpxFileW) gpxFileW.close();

    // print the error message
    ui.printDialogMessage(RED, 2, "Cannot open the temporaly BIN file");
    ui.printDialogMessage(RED, 3, "or create a GPX file for output.");
    return false;
  }

  ui.printDialogMessage(BLUE, 2, "Converting data to GPX format...");
  {
    parseopt_t parseopt = {config.trackMode, timeOffsetValues[config.timeOffsetIdx]};

    // convert the binary file to GPX file and get the summary
    MtkParser *parser = new MtkParser();
    parser->setOptions(parseopt);
    parser->convert(&binFileR, &gpxFileW, &progressCallback);
    uint32_t tracks = parser->getTrackCount();
    uint32_t trkpts = parser->getTrkptCount();
    time_t time1st = parser->getFirstTrkpt().time;
    delete parser;

    // close the binary and GPX files
    binFileR.close();
    gpxFileW.close();

    if (trkpts > 0) {
      // make a unique name for the GPX file
      struct tm *ltime = localtime(&time1st);
      char timestr[16], gpxName[32];
      sprintf(timestr, DATETIME_FMT,
              (ltime->tm_year + 1900),  // year (4 digits)
              (ltime->tm_mon + 1),      // month (1-12)
              ltime->tm_mday,           // day of month (1-31)
              ltime->tm_hour, ltime->tm_min, ltime->tm_sec);
      sprintf(gpxName, "%s%s.gpx", GPX_PREFIX, timestr);
      for (uint16_t i = 2; SDcard.exists(gpxName) && (i < 65535); i++) {
        sprintf(gpxName, "%s%s_%02d.gpx", GPX_PREFIX, timestr, i);
      }

      // rename the GPX file (download.gpx -> gpslog_YYYYMMDD-HHMMSS.gpx)
      SDcard.rename(TEMP_GPX, gpxName);

      // make the output message strings
      char outputstr[48], summarystr[48];
      sprintf(outputstr, "Output file : %s", gpxName);
      sprintf(summarystr, "Log summary : %d tracks, %d trkpts", tracks, trkpts);

      // print the result message
      ui.printDialogMessage(BLACK, 2, "Converting data to GPX format... done.");
      ui.printDialogMessage(BLUE, 3, outputstr);
      ui.printDialogMessage(BLUE, 4, summarystr);
    } else {
      // remove the GPX file that has no valid record
      SD.remove(TEMP_GPX);

      // print the result message
      ui.printDialogMessage(BLACK, 2, "Converting data to GPX format... done.");
      ui.printDialogMessage(BLUE, 3, "No output file is saved because of there is");
      ui.printDialogMessage(BLUE, 4, "no valid record in the log data.");
    }
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
  // return false if the logger is not paired yet
  if (!checkLoggerPaired()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Fix RTC time");
  ui.drawNavBar(NULL);

  // print the progress message
  ui.printDialogMessage(BLUE, 0, "Connecting to GPS logger...");
  {
    // connect to the logger
    if (!logger.connect(config.loggerAddr)) {
      ui.printDialogMessage(RED, 0, "Connecting to GPS logger... failed.");
      ui.printDialogMessage(RED, 1, "- Make sure Bluetooth is enabled on your");
      ui.printDialogMessage(RED, 2, "  GPS logger");
      ui.printDialogMessage(RED, 3, "- Power cycling may fix this problem");

      return false;
    }
  }
  ui.printDialogMessage(BLACK, 0, "Connecting to GPS logger... done.");

  ui.printDialogMessage(BLUE, 1, "Setting RTC datetime...");
  {
    if (!logger.fixRTCdatetime()) {
      logger.disconnect();

      ui.printDialogMessage(RED, 1, "Setting RTC datetime... failed.");
      ui.printDialogMessage(RED, 2, "- Keep GPS logger close to this device");

      return false;
    }

    logger.disconnect();
  }
  // print the result message
  ui.printDialogMessage(BLACK, 1, "Setting RTC datetime... done.");
  ui.printDialogMessage(BLUE, 2, "The logger has been restarted to apply the fix.");
  ui.printDialogMessage(BLUE, 3, "Please check the logging status.");

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
void onShowLocationSelected() {
  Serial.printf("[DEBUG] onSetPresetConfigMenuSelected\n");
}

bool runPairWithLogger() {
  const String DEVICE_NAMES[] = {"747PRO GPS", "HOLUX_M-241"};
  const int8_t DEVICE_COUNT = 2;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Pair with Logger");
  ui.drawNavBar(NULL);  // disable the navigation bar

  ui.printDialogMessage(BLACK, 0, "Discovering GPS logger...");

  for (int i = 0; i < DEVICE_COUNT; i++) {
    // print the device name trying to connect
    char msgbuf1[48], msgbuf2[48];
    sprintf(msgbuf1, "- %s", DEVICE_NAMES[i].c_str());
    ui.printDialogMessage(BLUE, (1 + i), msgbuf1);

    if (logger.connect(DEVICE_NAMES[i])) {  // successfully connected
      logger.disconnect();                  // disconnect

      // get the address of the connected device
      uint8_t addr[6];
      memcpy(addr, app.loggerAddr, 6);

      // print the success message
      sprintf(msgbuf1, "- %s : found.", DEVICE_NAMES[i].c_str());
      sprintf(msgbuf2, "Logger address : %02X%02X-%02X%02X-%02X%02X", addr[0], addr[1], addr[2],
              addr[3], addr[4], addr[5]);
      ui.printDialogMessage(BLACK, (1 + i), msgbuf1);
      ui.printDialogMessage(BLUE, (2 + i), "Successfully paired with the discovered logger.");
      ui.printDialogMessage(BLUE, (3 + i), msgbuf2);

      // copy the address and model name to the configuration
      memcpy(config.loggerAddr, addr, 6);
      strncpy(config.loggerName, DEVICE_NAMES[i].c_str(), 23);
      saveAppConfig();

      updateAppHint();
      ui.drawTitleBar(app.sdcAvail, app.sppActive);

      return true;
    } else {  // failed to connect to the device
      sprintf(msgbuf1, "- %s : not found.", DEVICE_NAMES[i].c_str());
      ui.printDialogMessage(BLACK, (1 + i), msgbuf1);
      continue;
    }
  }

  // print the failure message
  ui.printDialogMessage(RED, (1 + DEVICE_COUNT), "Cannot discover any supported logger.");
  ui.printDialogMessage(RED, (2 + DEVICE_COUNT), "- Make sure Bluetooth is enabled on your");
  ui.printDialogMessage(RED, (3 + DEVICE_COUNT), "  GPS logger");
  ui.printDialogMessage(RED, (4 + DEVICE_COUNT), "- Power cycling may fix this problem");

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
  // return false if the logger is not paired yet
  if (!checkLoggerPaired()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Erase Log");
  ui.drawNavBar(NULL);

  // prompt the user to confirm the operation
  ui.printDialogMessage(BLUE, 0, "Are you sure to erase log data?");
  {
    if (ui.waitForOkCancel() == BID_CANCEL) {
      ui.printDialogMessage(BLACK, 0, "Are you sure to erase log data?  [Cancel]");
      ui.printDialogMessage(RED, 1, "The operation was canceled by the user.");
      return false;
    }
  }
  ui.printDialogMessage(BLACK, 0, "Are you sure to erase log data? [ OK ]");
  ui.drawNavBar(NULL);

  // print the progress message
  ui.printDialogMessage(BLUE, 1, "Connecting to GPS logger...");
  {
    // connect to the logger
    if (!logger.connect(config.loggerAddr)) {
      // if failed, print the error message and hint(s)
      ui.printDialogMessage(RED, 1, "Connecting to GPS logger... failed.");
      ui.printDialogMessage(RED, 2, "- Make sure Bluetooth is enabled on your");
      ui.printDialogMessage(RED, 3, "  GPS logger");
      ui.printDialogMessage(RED, 4, "- Power cycling may fix this problem");
      return false;
    }
  }
  ui.printDialogMessage(BLACK, 1, "Connecting to GPS logger... done.");

  ui.printDialogMessage(BLUE, 2, "Erasing log data...");
  {
    // erase the log data
    if (!logger.clearFlash(&progressCallback)) {
      logger.disconnect();

      ui.printDialogMessage(RED, 2, "Erasing log data... failed.");
      ui.printDialogMessage(RED, 3, "- Keep GPS logger close to this device");
      ui.printDialogMessage(RED, 4, "- Power cycling may fix this problem");
    }
    logger.disconnect();

    return false;
  }
  ui.printDialogMessage(BLACK, 2, "Erasing log data... done.");
  ui.printDialogMessage(BLUE, 3, "Hope you have a nice trip next time!");

  return true;
}

void onClearFlashSelected() {
  runClearFlash();
  ui.waitForOk();
}

void onAppSettingSelected() {
  cfgmenu.selIndex = 0;
  cfgmenu.topIndex = 0;

  //  updateConfigMenuDescr();
  ui.drawConfigMenu(&cfgmenu);
  ui.handleInputForConfigMenu(&cfgmenu);

  saveAppConfig();
}

void onPrevButtonClick() {
  mainmenu.selIndex = (mainmenu.selIndex + 5) % 6;
  ui.drawMainMenu(&mainmenu);
}

void onNextButtonClick() {
  mainmenu.selIndex = (mainmenu.selIndex + 1) % 6;
  ui.drawMainMenu(&mainmenu);
}

void onSelectButtonClick() {
  menuitem_t *mi = &(mainmenu.items[mainmenu.selIndex]);
  if (mi->onSelect != NULL) {
    mi->onSelect();
    ui.drawMainMenu(&mainmenu);
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

void updateConfigMenuDescr() {
  for (int i = 0; i < cfgmenu.itemCount; i++) {
    if (cfgmenu.items[i].valueDescr == NULL) {
      cfgmenu.items[i].valueDescr = (char *)malloc(20);
      memset(cfgmenu.items[i].valueDescr, 0, 20);
    }
  }

  cfgitem_t *trkmode = &cfgmenu.items[1];
  if (config.trackMode == TRK_ONE_DAY) {
    strcpy(trkmode->valueDescr, "A track per day");
  } else if (config.trackMode == TRK_AS_IS) {
    strcpy(trkmode->valueDescr, "As log recorded");
  } else {
    strcpy(trkmode->valueDescr, "One track");
  }

  cfgitem_t *tzoffset = &cfgmenu.items[2];
  sprintf(tzoffset->valueDescr, "UTC%+.1f", timeOffsetValues[config.timeOffsetIdx]);

  cfgitem_t *putpoi = &cfgmenu.items[3];
  if (!config.putPOI) {
    strcpy(putpoi->valueDescr, "Disabled");
  } else {
    strcpy(putpoi->valueDescr, "Enabled");
  }

  cfgitem_t *logdist = &cfgmenu.items[4];
  if (logDistValues[config.logDistIdx] == 0) {
    strcpy(logdist->valueDescr, "Disabled");
  } else {
    sprintf(logdist->valueDescr, "%d meters", logDistValues[config.logDistIdx]);
  }

  cfgitem_t *logtime = &cfgmenu.items[5];
  if (logTimeValues[config.logTimeIdx] == 0) {
    strcpy(logtime->valueDescr, "Disabled");
  } else {
    sprintf(logtime->valueDescr, "%d seconds", logTimeValues[config.logTimeIdx]);
  }

  cfgitem_t *logspd = &cfgmenu.items[6];
  if (logSpeedValues[config.logSpeedIdx] == 0) {
    strcpy(logspd->valueDescr, "Disabled");
  } else {
    sprintf(logspd->valueDescr, "%d km/h", logSpeedValues[config.logSpeedIdx]);
  }
}

void onTrackModeChange() {
  config.trackMode = (trackmode_t)(((int)config.trackMode + 1) % 3);
  updateConfigMenuDescr();
}

void onTimezoneOffsetChange() {
  uint8_t valCount = sizeof(timeOffsetValues) / sizeof(float);

  config.timeOffsetIdx = (config.timeOffsetIdx + 1) % valCount;
  updateConfigMenuDescr();
}

void onPutPointOfInterestChange() {
  //
  config.putPOI = (!config.putPOI);
  updateConfigMenuDescr();
}

void onLogByDistanceChange() {
  uint8_t valCount = sizeof(logDistValues) / sizeof(int16_t);

  config.logDistIdx = (config.logDistIdx + 1) % valCount;
  config.logTimeIdx = 0;
  config.logSpeedIdx = 0;
  updateConfigMenuDescr();
}

void onLogByTimeChange() {
  uint8_t valCount = sizeof(logTimeValues) / sizeof(int16_t);

  config.logDistIdx = 0;
  config.logTimeIdx = (config.logTimeIdx + 1) % valCount;
  config.logSpeedIdx = 0;
  updateConfigMenuDescr();
}

void onLogBySpeedChange() {
  uint8_t valCount = sizeof(logSpeedValues) / sizeof(int16_t);

  config.logDistIdx = 0;
  config.logTimeIdx = 0;
  config.logSpeedIdx = (config.logSpeedIdx + 1) % valCount;
  updateConfigMenuDescr();
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

bool loadAppConfig(bool loadDefault) {
  uint8_t *pcfg = (uint8_t *)(&config);
  uint8_t chk = 0;

  Serial.printf("SmallStep.loadConfig: loading configuration data...\n");

  if (!loadDefault) {
    // read configuration data from EEPROM
    for (int8_t i = 0; i < sizeof(appconfig_t); i++) {
      uint8_t b = EEPROM.read(i);
      *pcfg = b;
      chk ^= *pcfg;
      pcfg += 1;

      Serial.printf("%02X ", b);
    }
  }

  // load the default configuration if loadDefault is set to true or
  // the EEPROM data is invalid
  loadDefault |=
      ((config.length != sizeof(appconfig_t)) || (EEPROM.read(sizeof(appconfig_t) != chk)));
  if (loadDefault) {
    memset(&config, 0, sizeof(appconfig_t));
    config.length = sizeof(appconfig_t);
    config.trackMode = TRK_ONE_DAY;
    config.timeOffsetIdx = 14;  // timeOffsetValues[14] = 0 (UTC+0)
    config.logDistIdx = 0;      // logDistValues[0] = 0 (disabled)
    config.logTimeIdx = 5;      // logTimeValues[5] = 15
    config.logSpeedIdx = 0;     // logSpeedValues[0] = 0 (disabled)

    Serial.printf("SmallStep.loadConfig: default configuration loaded\n");

    return false;
  }

  Serial.printf("SmallStep.loadConfig: configuration data loaded\n");

  return true;
}

void setup() {
  // start the serial
  Serial.begin(115200);

  // zero-clear global variables (status & config)
  memset(&app, 0, sizeof(app));
  memset(&config, 0, sizeof(config));

  // initialize the M5Stack
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setBrightness(LCD_BRIGHTNESS);
  EEPROM.begin(sizeof(appconfig_t) + 1);
  SDcard.begin(GPIO_NUM_4, SD_ACCESS_SPEED);

  logger.setEventCallback(bluetoothCallback);
  app.sdcAvail = (SDcard.card()->sectorCount() > 0);

  // navigation menu
  mainnav.items[0] = {"Prev", true};
  mainnav.items[1] = {"Next", true};
  mainnav.items[2] = {"Select", true};

  // main menu
  memset(&mainmenu, 0, sizeof(mainmenu_t));
  mainmenu.itemCount = 6;
  mainmenu.items[0] = {"Download Log", ICON_DOWNLOAD_LOG, true, &onDownloadLogSelected};
  mainmenu.items[1] = {"Fix RTC time", ICON_FIX_RTC, true, &onFixRTCtimeSelected};
  mainmenu.items[2] = {"Erase Log Data", ICON_ERASE_LOG, true, &onClearFlashSelected};
  mainmenu.items[3] = {"Show Location", ICON_SHOW_LOCATION, true, &onShowLocationSelected};
  mainmenu.items[4] = {"Pair w/ Logger", ICON_PAIR_LOGGER, true, &onPairWithLoggerSelected};
  mainmenu.items[5] = {"App Settings", ICON_APP_SETTINGS, true, &onAppSettingSelected};

  // config menu
  memset(&cfgmenu, 0, sizeof(cfgmenu_t));
  cfgmenu.itemCount = 4;
  cfgmenu.items[0] = {"Save and exit", "Return to the main menu", NULL, NULL, NULL};
  cfgmenu.items[1] = {"Output settings", "", (char *)malloc(8), NULL, NULL};
  strcpy(cfgmenu.items[1].valueDescr, ">>");
  cfgmenu.items[2] = {"Log mode settings", "", (char *)malloc(8), NULL, NULL};
  strcpy(cfgmenu.items[2].valueDescr, ">>");
  cfgmenu.items[3] = {"Log format settings", "", (char *)malloc(8), NULL, NULL};
  strcpy(cfgmenu.items[3].valueDescr, ">>");

  // load the configuration data
  M5.update();
  bool defaultConfig = (M5.BtnA.read() == 1);
  bool firstBoot = (loadAppConfig(defaultConfig) == false);

  // draw the screen
  updateAppHint();
  ui.setAppTitle(APP_NAME);
  ui.drawTitleBar(app.sdcAvail, app.sppActive);
  ui.drawNavBar(&mainnav);

  // if left button is pressed, clear the configuration
  if (firstBoot) {
    ui.drawDialogFrame("Hello!");
    ui.printDialogMessage(BLACK, 0, "The default configuration is loaded.");
    ui.printDialogMessage(BLACK, 1, "Please run the app setting menu and");
    ui.printDialogMessage(BLACK, 2, "pairing with your GPS logger.");
    ui.waitForOk();
  }
  ui.drawMainMenu(&mainmenu);

  app.idleTimer = millis() + POWER_OFF_TIME;
}

void loop() {
  btnid_t bid = ui.checkButtonInput(&mainnav);
  if (bid != BID_NONE) {
    onNavButtonPress((btnid_t)bid);

    ui.drawMainMenu(&mainmenu);
    ui.drawNavBar(&mainnav);

    app.idleTimer = millis() + POWER_OFF_TIME;  // reset the idle timer
  }

  // if the idle timer is expired, shutdown the system
  if (millis() > app.idleTimer) {
    SDcard.end();         // stop SD card access
    M5.Power.powerOFF();  // power off
  }

  delay(LOOP_INTERVAL);
}
