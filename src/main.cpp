#include <M5Stack.h>
#include <SdFat.h>
#include <SimpleBeep.h>

#include "AppUI.h"
#include "MtkLogger.h"
#include "MtkParser.h"
#include "Resources.h"

/* ################ Important Notice ################ */
/* You MUST increase RX_QUEUE_SIZE 512 to 4094 in BluetoothSerial.h. */
/* Otherwise, the BluetoothSerial library will not work properly. */
/* ################################################## */

#define SD_ACCESS_SPEED 12000000  // settins 20MHz should cause SD card error
#define BT_ADDR_LEN 6
#define DEV_NAME_LEN 20

#define BEEP_VOLUME 1
#define BEEP_FREQ_SUCCESS 4186  // C8
#define BEEP_FREQ_FAILURE 131   // C3
#define BEEP_DURATION_SHORT 120
#define BEEP_DURATION_LONG 600

typedef struct _appconfig {
  uint16_t length;                  // must be sizeof(appconfig_t)
  uint8_t loggerAddr[BT_ADDR_LEN];  // app / address of paired logger
  char loggerName[DEV_NAME_LEN];    // app / name of pairded logger
  bool playBeep;                    // app / play beep sound
  trackmode_t trackMode;            // parser / how to divide/put tracks
  uint8_t timeOffsetIdx;            // parser / timezone offset in hours
  bool putWaypt;                    // parser / treat points recorded by button as WPTs
  uint8_t logDistIdx;               // log mode / auto log by distance (meter, 0: disable)
  uint8_t logTimeIdx;               // log mode / auto log by time (seconds, 0: disable)
  uint8_t logSpeedIdx;              // log mode / auto log by speed (meter/sec, 0:disabled)
  bool logFullStop;                 // log mode / stop logging when flash is full
  uint32_t logFormat;               // log format / fields to be recorded
} appconfig_t;

// ******** function prototypes ********
bool isZeroedBytes(void *, uint16_t);
bool isLoggerPaired();
bool isSDcardAvailable();
bool isSameBytes(const void *, const void *, uint16_t);
void saveAppConfig();
bool loadAppConfig(bool);
void playBeep(bool success, uint16_t duration);
void bluetoothCallback(esp_spp_cb_event_t, esp_spp_cb_param_t *);
void progressCallback(int32_t, int32_t);
void setValueDescrByBool(char *, bool);
bool runDownloadLog();
bool runFixRTCtime();
bool runPairWithLogger();
bool runClearFlash();
bool runSetLogFormat();
bool runSetLogMode();
void onAppInputIdle();
void onDownloadLogSelect(menuitem_t *);
void onFixRTCtimeSelect(menuitem_t *);
void onShowLocationSelect(menuitem_t *);
void onPairWithLoggerSelect(menuitem_t *);
void onClearFlashSelect(menuitem_t *);
void onSetLogFormatSelect(menuitem_t *);
void onSetLogModeSelect(menuitem_t *);
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
void onRecordMillisSelect(cfgitem_t *);
void onRecordMillisUpdate(cfgitem_t *);
void onRecordSpeedSelect(cfgitem_t *);
void onRecordSpeedUpdate(cfgitem_t *);
void onRecordAltitudeSelect(cfgitem_t *);
void onRecordAltitudeUpdate(cfgitem_t *);
void onRecordHeadingSelect(cfgitem_t *);
void onRecordHeadingUpdate(cfgitem_t *);
void onRecordDistanceSelect(cfgitem_t *);
void onRecordDistanceUpdate(cfgitem_t *);
void onRecordDgpsSelect(cfgitem_t *);
void onRecordDgpsUpdate(cfgitem_t *);
void onRecordDopSelect(cfgitem_t *);
void onRecordDopUpdate(cfgitem_t *);
void onRecordSatSelect(cfgitem_t *);
void onRecordSatUpdate(cfgitem_t *);
void onPairWithLoggerCfgSelect(cfgitem_t *);
void onPairWithLoggerCfgUpdate(cfgitem_t *);
void onOutputSubMenuSelect(cfgitem_t *);
void onLogModeSubMenuSelect(cfgitem_t *);
void onLogFormatSubMenuSelect(cfgitem_t *);
void onEnableBeepCfgSelect(cfgitem_t *);
void onEnableBeepCfgUpdate(cfgitem_t *);
void onPerformFormatSelect(cfgitem_t *);
void onClearSettingsSelect(cfgitem_t *);
void updateAppHint();

const char *APP_NAME = "SmallStep";
const char LCD_BRIGHTNESS = 10;
const uint32_t IDLE_TIMEOUT = 120000;

const float TIME_OFFSET_VALUES[] = {
    -12, -11, -10, -9, -8,  -7, -6,  -5, -4.5, -4, -3.5, -3, -2,  -1, 0,  1,
    2,   3,   3.5, 4,  4.5, 5,  5.5, 6,  6.5,  7,  8,    9,  9.5, 10, 12, 13};                 // uint: hours
const int16_t LOG_DIST_VALUES[] = {0, 100, 300, 500, 1000, 2000, 3000, 5000};                  // uint: .1 meters
const int16_t LOG_TIME_VALUES[] = {0, 10, 30, 50, 100, 150, 200, 300, 600, 1200, 1800, 3000};  // unit: .1 seconds
const int16_t LOG_SPEED_VALUES[] = {0,   100,  200,  300,  400,  500,  600,  700,  800,        // uint: .1 km/h
                                    900, 1000, 1200, 1400, 1600, 1800, 2000, 2500, 3000};
const appconfig_t DEFAULT_CONFIG = {
    sizeof(appconfig_t),                                                             // length
    {0, 0, 0, 0, 0, 0},                                                              // loggerAddr
    "NO LOGGER",                                                                     // loggerName
    true,                                                                            // playBeep
    TRK_ONE_DAY,                                                                     // trackMode
    14,                                                                              // timeOffsetIdx (14: UTC+0)
    true,                                                                            // putWaypt
    0,                                                                               // logDistIdx (0: disabled)
    4,                                                                               // logTimeIdx (7: 10 seconds)
    0,                                                                               // logSpeedIdx (0: disabled)
    true,                                                                            // logFullStop
    (FMT_FIXONLY | FMT_TIME | FMT_LON | FMT_LAT | FMT_HEIGHT | FMT_SPEED | FMT_RCR)  // logFormat
};

