#include <BluetoothSerial.h>
#include <M5Stack.h>
#include <SdFat.h>

#include "AppUI.h"
#include "MtkLogger.h"
#include "MtkParser.h"
#include "Resources.h"

/* ######## Important Notice ######## */
/* You MUST increase RX_QUEUE_SIZE 512 to 2024 (bytes) in BluetoothSerial.h. */
/* Otherwise, the BluetoothSerial library will not work properly. */

#define SD_ACCESS_SPEED 15000000  // 20MHz may cause SD card error
#define BT_ADDR_LEN 6
#define DEV_NAME_LEN 20
#define IDLE_SHUTDOWN true  // unit: msec

#define DEFAULT_TRACK_MODE TRK_ONE_DAY
#define DEFAULT_TIME_OFFSET_IDX 14
#define DEFAULT_LOG_DIST_IDX  0
#define DEFAULT_LOG_TIME_IDX  4
#define DEFAULT_LOG_SPEED_IDX 0
#define DEFAULT_LOG_FORMAT (REG_FIX | REG_TIME | REG_LON | REG_LAT | REG_ELE | REG_SPEED)

typedef struct appstatus {
  bool sdcAvail;
  bool sppActive;
  uint8_t loggerAddr[BT_ADDR_LEN];
} appstatus_t;

typedef struct _appconfig {
  uint16_t length;         // sizeof(appconfig_t)
  uint8_t loggerAddr[BT_ADDR_LEN]; // app - address of paired logger
  char loggerName[DEV_NAME_LEN];   // app - name of pairded logger
  trackmode_t trackMode;   // parser - how to divide/put tracks
  uint8_t timeOffsetIdx;   // parser - timezone offset in hours
  bool putWaypt;           // parser - treat manually recorded points as Wpts
  bool leaveBinFile;       // parser - leave the BIN file after conversion
  uint8_t logDistIdx;      // log mode - auto log by distance (meter, 0: disable)
  uint8_t logTimeIdx;      // log mode - auto log by time (seconds, 0: disable)
  uint8_t logSpeedIdx;     // log mode - auto log by speed (meter/sec, 0:disabled)
  bool logFullStop;        // log mode - stop logging when flash is full
  uint32_t logFormat;      // log format - Contents to be recorded 
} appconfig_t;

// ******** function prototypes ********
bool isZeroedBytes(void *, uint16_t);
bool isLoggerPaired();
bool isSDcardAvailable();
void saveAppConfig();
bool loadAppConfig(bool);
void bluetoothCallback(esp_spp_cb_event_t, esp_spp_cb_param_t *);
void progressCallback(int32_t);
void setValueDescrByBool(bool, char *);
bool runDownloadLog();
bool runFixRTCtime();
bool runPairWithLogger();
bool runClearFlash();
void onDownloadLogSelect(menuitem_t *);
void onFixRTCtimeSelect(menuitem_t *);
void onShowLocationSelect(menuitem_t *);
void onPairWithLoggerSelect(menuitem_t *);
void onClearFlashSelect(menuitem_t *);
void onAppSettingSelect(menuitem_t *);
void onTimezoneUpdate(cfgitem_t *);
void onTimezoneSelect(cfgitem_t *);
void onTrackModeSelect(cfgitem_t *);
void onTrackModeUpdate(cfgitem_t *);
void onPutWayptSelect(cfgitem_t *);
void onPutWayptUpdate(cfgitem_t *);
void onLogByDistanceSelect(cfgitem_t *);
void onLogByDistanceUpdate(cfgitem_t *);
void onLogByTimeSelect(cfgitem_t *);
void onLogByTimeUpdate(cfgitem_t *);
void onLogBySpeedSelect(cfgitem_t *);
void onLogBySpeedUpdate(cfgitem_t *);
void onLogFullActionSelect(cfgitem_t *);
void onLogFullActionUpdate(cfgitem_t *);
void onLoadDefaultFormatSelect(cfgitem_t *);
void onLoadDefaultFormatUpdate(cfgitem_t *);
void onRecordRCRSelect(cfgitem_t *);
void onRecordRCRUpdate(cfgitem_t *);
void onRecordValidSelect(cfgitem_t *);
void onRecordValidUpdate(cfgitem_t *);
void onRecordSpeedSelect(cfgitem_t *);
void onRecordSpeedUpdate(cfgitem_t *);
void onRecordAltitudeSelect(cfgitem_t *);
void onRecordAltitudeUpdate(cfgitem_t *);
void onRecordHeadingSelect(cfgitem_t *);
void onRecordHeadingUpdate(cfgitem_t *);
void onRecordDistanceSelect(cfgitem_t *);
void onRecordDistanceUpdate(cfgitem_t *);
void onRecordDGPSSelect(cfgitem_t *);
void onRecordDOPSelect(cfgitem_t *);
void onRecordSatInViewSelect(cfgitem_t *);
void onOutputSubMenuSelect(cfgitem_t *);
void onLogModeSubMenuSelect(cfgitem_t *);
void onLogFormatSubMenuSelect(cfgitem_t *);
void updateAppHint();

const char *APP_NAME = "SmallStep";
const char LCD_BRIGHTNESS = 10;

const float TIME_OFFSET_VALUES[] = {-12, -11, -10, -9, -8, -7, -6,  -5,  -4.5, -4,  -3.5,
                                    -3,  -2,  -1,  0,  1,  2,  3,   3.5, 4,    4.5, 5,
                                    5.5, 6,   6.5, 7,  8,  9,  9.5, 10,  12,   13};  // uint: hours
const int16_t LOG_DIST_VALUES[] = {0, 10, 30, 50, 100, 200, 300, 500};               // uint: meters
const int16_t LOG_TIME_VALUES[] = {0, 1, 3, 5, 10, 15, 30, 60, 120, 180, 300};       // unit: seconds
const int16_t LOG_SPEED_VALUES[] = {0,   10,  20,  30,  40,  50,  60,  70, 80, 90,
                                    100, 120, 140, 160, 180, 200, 250, 300};  // uint: km/h

menuitem_t menuMain[] = {
  // {caption, iconData, enabled, onSelect}
  {"Download Log", ICON_DOWNLOAD_LOG, true, &onDownloadLogSelect},
  {"Fix RTC time", ICON_FIX_RTC, true, &onFixRTCtimeSelect},
  {"Erase Log Data", ICON_ERASE_LOG, true, &onClearFlashSelect},
  {"TEST01", NULL, true, NULL},
  {"TEST02", NULL, true, NULL},
  {"Show Location", ICON_SHOW_LOCATION, true, &onShowLocationSelect},
  {"Pair w/ Logger", ICON_PAIR_LOGGER, true, &onPairWithLoggerSelect},
  {"App Settings", ICON_APP_SETTINGS, true, &onAppSettingSelect},
};

cfgitem_t cfgOutput[] = {
  // {caption, descr, valueDescr, onSelect, valueDescrUpdate}
  {"Back", "Exit this menu", "<<", NULL, NULL},
  {"Track mode", "How to divide tracks in GPX file", "", &onTrackModeSelect, &onTrackModeUpdate},
  {"Timezone offset", "UTC offset for 'a track per day' mode", "", &onTimezoneSelect, &onTimezoneUpdate},  
  {"Save POIs as waypts", "Convert POIs to waypts. Need RCR enabled", "", &onPutWayptSelect, &onPutWayptUpdate},
  {"Leave BIN file", "Save BIN file with the same name as GPX file", "", NULL, NULL},
};

cfgitem_t cfgLogMode[] = {
  // {caption, descr, valueDescr, onSelect, valueDescrUpdate}
  {"Back", "Exit this menu", "<<", NULL, NULL},
  {"Log by distance", "Auto log by moving distance", "", &onLogByDistanceSelect, &onLogByDistanceUpdate},
  {"Log by time", "Auto log by elapsed time", "", &onLogByTimeSelect, &onLogByTimeUpdate},
  {"Log by speed", "Auto log by exceeded speed", "", &onLogBySpeedSelect, &onLogBySpeedUpdate},
  {"Flash full behavior", "Action when flash memory is full", "", &onLogFullActionSelect, &onLogFullActionUpdate},
};