menuitem_t menuMain[] = {
    // {caption, iconData, enabled, onSelect}
    {"Download Log", ICON_DOWNLOAD_LOG, true, &onDownloadLogSelect},
    {"Fix RTC time", ICON_FIX_RTC, true, &onFixRTCtimeSelect},
    {"Erase Log Data", ICON_ERASE_LOG, true, &onClearFlashSelect},
    {"Set Log Mode", ICON_LOG_MODE, true, &onSetLogModeSelect},
    {"Set Log Format", ICON_LOG_FORMAT, true, &onSetLogFormatSelect},
    //  {"Show Location", ICON_NAVIGATION, true, &onShowLocationSelect},
    {"Pair w/ Logger", ICON_PAIR_LOGGER, true, &onPairWithLoggerSelect},
    {"App Settings", ICON_APP_SETTINGS, true, &onAppSettingSelect},
};

cfgitem_t cfgOutput[] = {
    // {caption, hintText, valueDescr, onSelectItem, onDescrUpdate}
    {"Back", "Exit this menu", "<<", true, NULL, NULL},
    {"Track mode", "How to divide tracks in GPX file", "", true, &onTrackModeSelect, &onTrackModeUpdate},
    {"Timezone offset", "UTC offset for 'a track per day' mode", "", true, &onTimezoneSelect, &onTimezoneUpdate},
    {"Put WAYPTs", "Treat manulally recorded points as WAYPTs (POIs)", "", true, &onPutWayptSelect, &onPutWayptUpdate},
};

cfgitem_t cfgLogMode[] = {
    // {caption, descr, valueDescr, onSelect, valueDescrUpdate}
    {"Back", "Exit this menu", "<<", true, NULL, NULL},
    {"Log by distance", "Auto log by moving distance", "", true, &onLogByDistanceSelect, &onLogByDistanceUpdate},
    {"Log by time", "Auto log by elapsed time", "", true, &onLogByTimeSelect, &onLogByTimeUpdate},
    {"Log by speed", "Auto log by exceeded speed", "", true, &onLogBySpeedSelect, &onLogBySpeedUpdate},
    {"Log full action", "Behavior when the logger flash is full", "", true, &onLogFullActionSelect,
     &onLogFullActionUpdate},
};

cfgitem_t *cfgLogByDist = &cfgLogMode[1];
cfgitem_t *cfgLogByTime = &cfgLogMode[2];
cfgitem_t *cfgLogBySpd = &cfgLogMode[3];

cfgitem_t cfgLogFormat[] = {
    // {caption, descr, valueDescr, onSelect, valueDescrUpdate}
    {"Back", "Exit this menu", "<<", true, NULL, NULL},
    {"Load defaults", "Reset to the default format", "", true, &onLoadDefaultFormatSelect, &onLoadDefaultFormatUpdate},
    {"TIME (required fields)", "Date and time data in seconds", "Enabled", false, NULL, NULL},
    {"LAT, LON (required fields)", "Latitude and longitude data", "Enabled", false, NULL, NULL},
    {"SPEED", "Moving speed data", "", true, &onRecordSpeedSelect, &onRecordSpeedUpdate},
    {"ALT", "Altitude data", "", true, &onRecordAltitudeSelect, &onRecordAltitudeUpdate},
    {"RCR", "Record reason (needed to put WAYPTs)", "", true, &onRecordRCRSelect, &onRecordRCRUpdate},
    {"------", "The fields below are not used by SmallStep", "", false, NULL, NULL},
    {"TRACK", "Track angle data", "", true, &onRecordHeadingSelect, &onRecordHeadingUpdate},
    {"VALID", "Positioning status data (valid/invalid)", "", true, &onRecordValidSelect, &onRecordValidUpdate},
    {"DIST", "Moving distance data", "", true, &onRecordDistanceSelect, &onRecordDistanceUpdate},
    {"MSEC", "Time data in millisecond", "", true, &onRecordMillisSelect, &onRecordMillisUpdate},
    {"DSTA, DAGE", "Differencial GPS data", "Disabled", true, &onRecordDgpsSelect, &onRecordDgpsUpdate},
    {"PDOP, HDOP, VDOP", "Dilution of precision data", "Disabled", true, &onRecordDopSelect, &onRecordDopUpdate},
    {"NSAT, ELE, AZI, SNR", "Satellite data", "Disabled", true, &onRecordSatSelect, &onRecordSatUpdate},
};

cfgitem_t *cfgResetFormat = &cfgLogFormat[1];

cfgitem_t cfgMain[] = {
    // {caption, descr, valueDescr, onSelect, valueDescrUpdate}
    {"Save and exit", "Return to the main menu", "", true, NULL, NULL},
    {"Pairing with a GPS logger", "Discover supported GPS loggers", ">>", true, &onPairWithLoggerCfgSelect,
     &onPairWithLoggerCfgUpdate},
    {"Output settings", "GPX log output options", ">>", true, &onOutputSubMenuSelect, NULL},
    {"Log mode settings", "Logging behavior of the logger", ">>", true, &onLogModeSubMenuSelect, NULL},
    {"Log format settings", "Contents to be stored on the logger", ">>", true, &onLogFormatSubMenuSelect, NULL},
    {"Beep sound", "Play beep sound when a task is finished", ">>", true, &onEnableBeepCfgSelect,
     &onEnableBeepCfgUpdate},
    {"------", "", "", false, NULL, NULL},
    {"Format SD card", "Format inserted SD card", "", true, &onPerformFormatSelect, NULL},
    {"Clear all settings", "Erase the current settings of SmallStep", "", true, &onClearSettingsSelect, NULL},
};

AppUI ui = AppUI();
SdFat SDcard;
MtkLogger logger = MtkLogger(APP_NAME);
appconfig_t cfg;
uint32_t idleTimer;

void updateAppHint() {
  // set the Paied logger name and address as the application hint
  char addrStr[16];
  sprintf(addrStr, "%02X%02X-%02X%02X-%02X%02X",  // xxxx-xxxx-xxxx
          cfg.loggerAddr[0], cfg.loggerAddr[1], cfg.loggerAddr[2], cfg.loggerAddr[3], cfg.loggerAddr[4],
          cfg.loggerAddr[5]);
  ui.setAppHints(cfg.loggerName, addrStr);
}

void bluetoothCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  switch (event) {
  case ESP_SPP_INIT_EVT:  // SPP is started
    if (param == NULL) {
      ui.setBluetoothStatus(true);
      ui.drawTitleBar();  // redraw the title bar to change the bluetooth icon to blue
    }
    break;

  case ESP_SPP_OPEN_EVT:  // SPP connection is established
    memcpy(cfg.loggerAddr, param->open.rem_bda, BT_ADDR_LEN);
    break;

  case ESP_SPP_UNINIT_EVT:  // SPP is stopped
    if (param == NULL) {
      ui.setBluetoothStatus(false);
      ui.drawTitleBar();  // redraw the title bar to change the bluetooth icon to grey
    }
    break;
  }
}

void progressCallback(int32_t current, int32_t max) {
  ui.drawDialogProgress(current, max);
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
    ui.drawDialogMessage(BLUE, 1, "Pair with your GPS logger now?");

    if (ui.waitForInputOkCancel() == BID_OK) {
      ui.drawDialogMessage(BLACK, 1, "Pair with your GPS logger now? [ OK ]");
      paired = runPairWithLogger();

      if (paired) ui.waitForInputOk();
    } else {
      ui.drawDialogMessage(BLACK, 1, "Pair with your GPS logger now? [Cancel]");
      ui.drawDialogMessage(BLUE, 3, "The pairing operation is not performed.");
    }
  }

  return paired;
}

bool isSDcardAvailable() {
  if (SDcard.card()->sectorCount() == 0) {  // cannot get the sector count
    // print the error message
    ui.drawDialogFrame("SD Card Error");
    ui.drawDialogMessage(RED, 0, "SD card is not available.");
    ui.drawDialogMessage(RED, 1, "- SD card is corrupted or is not inserted");
    return false;
  }

  return true;
}

void setValueDescrByBool(char *descr, bool value) {
  if (value) {
    strcpy(descr, "Enabled");
  } else {
    strcpy(descr, "Disabled");
  }
}

bool connectLogger(uint8_t msgLine) {
  // connect to the logger
  ui.drawDialogMessage(BLUE, (msgLine + 0), "Connecting to GPS logger...");

  if (!logger.connect(cfg.loggerAddr)) {
    ui.drawDialogMessage(RED, (msgLine + 0), "Connecting to GPS logger... failed.");
    ui.drawDialogMessage(RED, (msgLine + 1), "- Make sure BT is enabled on the GPS logger");
    ui.drawDialogMessage(RED, (msgLine + 2), "- If this problem occurs repeatly, please re-");
    ui.drawDialogMessage(RED, (msgLine + 3), "  start the logger and SmallStep");
    return false;
  }

  ui.drawDialogMessage(BLACK, (msgLine), "Connecting to GPS logger... done.");

  return true;
}