cfgitem_t cfgLogFormat[] = {
  // {caption, descr, valueDescr, onSelect, valueDescrUpdate}
  {"Back", "Exit this menu", "<<", NULL, NULL},
  {"Load defaults", "Restore to the default log format", "", &onLoadDefaultFormatSelect, &onLoadDefaultFormatUpdate},
  {"RCR", "Store record reason (dist/time/speed/user)", "", &onRecordRCRSelect, &onRecordRCRUpdate},
  {"Valid", "Store positioning status (valid/invalid)", "", &onRecordValidSelect, &onRecordValidUpdate}, // A: valid, V: invalid
  {"Speed", "Store speed", "", &onRecordSpeedSelect, &onRecordSpeedUpdate},
  {"Altitude", "Store altitude", "", &onRecordAltitudeSelect, &onRecordAltitudeUpdate},
  {"Heading", "Store moving direction", "", &onRecordHeadingSelect, &onRecordHeadingUpdate},
  {"Distance", "Store moving distance", "", &onRecordDistanceSelect, &onRecordDistanceUpdate},
  {"DGPS info", "Store differencial GPS data", "Always disabled", &onRecordDGPSSelect, NULL},
  {"DOP info", "Store dilution of precision data", "Always disabled", &onRecordDOPSelect, NULL},
  {"SAT info", "Store satellite in view data", "Always disabled", &onRecordSatInViewSelect, NULL},
};

cfgitem_t cfgMain[] = {
  // {caption, descr, valueDescr, onSelect, valueDescrUpdate}
  {"Save and exit", "Return to the main menu", "", NULL, NULL},
  {"Output settings", "Output options", ">>", &onOutputSubMenuSelect, NULL},
  {"Log mode settings", "Logging behavior of the logger", ">>", &onLogModeSubMenuSelect, NULL},
  {"Log format settings", "Contents to be stored on the logger", ">>", &onLogFormatSubMenuSelect, NULL},
  {"Clear all settings", "Erase the current settings of SmallStep", "", NULL, NULL},
};

AppUI ui = AppUI();
SdFat SDcard;
MtkLogger logger = MtkLogger();
appstatus_t app;
appconfig_t cfg;
uint32_t idleTimer;

void updateAppHint() {
  // set the Paied logger name and address as the application hint
  if (!isZeroedBytes(&cfg.loggerAddr, sizeof(cfg.loggerAddr))) {
    char addrStr[16];
    sprintf(addrStr, "%02X%02X-%02X%02X-%02X%02X",  // xxxx-xxxx-xxxx
            cfg.loggerAddr[0], cfg.loggerAddr[1], cfg.loggerAddr[2],
            cfg.loggerAddr[3], cfg.loggerAddr[4], cfg.loggerAddr[5]);
    ui.setAppHints(cfg.loggerName, addrStr);
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
    memcpy(app.loggerAddr, param->open.rem_bda, BT_ADDR_LEN);
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

bool isLoggerPaired() {
  bool paired = (!isZeroedBytes(&cfg.loggerAddr, BT_ADDR_LEN));
  if (!paired) {
    ui.drawDialogFrame("Setup Required");
    ui.drawDialogMessage(BLACK, 0, "Pairing has not been done yet.");
    ui.drawDialogMessage(BLACK, 1, "Pair with your GPS logger now?");

    if (ui.waitForInputOkCancel(IDLE_SHUTDOWN) == BID_OK) {
      ui.drawDialogMessage(BLACK, 1, "Pair with your GPS logger now? [ OK ]");
      paired = runPairWithLogger();
      if (paired) ui.waitForInputOk(IDLE_SHUTDOWN);
    } else {
      paired = false;
      ui.drawDialogMessage(BLACK, 1, "Pair with your GPS logger now? [Cancel]");
      ui.drawDialogMessage(BLUE, 2, "The Operation was canceled.");
    }
  }

  return paired;
}

bool isSDcardAvailable() {
  if (SDcard.card()->sectorCount() == 0) {  // cannot get the sector count
    // print the error message
    ui.drawDialogFrame("SD Card Error");
    ui.drawDialogMessage(RED, 0, "A SD card is not available.");
    ui.drawDialogMessage(RED, 1, "- Make sure the SD card is inserted");
    ui.drawDialogMessage(RED, 2, "- The card must be FAT32 formatted");

    return false;
  }

  return true;
}

void setValueDescrByBool(bool value, char *descr) {
  if (value) {
    strcpy(descr, "Enabled");
  } else {
    strcpy(descr, "Disabled");
  }
}

bool runDownloadLog() {
  const char TEMP_BIN[] = "download.bin";
  const char TEMP_GPX[] = "download.gpx";
  const char GPX_PREFIX[] = "gpslog_";
  const char DATETIME_FMT[] = "%04d%02d%02d-%02d%02d%02d";

  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;
  if (!isSDcardAvailable()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Download Log");
  ui.drawNavBar(NULL);

  // print the progress message
  ui.drawDialogMessage(BLUE, 0, "Connecting to GPS logger...");
  {
    if (!logger.connect(cfg.loggerAddr)) {
      ui.drawDialogMessage(RED, 0, "Connecting to GPS logger... failed.");
      ui.drawDialogMessage(RED, 1, "- Make sure Bluetooth is enabled on your");
      ui.drawDialogMessage(RED, 2, "  GPS logger");
      ui.drawDialogMessage(RED, 3, "- Power cycling may fix this problem");

      return false;
    }
  }
  ui.drawDialogMessage(BLACK, 0, "Connecting to GPS logger... done.");

  File32 binFileW = SDcard.open(TEMP_BIN, (O_CREAT | O_WRITE | O_TRUNC));
  if (!binFileW) {
    logger.disconnect();

    ui.drawDialogMessage(RED, 1, "Cannot create a temporaly BIN file.");
    return false;
  }

  ui.drawDialogMessage(BLUE, 1, "Downloading log data...");
  {
    if (!logger.downloadLog(&binFileW, &progressCallback)) {
      logger.disconnect();
      binFileW.close();

      ui.drawDialogMessage(RED, 1, "Downloading log data... failed.");
      ui.drawDialogMessage(RED, 2, "- Keep your logger close to this device");
      ui.drawDialogMessage(RED, 3, "- Power cycling may fix this problem");
      return false;
    }

    // disconnect from the logger and close the downloaded file
    logger.disconnect();
    binFileW.close();
  }
  ui.drawDialogMessage(BLACK, 1, "Downloading log data... done.");

  // open the downloaded binary file for reading and
  // create a new GPS file for writing
  File32 binFileR = SDcard.open(TEMP_BIN, (O_READ));
  File32 gpxFileW = SDcard.open(TEMP_GPX, (O_CREAT | O_WRITE | O_TRUNC));
  if ((!binFileR) || (!gpxFileW)) {  // BIN or GPX file open failed
    if (binFileR) binFileR.close();
    if (gpxFileW) gpxFileW.close();

    // print the error message
    ui.drawDialogMessage(RED, 2, "Cannot open the temporaly BIN file");
    ui.drawDialogMessage(RED, 3, "or create a GPX file for output.");
    return false;
  }

  ui.drawDialogMessage(BLUE, 2, "Converting data to GPX format...");
  {
    parseopt_t parseopt = {cfg.trackMode, TIME_OFFSET_VALUES[cfg.timeOffsetIdx]};

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
      ui.drawDialogMessage(BLACK, 2, "Converting data to GPX format... done.");
      ui.drawDialogMessage(BLUE, 3, outputstr);
      ui.drawDialogMessage(BLUE, 4, summarystr);
    } else {
      // remove the GPX file that has no valid record
      SD.remove(TEMP_GPX);

      // print the result message
      ui.drawDialogMessage(BLACK, 2, "Converting data to GPX format... done.");
      ui.drawDialogMessage(BLUE, 3, "No output file is saved because of there is");
      ui.drawDialogMessage(BLUE, 4, "no valid record in the log data.");
    }
  }

  return true;
}

/**
 *
 */
void onDownloadLogSelect(menuitem_t *item) {
  runDownloadLog();
  ui.waitForInputOk(IDLE_SHUTDOWN);
}

bool runFixRTCtime() {
  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Fix RTC time");
  ui.drawNavBar(NULL);

  // print the progress message
  ui.drawDialogMessage(BLUE, 0, "Connecting to GPS logger...");
  {
    // connect to the logger
    if (!logger.connect(cfg.loggerAddr)) {
      ui.drawDialogMessage(RED, 0, "Connecting to GPS logger... failed.");
      ui.drawDialogMessage(RED, 1, "- Make sure Bluetooth is enabled on your");
      ui.drawDialogMessage(RED, 2, "  GPS logger");
      ui.drawDialogMessage(RED, 3, "- Power cycling may fix this problem");

      return false;
    }
  }
  ui.drawDialogMessage(BLACK, 0, "Connecting to GPS logger... done.");

  ui.drawDialogMessage(BLUE, 1, "Setting RTC datetime...");
  {
    if (!logger.fixRTCdatetime()) {
      logger.disconnect();

      ui.drawDialogMessage(RED, 1, "Setting RTC datetime... failed.");
      ui.drawDialogMessage(RED, 2, "- Keep GPS logger close to this device");

      return false;
    }

    logger.disconnect();
  }
  // print the result message
  ui.drawDialogMessage(BLACK, 1, "Setting RTC datetime... done.");
  ui.drawDialogMessage(BLUE, 2, "The logger has been restarted to apply the fix.");
  ui.drawDialogMessage(BLUE, 3, "Please check the logging status.");

  return true;
}

/**
 *
 */
void onFixRTCtimeSelect(menuitem_t *item) {
  runFixRTCtime();
  ui.waitForInputOk(IDLE_SHUTDOWN);
}

/**
 *
 */
void onShowLocationSelect(menuitem_t *item) {
  Serial.printf("[DEBUG] onSetPresetConfigMenuSelected\n");
}

bool runPairWithLogger() {
  const String DEVICE_NAMES[] = {"747PRO GPS", "HOLUX_M-241"};
  const int8_t DEVICE_COUNT = 2;

  // draw a message dialog frame and clear the navigation bar
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
      uint8_t addr[BT_ADDR_LEN];
      memcpy(addr, app.loggerAddr, BT_ADDR_LEN);

      // print the success message
      sprintf(msgbuf1, "- %s : found.", DEVICE_NAMES[i].c_str());
      sprintf(msgbuf2, "Logger address : %02X%02X-%02X%02X-%02X%02X", addr[0], addr[1], addr[2],
              addr[3], addr[4], addr[5]);
      ui.drawDialogMessage(BLACK, (1 + i), msgbuf1);
      ui.drawDialogMessage(BLUE, (2 + i), "Successfully paired with the discovered logger.");
      ui.drawDialogMessage(BLUE, (3 + i), msgbuf2);

      // copy the address and model name to the configuration
      memcpy(cfg.loggerAddr, addr, BT_ADDR_LEN);
      strncpy(cfg.loggerName, DEVICE_NAMES[i].c_str(), (DEV_NAME_LEN-1));
      saveAppConfig();

      updateAppHint();
      ui.drawTitleBar(app.sdcAvail, app.sppActive);

      return true;
    } else {  // failed to connect to the device
      sprintf(msgbuf1, "- %s : not found.", DEVICE_NAMES[i].c_str());
      ui.drawDialogMessage(BLACK, (1 + i), msgbuf1);
      continue;
    }
  }

  // print the failure message
  ui.drawDialogMessage(RED, (1 + DEVICE_COUNT), "Cannot discover any supported logger.");
  ui.drawDialogMessage(RED, (2 + DEVICE_COUNT), "- Make sure Bluetooth is enabled on your");
  ui.drawDialogMessage(RED, (3 + DEVICE_COUNT), "  GPS logger");
  ui.drawDialogMessage(RED, (4 + DEVICE_COUNT), "- Power cycling may fix this problem");

  return false;
}