bool runDownloadLog() {
  const char TEMP_BIN[] = "download.bin";
  const char TEMP_GPX[] = "download.gpx";
  const char FILENAME_FMT[] = "gps_%04d%02d%02d-%02d%02d%02d";

  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;
  if (!isSDcardAvailable()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Download Log");
  ui.drawNavBar(NULL);

  File32 binFile = SDcard.open(TEMP_BIN, (O_CREAT | O_RDWR));
  File32 gpxFile = SDcard.open(TEMP_GPX, (O_CREAT | O_RDWR | O_TRUNC));
  if ((!binFile) || (!gpxFile)) {
    if (binFile) binFile.close();
    if (gpxFile) gpxFile.close();

    ui.drawDialogMessage(RED, 0, "Could not open temporally files.");
    return false;
  }

  if (!connectLogger(0)) return false;

  ui.drawDialogMessage(BLUE, 1, "Downloading log data...");
  {
    if (!logger.downloadLogData(&binFile, &progressCallback)) {
      binFile.close();
      gpxFile.close();

      ui.drawDialogMessage(RED, 1, "Downloading log data... failed.");
      ui.drawDialogMessage(RED, 2, "- Keep your logger close to this device");
      ui.drawDialogMessage(RED, 3, "- Power cycling may fix this problem");
      return false;
    }

    // disconnect from the logger and close the downloaded data file
    logger.disconnect();
  }
  ui.drawDialogMessage(BLACK, 1, "Downloading log data... done.");

  ui.drawDialogMessage(BLUE, 2, "Converting data to GPX file...");
  {
    // convert the binary file to GPX file and get the summary
    MtkParser *parser = new MtkParser();
    parseopt_t parseopt = {cfg.trackMode, TIME_OFFSET_VALUES[cfg.timeOffsetIdx], cfg.putWaypt};
    parser->setOptions(parseopt);
    parser->convert(&binFile, &gpxFile, &progressCallback);
    uint32_t trackCnt = parser->getTrackCount();
    uint32_t trkptCnt = parser->getTrkptCount();
    uint32_t wayptCnt = parser->getWayptCount();
    time_t time1st = parser->getFirstTrkpt().time;
    delete parser;

    // close the GPX file before rename and rename to the output file name
    binFile.close();
    gpxFile.close();

    // make a unique name for the GPX file
    struct tm *ltime = localtime(&time1st);
    char baseName[24];
    char gpxName[32];
    char binName[32];
    sprintf(baseName, FILENAME_FMT,
            (ltime->tm_year + 1900),  // year (4 digits)
            (ltime->tm_mon + 1),      // month (1-12)
            ltime->tm_mday,           // day of month (1-31)
            ltime->tm_hour, ltime->tm_min, ltime->tm_sec);

    // 最初のtrkptの時刻を使ってGPXファイルとBINファイルの保存名を決める
    for (uint16_t i = 1; i <= 65535; i++) {
      sprintf(gpxName, "%s_%02d.gpx", baseName, i);
      sprintf(binName, "%s_%02d.bin", baseName, i);

      if (!SDcard.exists(gpxName)) break;
    }

    if (trkptCnt > 0) {
      SDcard.rename(TEMP_GPX, gpxName);

      // make the output message strings
      char outputstr[48], summarystr[48];
      sprintf(outputstr, "Output file : %s", gpxName);
      sprintf(summarystr, "Summary : %d TRKs, %d TRKPTs, %d WPTs", trackCnt, trkptCnt, wayptCnt);

      // print the result message
      ui.drawDialogMessage(BLACK, 2, "Converting data to GPX file... done.");
      ui.drawDialogMessage(BLUE, 3, outputstr);
      ui.drawDialogMessage(BLUE, 4, summarystr);
    } else {
      // print the result message
      ui.drawDialogMessage(BLACK, 2, "Converting data to GPX file... done.");
      ui.drawDialogMessage(BLUE, 3, "No output file is saved because of there is");
      ui.drawDialogMessage(BLUE, 4, "no valid record in the log data.");
    }
  }

  return true;
}

void onAppInputIdle() {
  Serial.printf("SmallStep.onAppInputIdle: idle shutdown\n");
  SDcard.end();
  M5.Power.powerOFF();
}

/**
 *
 */
void onDownloadLogSelect(menuitem_t *item) {
  bool result = runDownloadLog();
  logger.disconnect();

  playBeep(result, BEEP_DURATION_LONG);
  ui.waitForInputOk();
}

bool runFixRTCtime() {
  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Fix RTC time");
  ui.drawNavBar(NULL);

  // connect to the logger
  if (!connectLogger(0)) return false;

  ui.drawDialogMessage(BLUE, 1, "Setting RTC datetime...");
  {
    if (!logger.fixRTCdatetime()) {
      ui.drawDialogMessage(RED, 1, "Setting RTC datetime... failed.");
      ui.drawDialogMessage(RED, 2, "- Keep GPS logger close to this device");
      return false;
    }
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
  bool result = runFixRTCtime();
  logger.disconnect();

  playBeep(result, BEEP_DURATION_LONG);
  ui.waitForInputOk();
}

/**
 *
 */
void onShowLocationSelect(menuitem_t *item) {
}

bool runPairWithLogger() {
  const String DEVICE_NAMES[] = {"747PRO GPS", "HOLUX_M-241"};
  const int8_t DEVICE_COUNT = 2;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Pair with Logger");
  ui.drawNavBar(NULL);  // disable the navigation bar

  ui.drawDialogMessage(BLUE, 0, "Discovering GPS logger...");

  for (int i = 0; i < DEVICE_COUNT; i++) {
    // print the device name trying to connect
    char msgbuf1[48], msgbuf2[48];
    sprintf(msgbuf1, "- %s", DEVICE_NAMES[i].c_str());
    ui.drawDialogMessage(BLUE, (1 + i), msgbuf1);

    if (logger.connect(DEVICE_NAMES[i])) {  // successfully connected
      logger.disconnect();                  // disconnect

      // Note: bluetoothCallback is called when the device is connected
      // and the logger address is set to the configuration before running below code

      // set the logger model name to the configuration
      strncpy(cfg.loggerName, DEVICE_NAMES[i].c_str(), (DEV_NAME_LEN - 1));

      // print the success message
      sprintf(msgbuf1, "- %s : found.", DEVICE_NAMES[i].c_str());
      sprintf(msgbuf2, "Logger address : %02X%02X-%02X%02X-%02X%02X", cfg.loggerAddr[0], cfg.loggerAddr[1],
              cfg.loggerAddr[2], cfg.loggerAddr[3], cfg.loggerAddr[4], cfg.loggerAddr[5]);
      ui.drawDialogMessage(BLACK, 0, "Discovering GPS logger... done");
      ui.drawDialogMessage(BLACK, (1 + i), msgbuf1);
      ui.drawDialogMessage(BLUE, (2 + i), "Successfully paired with the discovered logger.");
      ui.drawDialogMessage(BLUE, (3 + i), msgbuf2);

      // update GUI
      updateAppHint();
      ui.drawTitleBar();

      // save the app configuration
      saveAppConfig();
      return true;
    } else {  // failed to connect to the device
      sprintf(msgbuf1, "- %s : not found.", DEVICE_NAMES[i].c_str());
      ui.drawDialogMessage(BLACK, (1 + i), msgbuf1);
      continue;
    }
  }

  // print the failure message
  ui.drawDialogMessage(BLACK, 0, "Discovering GPS logger... failed.");
  ui.drawDialogMessage(RED, (1 + DEVICE_COUNT), "Cannot discover any supported logger.");
  ui.drawDialogMessage(RED, (2 + DEVICE_COUNT), "- If this problem occurs repeatly, please re-");
  ui.drawDialogMessage(RED, (3 + DEVICE_COUNT), "  start the logger and SmallStep");

  return false;
}

/**
 *
 */
void onPairWithLoggerSelect(menuitem_t *item) {
  bool result = runPairWithLogger();

  playBeep(result, BEEP_DURATION_LONG);
  ui.waitForInputOk();
}

bool runClearFlash() {
  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Erase Log");
  ui.drawNavBar(NULL);

  // prompt the user to confirm the operation
  ui.drawDialogMessage(BLUE, 0, "Are you sure to erase log data?");
  if (ui.waitForInputOkCancel() == BID_OK) {
    ui.drawDialogMessage(BLACK, 0, "Are you sure to erase log data? [ OK ]");
    ui.drawNavBar(NULL);
  } else {
    ui.drawDialogMessage(BLACK, 0, "Are you sure to erase log data?  [Cancel]");
    ui.drawDialogMessage(BLUE, 1, "The operation is not performed.");
    return false;
  }

  // connect to the logger
  if (!connectLogger(1)) return false;

  ui.drawDialogMessage(BLUE, 2, "Erasing log data...");
  if (logger.clearFlash(&progressCallback)) {
    ui.drawDialogMessage(BLACK, 2, "Erasing log data... done.");
  } else {
    ui.drawDialogMessage(RED, 2, "Erasing log data... timeout.");
    ui.drawDialogMessage(RED, 3, "Please retry the erase operation");
    return false;
  }

  // reload the logger and break
  ui.drawDialogMessage(BLUE, 3, "Reloading logger... ");

  if (logger.reloadDevice()) {
    ui.drawDialogMessage(BLACK, 3, "Reloading logger... done.");
    ui.drawDialogMessage(BLUE, 4, "Hope you have a nice trip next time :)");
  } else {
    ui.drawDialogMessage(RED, 3, "Reloading logger... failed.");
    ui.drawDialogMessage(RED, 4, "Please restart GPS logger manually");
    ui.drawDialogMessage(RED, 4, "before use it.");
    return false;
  }

  return true;
}

void onClearFlashSelect(menuitem_t *item) {
  bool result = runClearFlash();
  logger.disconnect();

  playBeep(result, BEEP_DURATION_LONG);
  ui.waitForInputOk();
}

bool runSetLogFormat() {
  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Set Log Format");
  ui.drawNavBar(NULL);

  // connect to the logger
  if (!connectLogger(0)) return false;

  uint32_t curLogFormat = 0;
  uint32_t newLogFormat = cfg.logFormat;

  ui.drawDialogMessage(BLUE, 1, "Updating the log format...");
  if (!logger.getLogFormat(&curLogFormat)) {
    ui.drawDialogMessage(RED, 1, "Updating the log format... failed");
    ui.drawDialogMessage(RED, 1, "Cannot get the current log format.");
    return false;
  }

  if (curLogFormat == newLogFormat) {
    char buf[40];
    sprintf(buf, "Log format : 0x%08X", newLogFormat);

    ui.drawDialogMessage(BLACK, 1, "Updating the log format... unchanged.");
    ui.drawDialogMessage(BLUE, 2, "The configured log format is already set.");
    ui.drawDialogMessage(BLUE, 3, buf);
    return true;
  }

  if (!logger.setLogFormat(newLogFormat)) {
    ui.drawDialogMessage(RED, 1, "Updating the log format... failed");
    ui.drawDialogMessage(RED, 2, "Failed to change the log format.");
    return false;
  }

  char buf[48];
  sprintf(buf, "from 0x%08X to 0x%08X.", curLogFormat, newLogFormat);
  ui.drawDialogMessage(BLACK, 1, "Updating the log format... done.");
  ui.drawDialogMessage(BLUE, 2, "The Log format on the logger is changed");
  ui.drawDialogMessage(BLUE, 3, buf);

  return true;
}

void onSetLogFormatSelect(menuitem_t *item) {
  bool result = runSetLogFormat();
  logger.disconnect();

  playBeep(result, BEEP_DURATION_LONG);
  ui.waitForInputOk();
}

bool runSetLogMode() {
  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Set Log Mode");
  ui.drawNavBar(NULL);

  // connect to the logger
  if (!connectLogger(0)) return false;

  recordmode_t curRecMode = MODE_FULLSTOP;
  logcriteria_t curLogCri = {0, 0, 0};
  recordmode_t newRecMode = (cfg.logFullStop) ? MODE_FULLSTOP : MODE_OVERWRITE;
  logcriteria_t newLogCri = {LOG_DIST_VALUES[cfg.logDistIdx], LOG_TIME_VALUES[cfg.logTimeIdx],
                             LOG_SPEED_VALUES[cfg.logSpeedIdx]};

  ui.drawDialogMessage(BLUE, 1, "Updating the log mode...");
  if ((!logger.getLogRecordMode(&curRecMode)) || (!logger.getLogCriteria(&curLogCri))) {
    ui.drawDialogMessage(RED, 1, "Updating the log mode... failed.");
    ui.drawDialogMessage(RED, 2, "Cannot get the current mode. Please retry.");
    return false;
  }

  char buf[2][40];
  if ((newLogCri.time % 10) == 0) {
    sprintf(buf[0], "Log by : %d meters, %d seconds, %d km/h", (newLogCri.distance / 10), (newLogCri.time / 10),
            (newLogCri.speed / 10));
  } else {
    sprintf(buf[0], "Log by : %d meters, %d.%d seconds, %d km/h", (newLogCri.distance / 10), (newLogCri.time / 10),
            (newLogCri.time % 10), (newLogCri.speed / 10));
  }
  if (newRecMode == MODE_FULLSTOP) {
    strcpy(buf[1], "Log full action : Stop logging");
  } else {
    strcpy(buf[1], "Log full action : Overwrap");
  }

  if ((curRecMode == newRecMode) && (isSameBytes(&curLogCri, &newLogCri, sizeof(logcriteria_t)))) {
    ui.drawDialogMessage(BLACK, 1, "Updating the log mode... unchanged.");
    ui.drawDialogMessage(BLUE, 2, "The configured log mode is already set.");
    ui.drawDialogMessage(BLUE, 3, buf[0]);
    ui.drawDialogMessage(BLUE, 4, buf[1]);
    return true;
  }

  if (!logger.setLogRecordMode(newRecMode) || !logger.setLogCriteria(newLogCri)) {
    ui.drawDialogMessage(RED, 1, "Updating the log mode... failed.");
    ui.drawDialogMessage(RED, 2, "Cannot set the new log mode. Please retry.");
    return false;
  }

  ui.drawDialogMessage(BLACK, 1, "Updating the log mode... done.");
  ui.drawDialogMessage(BLUE, 2, "The configured log mode is set successfully.");
  ui.drawDialogMessage(BLUE, 3, buf[0]);
  ui.drawDialogMessage(BLUE, 4, buf[1]);

  return true;
}

void onSetLogModeSelect(menuitem_t *item) {
  bool result = runSetLogMode();
  logger.disconnect();

  playBeep(result, BEEP_DURATION_LONG);
  ui.waitForInputOk();
}

void onAppSettingSelect(menuitem_t *item) {
  Serial.printf("SmallStep.onAppSettingSelect: open the app settings menu\n");

  appconfig_t oldcfg;
  memcpy(&oldcfg, &cfg, sizeof(appconfig_t));

  // open the configuration menu
  int8_t itemCount = (sizeof(cfgMain) / sizeof(cfgitem_t));
  ui.openConfigMenu("Settings", cfgMain, itemCount);

  // save the configuration if modified
  if (!isSameBytes(&oldcfg, &cfg, sizeof(appconfig_t))) {
    saveAppConfig();
  }
}

void onTrackModeSelect(cfgitem_t *item) {
  cfg.trackMode = (trackmode_t)(((int)cfg.trackMode + 1) % 3);
}

void onTrackModeUpdate(cfgitem_t *item) {
  if (cfg.trackMode == TRK_ONE_DAY) {
    strcpy(item->valueDescr, "A track per day");
  } else if (cfg.trackMode == TRK_AS_IS) {
    strcpy(item->valueDescr, "Split as recorded");
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
  setValueDescrByBool(item->valueDescr, cfg.putWaypt);
}

void onLogByDistanceSelect(cfgitem_t *item) {
  uint8_t valCount = sizeof(LOG_DIST_VALUES) / sizeof(int16_t);

  cfg.logDistIdx = max(1, (cfg.logDistIdx + 1) % valCount);
  cfg.logTimeIdx = 0;
  cfg.logSpeedIdx = 0;

  cfgLogByTime->onUpdateDescr(cfgLogByTime);  // update the logByTime menu
  cfgLogBySpd->onUpdateDescr(cfgLogBySpd);    // update the logBySpeed menu
}

void onLogByDistanceUpdate(cfgitem_t *item) {
  if (LOG_DIST_VALUES[cfg.logDistIdx] == 0) {
    strcpy(item->valueDescr, "Disabled");
  } else if ((LOG_DIST_VALUES[cfg.logDistIdx] % 10) == 0) {
    sprintf(item->valueDescr, "%d meters", (LOG_DIST_VALUES[cfg.logDistIdx] / 10));
  } else {
    sprintf(item->valueDescr, "%d.%d meters", (LOG_DIST_VALUES[cfg.logDistIdx] / 10),
            (LOG_DIST_VALUES[cfg.logDistIdx] % 10));
  }
}

void onLogByTimeSelect(cfgitem_t *item) {
  uint8_t valCount = sizeof(LOG_TIME_VALUES) / sizeof(int16_t);

  cfg.logDistIdx = 0;
  cfg.logTimeIdx = max(1, (cfg.logTimeIdx + 1) % valCount);
  cfg.logSpeedIdx = 0;

  cfgLogByDist->onUpdateDescr(cfgLogByDist);  // update the logByDist menu
  cfgLogBySpd->onUpdateDescr(cfgLogBySpd);    // update the logBySpeed menu
}

void onLogByTimeUpdate(cfgitem_t *item) {
  if (LOG_TIME_VALUES[cfg.logTimeIdx] == 0) {
    strcpy(item->valueDescr, "Disabled");
  } else if ((LOG_TIME_VALUES[cfg.logTimeIdx] % 10) == 0) {
    sprintf(item->valueDescr, "%d seconds", (LOG_TIME_VALUES[cfg.logTimeIdx] / 10));
  } else {
    sprintf(item->valueDescr, "%d.%d seconds", (LOG_TIME_VALUES[cfg.logTimeIdx] / 10),
            (LOG_TIME_VALUES[cfg.logTimeIdx] % 10));
  }
}

void onLogBySpeedSelect(cfgitem_t *item) {
  uint8_t valCount = sizeof(LOG_SPEED_VALUES) / sizeof(int16_t);

  cfg.logDistIdx = 0;
  cfg.logTimeIdx = 0;
  cfg.logSpeedIdx = max(1, (cfg.logSpeedIdx + 1) % valCount);

  cfgLogByDist->onUpdateDescr(cfgLogByDist);  // update the logByDist menu
  cfgLogByTime->onUpdateDescr(cfgLogByTime);  // update the logByTime menu
}

void onLogBySpeedUpdate(cfgitem_t *item) {
  if (LOG_SPEED_VALUES[cfg.logSpeedIdx] == 0) {
    strcpy(item->valueDescr, "Disabled");
  } else if ((LOG_SPEED_VALUES[cfg.logSpeedIdx] % 10) == 0) {
    sprintf(item->valueDescr, "%d km/h", (LOG_SPEED_VALUES[cfg.logSpeedIdx] / 10));
  } else {
    sprintf(item->valueDescr, "%d.%d km/h", (LOG_SPEED_VALUES[cfg.logSpeedIdx] / 10),
            (LOG_SPEED_VALUES[cfg.logSpeedIdx] % 10));
  }
}

void onLogFullActionSelect(cfgitem_t *item) {
  cfg.logFullStop = (!cfg.logFullStop);
}

void onLogFullActionUpdate(cfgitem_t *item) {
  if (cfg.logFullStop) {
    strcpy(item->valueDescr, "Stop logging");
  } else {
    strcpy(item->valueDescr, "Overwrap");
  }
}

void onLoadDefaultFormatSelect(cfgitem_t *item) {
  cfg.logFormat = DEFAULT_CONFIG.logFormat;

  for (int8_t i = 0; i < sizeof(cfgLogFormat) / sizeof(cfgitem_t); i++) {
    cfgitem_t *ci = &cfgLogFormat[i];
    if (ci->onUpdateDescr != NULL) ci->onUpdateDescr(ci);
  }
}

void onLoadDefaultFormatUpdate(cfgitem_t *item) {
  sprintf(item->valueDescr, "0x%08X", cfg.logFormat);
}

void onRecordRCRSelect(cfgitem_t *item) {
  cfg.logFormat ^= FMT_RCR;
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordRCRUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, (cfg.logFormat & FMT_RCR));
}

void onRecordValidSelect(cfgitem_t *item) {
  cfg.logFormat ^= FMT_VALID;
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordValidUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, (cfg.logFormat & FMT_VALID));
}

void onRecordMillisSelect(cfgitem_t *item) {
  cfg.logFormat ^= FMT_MSEC;
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordMillisUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, (cfg.logFormat & FMT_MSEC));
}