/**
 *
 */
void onPairWithLoggerSelect(menuitem_t *item) {
  runPairWithLogger();
  ui.waitForInputOk(IDLE_SHUTDOWN);
}

bool runClearFlash() {
  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Erase Log");
  ui.drawNavBar(NULL);

  // prompt the user to confirm the operation
  ui.drawDialogMessage(BLUE, 0, "Are you sure to erase log data?");
  {
    if (ui.waitForInputOkCancel(IDLE_SHUTDOWN) == BID_CANCEL) {
      ui.drawDialogMessage(BLACK, 0, "Are you sure to erase log data?  [Cancel]");
      ui.drawDialogMessage(RED, 1, "The operation was canceled by the user.");
      return false;
    }
  }
  ui.drawDialogMessage(BLACK, 0, "Are you sure to erase log data? [ OK ]");
  ui.drawNavBar(NULL);

  // print the progress message
  ui.drawDialogMessage(BLUE, 1, "Connecting to GPS logger...");
  {
    // connect to the logger
    if (!logger.connect(cfg.loggerAddr)) {
      // if failed, print the error message and hint(s)
      ui.drawDialogMessage(RED, 1, "Connecting to GPS logger... failed.");
      ui.drawDialogMessage(RED, 2, "- Make sure Bluetooth is enabled on your");
      ui.drawDialogMessage(RED, 3, "  GPS logger");
      ui.drawDialogMessage(RED, 4, "- Power cycling may fix this problem");
      return false;
    }
  }
  ui.drawDialogMessage(BLACK, 1, "Connecting to GPS logger... done.");

  ui.drawDialogMessage(BLUE, 2, "Erasing log data...");
  {
    // erase the log data
    if (!logger.clearFlash(&progressCallback)) {
      logger.disconnect();

      ui.drawDialogMessage(RED, 2, "Erasing log data... failed.");
      ui.drawDialogMessage(RED, 3, "- Keep GPS logger close to this device");
      ui.drawDialogMessage(RED, 4, "- Power cycling may fix this problem");
    }
    logger.disconnect();

    return false;
  }
  ui.drawDialogMessage(BLACK, 2, "Erasing log data... done.");
  ui.drawDialogMessage(BLUE, 3, "Hope you have a nice trip next time!");

  return true;
}

void onClearFlashSelect(menuitem_t *item) {
  runClearFlash();
  ui.waitForInputOk(IDLE_SHUTDOWN);
}

void onAppSettingSelect(menuitem_t *item) {
  int8_t itemCount = (sizeof(cfgMain) / sizeof(cfgitem_t));
  ui.enterConfigMenu("Settings", cfgMain, itemCount, IDLE_SHUTDOWN);

  saveAppConfig();
}

void onTrackModeSelect(cfgitem_t *item) {
  cfg.trackMode = (trackmode_t)(((int)cfg.trackMode + 1) % 3);
}

void onTrackModeUpdate(cfgitem_t *item) {
  if (cfg.trackMode == TRK_ONE_DAY) {
    strcpy(item->valueDescr, "A track per day");
  } else if (cfg.trackMode == TRK_AS_IS) {
    strcpy(item->valueDescr, "As log recorded");
  } else {
    strcpy(item->valueDescr, "One track");
  }
}

void onTimezoneSelect(cfgitem_t *item) {
  uint8_t valCount = sizeof(TIME_OFFSET_VALUES) / sizeof(float);
  cfg.timeOffsetIdx = (cfg.timeOffsetIdx + 1) % valCount;
}

void onTimezoneUpdate(cfgitem_t *item) {
  sprintf(item->valueDescr, "UTC%+.1f", TIME_OFFSET_VALUES[cfg.timeOffsetIdx]);
}

void onPutWayptSelect(cfgitem_t *item) {
  cfg.putWaypt = (!cfg.putWaypt);
}

void onPutWayptUpdate(cfgitem_t *item) {
  setValueDescrByBool(cfg.putWaypt, item->valueDescr);
}

void onLogByDistanceSelect(cfgitem_t *item) {
  uint8_t valCount = sizeof(LOG_DIST_VALUES) / sizeof(int16_t);

  cfg.logDistIdx = max(1, (cfg.logDistIdx + 1) % valCount);
  cfg.logTimeIdx = 0;
  cfg.logSpeedIdx = 0;

  cfgLogMode[2].updateValueDescr(&cfgLogMode[2]); // update the logByTime menu
  cfgLogMode[3].updateValueDescr(&cfgLogMode[3]); // update the logBySpeed menu
}

void onLogByDistanceUpdate(cfgitem_t *item) {
  if (LOG_DIST_VALUES[cfg.logDistIdx] == 0) {
    strcpy(item->valueDescr, "Disabled");
  } else {
    sprintf(item->valueDescr, "%d meters", LOG_DIST_VALUES[cfg.logDistIdx]);
  }
}

void onLogByTimeSelect(cfgitem_t *item) {
  uint8_t valCount = sizeof(LOG_TIME_VALUES) / sizeof(int16_t);

  cfg.logDistIdx = 0;
  cfg.logTimeIdx = max(1, (cfg.logTimeIdx + 1) % valCount);
  cfg.logSpeedIdx = 0;

  cfgLogMode[1].updateValueDescr(&cfgLogMode[1]); // update the logByDist menu
  cfgLogMode[3].updateValueDescr(&cfgLogMode[3]); // update the logBySpeed menu
}