void onRecordSpeedSelect(cfgitem_t *item) {
  cfg.logFormat ^= FMT_SPEED;
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordSpeedUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, (cfg.logFormat & FMT_SPEED));
}

void onRecordAltitudeSelect(cfgitem_t *item) {
  cfg.logFormat ^= FMT_HEIGHT;
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordAltitudeUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, (cfg.logFormat & FMT_HEIGHT));
}

void onRecordHeadingSelect(cfgitem_t *item) {
  cfg.logFormat ^= FMT_TRACK;
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordHeadingUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, (cfg.logFormat & FMT_TRACK));
}

void onRecordDistanceSelect(cfgitem_t *item) {
  cfg.logFormat ^= FMT_DIST;
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordDistanceUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, (cfg.logFormat & FMT_DIST));
}

void onRecordDgpsSelect(cfgitem_t *item) {
  cfg.logFormat ^= FMT_DSTA;
  cfg.logFormat |= FMT_DAGE;
  if (cfg.logFormat & FMT_DSTA) cfg.logFormat ^= FMT_DAGE;

  onLoadDefaultFormatUpdate(cfgResetFormat);
};

void onRecordDgpsUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, (cfg.logFormat & FMT_DSTA));
};

void onRecordDopSelect(cfgitem_t *item) {
  cfg.logFormat ^= FMT_PDOP;
  cfg.logFormat |= (FMT_HDOP | FMT_VDOP);
  if (!(cfg.logFormat & FMT_PDOP)) cfg.logFormat ^= (FMT_HDOP | FMT_VDOP);

  onLoadDefaultFormatUpdate(cfgResetFormat);
};

void onRecordDopUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, (cfg.logFormat & FMT_PDOP));
};

void onRecordSatSelect(cfgitem_t *item) {
  cfg.logFormat ^= FMT_NSAT;
  cfg.logFormat |= (FMT_ELE | FMT_AZI | FMT_SNR);
  if (!(cfg.logFormat & FMT_NSAT)) cfg.logFormat ^= (FMT_ELE | FMT_AZI | FMT_SNR);

  onLoadDefaultFormatUpdate(cfgResetFormat);
};

void onRecordSatUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, (cfg.logFormat & FMT_NSAT));
};

void onPairWithLoggerCfgSelect(cfgitem_t *item) {
  runPairWithLogger();
  ui.waitForInputOk();
}

void onPairWithLoggerCfgUpdate(cfgitem_t *item) {
  if (isZeroedBytes(&cfg.loggerAddr, BT_ADDR_LEN)) {
    strcpy(item->valueDescr, "Not paired");
  } else {
    strcpy(item->valueDescr, cfg.loggerName);
  }
}

void onOutputSubMenuSelect(cfgitem_t *item) {
  // enter the output configuration sub-menu
  int8_t itemCount = (sizeof(cfgOutput) / sizeof(cfgitem_t));
  ui.openConfigMenu("Settings > Output", cfgOutput, itemCount);
}