void onLogByTimeUpdate(cfgitem_t *item) {
  if (LOG_TIME_VALUES[cfg.logTimeIdx] == 0) {
    strcpy(item->valueDescr, "Disabled");
  } else {
    sprintf(item->valueDescr, "%d seconds", LOG_TIME_VALUES[cfg.logTimeIdx]);
  }
}

void onLogBySpeedSelect(cfgitem_t *item) {
  uint8_t valCount = sizeof(LOG_SPEED_VALUES) / sizeof(int16_t);

  cfg.logDistIdx = 0;
  cfg.logTimeIdx = 0;
  cfg.logSpeedIdx = max(1, (cfg.logSpeedIdx + 1) % valCount);
  
  cfgLogMode[1].updateValueDescr(&cfgLogMode[1]); // update the logByDist menu
  cfgLogMode[2].updateValueDescr(&cfgLogMode[2]); // update the logByTime menu
}

void onLogBySpeedUpdate(cfgitem_t *item) {
  if (LOG_SPEED_VALUES[cfg.logSpeedIdx] == 0) {
    strcpy(item->valueDescr, "Disabled");
  } else {
    sprintf(item->valueDescr, "%d km/h", LOG_SPEED_VALUES[cfg.logSpeedIdx]);
  }
}

void onLogFullActionSelect(cfgitem_t *item) {
  cfg.logFullStop = (!cfg.logFullStop);
}

void onLogFullActionUpdate(cfgitem_t *item) {
  if (!cfg.logFullStop) {
    strcpy(item->valueDescr, "Stop logging");
  } else {
    strcpy(item->valueDescr, "Overwrite");
  }
}

void onLoadDefaultFormatSelect(cfgitem_t *item) {
  cfg.logFormat = DEFAULT_LOG_FORMAT;

  for (int8_t i = 0; i < sizeof(cfgLogFormat) / sizeof(cfgitem_t); i++) {
    cfgitem_t *ci = &cfgLogFormat[i];
    if (ci->updateValueDescr != NULL) ci->updateValueDescr(ci);
  }
}

void onLoadDefaultFormatUpdate(cfgitem_t *item) {
  sprintf(item->valueDescr, "0x%08X", cfg.logFormat);
}

void onRecordRCRSelect(cfgitem_t *item) {
  cfg.logFormat ^= REG_RCR;
  onLoadDefaultFormatUpdate(&cfgLogFormat[1]);
}

void onRecordRCRUpdate(cfgitem_t *item) {
  setValueDescrByBool((cfg.logFormat & REG_RCR), item->valueDescr);
}

void onRecordValidSelect(cfgitem_t *item) {
  cfg.logFormat ^= REG_VALID;
  onLoadDefaultFormatUpdate(&cfgLogFormat[1]);
}

void onRecordValidUpdate(cfgitem_t *item) {
  setValueDescrByBool((cfg.logFormat & REG_VALID), item->valueDescr);
}

void onRecordSpeedSelect(cfgitem_t *item) {
  cfg.logFormat ^= REG_SPEED;
  onLoadDefaultFormatUpdate(&cfgLogFormat[1]);
}

void onRecordSpeedUpdate(cfgitem_t *item) {
  setValueDescrByBool((cfg.logFormat & REG_SPEED), item->valueDescr);
}

void onRecordAltitudeSelect(cfgitem_t *item) {
  cfg.logFormat ^= REG_ELE;
  onLoadDefaultFormatUpdate(&cfgLogFormat[1]);
}

void onRecordAltitudeUpdate(cfgitem_t *item) {
  setValueDescrByBool((cfg.logFormat & REG_ELE), item->valueDescr);
}

void onRecordHeadingSelect(cfgitem_t *item) {
  cfg.logFormat ^= REG_HEAD;
  onLoadDefaultFormatUpdate(&cfgLogFormat[1]);
}

void onRecordHeadingUpdate(cfgitem_t *item) {
  setValueDescrByBool((cfg.logFormat & REG_HEAD), item->valueDescr);
}

void onRecordDistanceSelect(cfgitem_t *item) {
  cfg.logFormat ^= REG_DIST;
  onLoadDefaultFormatUpdate(&cfgLogFormat[1]);
}

void onRecordDistanceUpdate(cfgitem_t *item) {
  setValueDescrByBool((cfg.logFormat & REG_DIST), item->valueDescr);
}

void onRecordDGPSSelect(cfgitem_t *item) {
    // nothing to do (always disabled)
}

void onRecordDOPSelect(cfgitem_t *item) {
  // nothing to do (always disabled)
}

void onRecordSatInViewSelect(cfgitem_t *item) {
  // nothing to do (always disabled)
}

void onOutputSubMenuSelect(cfgitem_t *item) {
  // enter the output configuration sub-menu
  int8_t itemCount = (sizeof(cfgOutput) / sizeof(cfgitem_t));
  ui.enterConfigMenu("Settings > Output", cfgOutput, itemCount, IDLE_SHUTDOWN);

  // return to the parent menu
}

void onLogModeSubMenuSelect(cfgitem_t *item) {
  // enter the log mode configuration sub-menu
  int8_t itemCount = (sizeof(cfgLogMode) / sizeof(cfgitem_t));
  ui.enterConfigMenu("Settings > Log Mode", cfgLogMode, itemCount, IDLE_SHUTDOWN);
  
  // return to the parent menu
}

void onLogFormatSubMenuSelect(cfgitem_t *item) {
  // enter the log format configuration sub-menu
  int8_t itemCount = (sizeof(cfgLogFormat) / sizeof(cfgitem_t));
  ui.enterConfigMenu("Settings > Log Format", cfgLogFormat, itemCount, IDLE_SHUTDOWN);

  // return to the parent menu
}

void saveAppConfig() {
  uint8_t *pcfg = (uint8_t *)(&cfg);
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
  uint8_t *pcfg = (uint8_t *)(&cfg);
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
      ((cfg.length != sizeof(appconfig_t)) || (EEPROM.read(sizeof(appconfig_t) != chk)));
  if (loadDefault) {
    memset(&cfg, 0, sizeof(appconfig_t));
    cfg.length = sizeof(appconfig_t);
    cfg.trackMode = DEFAULT_TRACK_MODE;
    cfg.logFormat = DEFAULT_LOG_FORMAT;
    cfg.timeOffsetIdx = DEFAULT_TIME_OFFSET_IDX;
    cfg.logDistIdx = DEFAULT_LOG_DIST_IDX;
    cfg.logTimeIdx = DEFAULT_LOG_TIME_IDX;
    cfg.logSpeedIdx = DEFAULT_LOG_DIST_IDX;

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
  memset(&cfg, 0, sizeof(cfg));

  // initialize the M5Stack
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setBrightness(LCD_BRIGHTNESS);
  EEPROM.begin(sizeof(appconfig_t) + 1);
  SDcard.begin(GPIO_NUM_4, SD_ACCESS_SPEED);

  logger.setEventCallback(bluetoothCallback);
  app.sdcAvail = (SDcard.card()->sectorCount() > 0);

  // load the configuration data
  M5.update();
  bool defaultConfig = (M5.BtnA.read() == 1);
  bool firstBoot = (loadAppConfig(defaultConfig) == false);

  // draw the screen
  updateAppHint();
  ui.setAppTitle(APP_NAME);
  ui.drawTitleBar(app.sdcAvail, app.sppActive);
 
  // if left button is pressed, clear the configuration
  if (firstBoot) {
    ui.drawDialogFrame("Hello :)");
    ui.drawDialogMessage(BLUE, 0, "Thank you for using SmallStep!");
    ui.drawDialogMessage(BLUE, 1, "This is a data downloading utility for classic");
    ui.drawDialogMessage(BLUE, 2, "MTK GPS loggers.");
    ui.drawDialogMessage(BLACK, 3, "Please pair with your logger and configure the");
    ui.drawDialogMessage(BLACK, 4, "app settings before use. In particular, you");
    ui.drawDialogMessage(BLACK, 5, "WOULD need to set your timezone offset in the");
    ui.drawDialogMessage(BLACK, 6, "output settings menu.");
    ui.waitForInputOk(IDLE_SHUTDOWN);
  }

  // enter the main menu (infinit loop in this function)
  int8_t itemCount = (sizeof(menuMain) / sizeof(menuitem_t));
  ui.enterMainMenu(menuMain, itemCount, IDLE_SHUTDOWN);
}

void loop() {
  // *** NEVER REACH HERE ***
  // power off the device if reaching here
  SDcard.end();         // stop SD card access
  M5.Power.powerOFF();  // power off
}