void onLogModeSubMenuSelect(cfgitem_t *item) {
  // enter the log mode configuration sub-menu
  int8_t itemCount = (sizeof(cfgLogMode) / sizeof(cfgitem_t));
  ui.openConfigMenu("Settings > Log Mode", cfgLogMode, itemCount);
}

void onLogFormatSubMenuSelect(cfgitem_t *item) {
  // enter the log format configuration sub-menu
  int8_t itemCount = (sizeof(cfgLogFormat) / sizeof(cfgitem_t));
  ui.openConfigMenu("Settings > Log Format", cfgLogFormat, itemCount);
}

void onEnableBeepCfgSelect(cfgitem_t *item) {
  cfg.playBeep = (!cfg.playBeep);
}

void onEnableBeepCfgUpdate(cfgitem_t *item) {
  setValueDescrByBool(item->valueDescr, cfg.playBeep);
}

void onPerformFormatSelect(cfgitem_t *item) {
  ui.drawDialogFrame("Format SD card");
  ui.drawNavBar(NULL);

  ui.drawDialogMessage(BLUE, 0, "Are you sure to format the SD card?");
  ui.drawDialogMessage(RED, 1, "Note : All data on the SD card will be lost.");
  ui.drawDialogMessage(RED, 2, "  This operation cannot be undone.");

  if (ui.waitForInputOkCancel() == BID_OK) {
    ui.drawDialogMessage(BLACK, 0, "Are you sure to format the SD card? [ OK ]");
  } else {
    ui.drawDialogMessage(BLACK, 0, "Are you sure to format the SD card? [Cancel]");
    ui.drawDialogMessage(BLUE, 4, "The operation is not performed.");

    ui.waitForInputOk();
    return;
  }

  ui.drawDialogMessage(BLUE, 3, "Formatting the SD card...");
  if (SDcard.format()) {
    ui.drawDialogMessage(BLACK, 3, "Formatting the SD card... done.");
    ui.drawDialogMessage(BLUE, 4, "The SD card is formatted successfully.");
  } else {
    ui.drawDialogMessage(RED, 3, "Formatting the SD card... failed.");
    ui.drawDialogMessage(RED, 4, "Failed to format the SD card. Please retry.");
    return;
  }

  ui.waitForInputOk();

  SDcard.end();
  M5.Power.reset();
}

void onClearSettingsSelect(cfgitem_t *item) {
  ui.drawDialogFrame("Clear Settings");
  ui.drawDialogMessage(BLUE, 0, "Are you sure to clear all settings?");
  ui.drawDialogMessage(BLACK, 1, "Note : The log data on the paired GPS logger");
  ui.drawDialogMessage(BLACK, 2, "  and the files in the SD card will NOT be");
  ui.drawDialogMessage(BLACK, 3, "  deleted by this operation.");

  if (ui.waitForInputOkCancel() == BID_OK) {
    ui.drawDialogMessage(BLACK, 0, "Are you sure to clear all settings? [ OK ]");
    ui.drawDialogMessage(BLUE, 5, "Clear all settings...");
    delay(1000);

    // clear the configuration data
    cfg.length = 0;   // write the invalid length to the configuration
    saveAppConfig();  // save the configuration

    // stop the SD card access and reset the device
    SDcard.end();
    M5.Power.reset();
  } else {
    ui.drawDialogMessage(BLACK, 0, "Are you sure to clear all settings? [Cancel]");
    ui.drawDialogMessage(BLUE, 5, "The operation is not performed.");

    ui.waitForInputOk();
  }
}

bool isSameBytes(const void *data1, const void *data2, uint16_t size) {
  uint8_t *pdata1 = (uint8_t *)(data1);
  uint8_t *pdata2 = (uint8_t *)(data2);
  bool diff = false;

  for (uint16_t i = 0; i < size; i++) {
    diff |= (*pdata1 != *pdata2);
    pdata1 += 1;
    pdata2 += 1;
  }

  return diff;
}

void saveAppConfig() {
  uint8_t *pcfg = (uint8_t *)(&cfg);
  uint8_t chk = 0;

  // write the current configuration to EEPROM
  for (uint16_t i = 0; i < sizeof(appconfig_t); i++) {
    EEPROM.write(i, *pcfg);  // write the configuration data byte
    chk ^= *pcfg;            // calculate the checksum (XOR all bytes)
    pcfg += 1;
  }
  EEPROM.write(sizeof(appconfig_t), chk);  // checksum
  EEPROM.commit();

  Serial.printf("SmallStep.saveConfig: the configuration is saved to the EEPROM\n");
}

bool loadAppConfig(bool loadDefault) {
  uint8_t *pcfg = (uint8_t *)(&cfg);
  uint8_t chk = 0;

  Serial.printf("SmallStep.loadConfig: loading configuration data from the EEPROM\n");

  if (!loadDefault) {
    // read configuration data from EEPROM
    for (uint16_t i = 0; i < sizeof(appconfig_t); i++) {
      uint8_t b = EEPROM.read(i);
      *pcfg = b;
      chk ^= *pcfg;
      pcfg += 1;

      Serial.printf("%02X ", b);
    }
    Serial.printf("\n");
  }

  // load the default configuration if loadDefault is set to true or the EEPROM data is invalid
  // (the length field is wrong or the checksum is not matched)
  loadDefault |= ((cfg.length != sizeof(appconfig_t)) || (EEPROM.read(sizeof(appconfig_t) != chk)));
  if (loadDefault) {
    memcpy(&cfg, &DEFAULT_CONFIG, sizeof(appconfig_t));

    Serial.printf("SmallStep.loadConfig: the default configuration is loaded\n");

    return false;
  }

  Serial.printf("SmallStep.loadConfig: the EEPROM configuration is loaded\n");

  return true;
}

void playBeep(bool success, uint16_t duration) {
  uint16_t freq = (success) ? BEEP_FREQ_SUCCESS : BEEP_FREQ_FAILURE;
  if (cfg.playBeep) sb.beep(BEEP_VOLUME, freq, duration);
}

void setup() {
  // start a serial port
  Serial.begin(115200);

  // initialize the configuration variable
  memcpy(&cfg, &DEFAULT_CONFIG, sizeof(appconfig_t));

  // initialize a M5Stack
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setBrightness(LCD_BRIGHTNESS);
  M5.Lcd.fillScreen(BLACK);
  EEPROM.begin(sizeof(appconfig_t) + 1);
  SDcard.begin(GPIO_NUM_4, SD_ACCESS_SPEED);

  // initialize SimpleBeep;
  sb.init();

  // set the bluetooth event handler
  logger.setEventCallback(bluetoothCallback);

  // load the configuration data
  M5.update();
  bool defaultConfig = (M5.BtnA.read() == 1);
  bool firstBoot = (loadAppConfig(defaultConfig) == false);

  // draw the screen
  updateAppHint();
  ui.setAppIcon(ICON_APP);
  ui.setAppTitle(APP_NAME);
  ui.setBluetoothStatus(false);
  ui.setSDcardStatus((bool)SDcard.card()->sectorCount());
  ui.drawTitleBar();
  ui.setIdleCallback(&onAppInputIdle, IDLE_TIMEOUT);

  // if left button is pressed, clear the configuration
  if (firstBoot) {
    // show the welcome message
    ui.drawDialogFrame("Hello :)");
    ui.drawDialogMessage(BLUE, 0, "SmallStep is a configuration and data down-");
    ui.drawDialogMessage(BLUE, 1, "loading utility for classic MTK GPS loggers.");
    ui.drawDialogMessage(BLACK, 3, "This software has been tested and released,");
    ui.drawDialogMessage(BLACK, 4, "but comes with NO WARRANTY. If you find any");
    ui.drawDialogMessage(BLACK, 5, "bugs, please report it to the author.");
    ui.waitForInputOk();

    // welcome message for Holux M-241 users
    ui.drawDialogFrame("For Holux m-241 users");
    ui.drawDialogMessage(BLACK, 0, "Smallstep can be used with Holux M-241.");
    ui.drawDialogMessage(RED, 1, "However, changing the log mode and log format");
    ui.drawDialogMessage(RED, 2, "are NOT SUPPORTED on this model.");
    ui.drawDialogMessage(BLACK, 3, "Please use the settings menu on the logger to");
    ui.drawDialogMessage(BLACK, 4, "configure them.");
    ui.waitForInputOk();
  }

  // enter the main menu (infinit loop in this function)
  int8_t itemCount = (sizeof(menuMain) / sizeof(menuitem_t));
  ui.openMainMenu(menuMain, itemCount);
}

void loop() {
  // *** NEVER REACH HERE ***
  // power off the device if reaching here
  SDcard.end();         // stop SD card access
  M5.Power.powerOFF();  // power off
}
