#include <M5Stack.h>
#include <SdFat.h>
#include <SimpleBeep.h>

#include "AppUI.h"
#include "MtkLogger.h"
#include "MtkParser.h"
#include "Resources.h"

#define BT_ADDR_LEN 6    // Bluetooth address length (6 bytes; fixed)
#define DEV_NAME_LEN 20  // Bluetooth device name length (20 bytes)

#define SERIAL_SPEED 115200       // 115200 baud
#define CPU_FREQ_LOW 80           // 80 MHz
#define CPU_FREQ_HIGH 240         // 240 MHz
#define SD_ACCESS_SPEED 15000000  // 15 MHz (note: >15 Mhz may cause I/O errors)

#define BEEP_VOLUME 1            // beep volume level (range: 1-10)
#define BEEP_FREQ_SUCCESS 4186   // 4186 Hz (C8) for success beep sound
#define BEEP_FREQ_FAILURE 131    // 131 Hz (C3) for failure beep sound
#define BEEP_DURATION_SHORT 120  // 120 msec for short beep
#define BEEP_DURATION_LONG 600   // 600 msec for long beep

#define TEMP_BIN_NAME "download.bin"  // filename for download cache
#define TEMP_GPX_NAME "download.gpx"  // filename for converting data (before rename)

typedef struct _logmodeset {
  uint8_t distIdx;
  uint8_t timeIdx;
  uint8_t speedIdx;
  bool fullStop;
} logmodeset_t;

typedef struct _appconfig {
  uint16_t length;                  // must be sizeof(appconfig_t)
  bool firstRun;                    // app / first run flag
  uint8_t loggerAddr[BT_ADDR_LEN];  // app / address of paired logger
  char loggerName[DEV_NAME_LEN];    // app / name of pairded logger
  bool playBeep;                    // app / play beep sound
  trackmode_t trackMode;            // parser / how to divide/put tracks
  uint8_t timeOffsetIdx;            // parser / timezone offset in hours
  bool putWaypt;                    // parser / treat points recorded by button as WPTs
  logmodeset_t logMode1;            // log mode #1 / auto log criterias
  logmodeset_t logMode2;            // log mode #2 / auto log criterias
  uint32_t logFormat;               // log format / what fields to be recorded
} appconfig_t;

// general functions
void saveAppConfig();
void loadAppConfig(bool);
bool isZeroedBytes(void *, uint16_t);
bool isLoggerPaired();
bool isSDcardAvailable();
bool isSameBytes(const void *, const void *, uint16_t);
uint32_t switchBitFlags(uint32_t *, uint32_t);
void playBeep(bool, uint16_t);
uint16_t makeNewFileName(char *, char *, uint32_t);
void setBoolDescr(char *, bool);
void updateAppHint();

// application functions
bool runDownloadLog();
bool runFixRTCtime();
bool runPairWithLogger();
bool runClearFlash();
bool runSetLogFormat();
bool runSetLogMode();

// system event handler
void onAppInputIdle();
void onBTStatusUpdate(esp_spp_cb_event_t, esp_spp_cb_param_t *);
void onProgressUpdate(int32_t, int32_t);

// menu item event handlers
void onDownloadLogSelect(mainmenuitem_t *);
void onFixRTCtimeSelect(mainmenuitem_t *);
void onShowLocationSelect(mainmenuitem_t *);
void onPairWithLoggerSelect(mainmenuitem_t *);
void onClearFlashSelect(mainmenuitem_t *);
void onSetLogFormatSelect(mainmenuitem_t *);
void onSetLogModeSelect(mainmenuitem_t *);
void onAppSettingSelect(mainmenuitem_t *);
void onTimezoneUpdate(textmenuitem_t *);
void onTimezoneSelect(textmenuitem_t *);
void onTrackModeSelect(textmenuitem_t *);
void onTrackModeUpdate(textmenuitem_t *);
void onPutWayptSelect(textmenuitem_t *);
void onPutWayptUpdate(textmenuitem_t *);
void onAutoLogDistSelect(textmenuitem_t *);
void onAutoLogDistUpdate(textmenuitem_t *);
void onAutoLogTimeSelect(textmenuitem_t *);
void onAutoLogTimeUpdate(textmenuitem_t *);
void onAutoLogSpeedSelect(textmenuitem_t *);
void onAutoLogSpeedUpdate(textmenuitem_t *);
void onLogFullActionSelect(textmenuitem_t *);
void onLogFullActionUpdate(textmenuitem_t *);
void onLoadDefaultFormatSelect(textmenuitem_t *);
void onLoadDefaultFormatUpdate(textmenuitem_t *);
void onRecordRCRSelect(textmenuitem_t *);
void onRecordRCRUpdate(textmenuitem_t *);
void onRecordValidSelect(textmenuitem_t *);
void onRecordValidUpdate(textmenuitem_t *);
void onRecordMillisSelect(textmenuitem_t *);
void onRecordMillisUpdate(textmenuitem_t *);
void onRecordSpeedSelect(textmenuitem_t *);
void onRecordSpeedUpdate(textmenuitem_t *);
void onRecordAltitudeSelect(textmenuitem_t *);
void onRecordAltitudeUpdate(textmenuitem_t *);
void onRecordHeadingSelect(textmenuitem_t *);
void onRecordHeadingUpdate(textmenuitem_t *);
void onRecordDistanceSelect(textmenuitem_t *);
void onRecordDistanceUpdate(textmenuitem_t *);
void onRecordDgpsSelect(textmenuitem_t *);
void onRecordDgpsUpdate(textmenuitem_t *);
void onRecordDopSelect(textmenuitem_t *);
void onRecordDopUpdate(textmenuitem_t *);
void onRecordSatSelect(textmenuitem_t *);
void onRecordSatUpdate(textmenuitem_t *);
void onPairWithLoggerCfgSelect(textmenuitem_t *);
void onPairWithLoggerCfgUpdate(textmenuitem_t *);
void onOutputSubMenuSelect(textmenuitem_t *);
void onLogMode1SubMenuSelect(textmenuitem_t *);
void onLogMode2SubMenuSelect(textmenuitem_t *);
void onLogFormatSubMenuSelect(textmenuitem_t *);
void onEnableBeepCfgSelect(textmenuitem_t *);
void onEnableBeepCfgUpdate(textmenuitem_t *);
void onClearCacheFileSelect(textmenuitem_t *);
void onPerformFormatSelect(textmenuitem_t *);
void onClearSettingsSelect(textmenuitem_t *);

// device settings
const char *APP_NAME = "SmallStep";  // application title
const char LCD_BRIGHTNESS = 10;
const uint32_t IDLE_TIMEOUT = 120000;

// configuration value lists
const float TIME_OFFSET_VALUES[] = {
    -12, -11, -10, -9, -8,  -7, -6,  -5, -4.5, -4, -3.5, -3, -2,  -1, 0,  1,
    2,   3,   3.5, 4,  4.5, 5,  5.5, 6,  6.5,  7,  8,    9,  9.5, 10, 12, 13};                       // uint: hours
const int16_t LOG_DIST_VALUES[] = {0, 100, 300, 500, 1000, 2000, 3000, 5000};                        // uint: .1 meters
const int16_t LOG_TIME_VALUES[] = {0, 2, 5, 10, 30, 50, 100, 150, 200, 300, 600, 1200, 1800, 3000};  // unit: .1 seconds
const int16_t LOG_SPEED_VALUES[] = {0,   100,  200,  300,  400,  500,  600,  700,  800,              // uint: .1 km/h
                                    900, 1000, 1200, 1400, 1600, 1800, 2000, 2500, 3000};

// global instances and variables
AppUI ui = AppUI();
SdFat SDcard;
MtkLogger logger = MtkLogger(APP_NAME);
appconfig_t cfg;
// uint32_t idleTimer;

// the default configuration
const appconfig_t DEFAULT_CONFIG = {
    sizeof(appconfig_t),  // length
    true,                 // firstRun
    {0, 0, 0, 0, 0, 0},   // loggerAddr
    "NO LOGGER",          // loggerName
    true,                 // playBeep
    TRK_ONE_DAY,          // trackMode
    14,                   // timeOffsetIdx (14 -> UTC+0)
    true,                 // putWaypt
    {0, 7, 0, true},      // logMode1 {distIdx, timeIdx (7 -> 15sec), speedIdx, fullStop}
    {0, 5, 0, true},      // logMode2 {distIdx, timeIdx (5 -> 5sec), speedIdx, fullStop}
    (FMT_FIXONLY | FMT_TIME | FMT_LON | FMT_LAT | FMT_HEIGHT | FMT_SPEED | FMT_RCR)  // logFormat
};

// application main menu item list
mainmenuitem_t menuMain[] = {
    // {caption, iconData, enabled, onSelect}
    {"Download Log", ICON_DOWNLOAD_LOG, true, &onDownloadLogSelect},
    {"Fix RTC Time", ICON_FIX_RTC, true, &onFixRTCtimeSelect},
    {"Erase Log Data", ICON_ERASE_LOG, true, &onClearFlashSelect},
    {"Set Log Mode", ICON_LOG_MODE, true, &onSetLogModeSelect},
    {"Set Log Format", ICON_LOG_FORMAT, true, &onSetLogFormatSelect},
    {"Show Location", ICON_NAVIGATION, true, &onShowLocationSelect},
    {"Link to Logger", ICON_PAIR_LOGGER, true, &onPairWithLoggerSelect},
    {"App Settings", ICON_APP_SETTINGS, true, &onAppSettingSelect},
};

// output settings sub-menu item list
textmenuitem_t cfgOutput[] = {
    // {caption, hintText, valueDescr, onSelectItem, onDescrUpdate}
    {"Back", "Exit this menu", "<<", true, NULL, NULL, NULL},
    {"Track mode", "How to divide tracks in GPX file", "", true,  //
     &onTrackModeSelect, &onTrackModeUpdate, NULL},
    {"Timezone offset", "UTC offset for 'a track per day' mode", "", true,  //
     &onTimezoneSelect, &onTimezoneUpdate, NULL},
    {"Put WAYPTs", "Treat manulally recorded points as WAYPTs (POIs)", "", true,  //
     &onPutWayptSelect, &onPutWayptUpdate, NULL},
};

// log mode settings sub-menu item list
textmenuitem_t cfgLogMode1[] = {
    // {caption, descr, valueDescr, onSelect, valueDescrUpdate}
    {"Back", "Exit this menu", "<<", true, NULL, NULL, NULL},
    {"Log by distance", "Auto log by moving distance", "", true,  //
     &onAutoLogDistSelect, &onAutoLogDistUpdate, &cfg.logMode1.distIdx},
    {"Log by time", "Auto log by elapsed time", "", true,  //
     &onAutoLogTimeSelect, &onAutoLogTimeUpdate, &cfg.logMode1.timeIdx},
    {"Log by speed", "Auto log by exceeded speed", "", true,  //
     &onAutoLogSpeedSelect, &onAutoLogSpeedUpdate, &cfg.logMode1.speedIdx},
    {"Log full action", "Behavior when the logger flash is full", "", true,  //
     &onLogFullActionSelect, &onLogFullActionUpdate, &cfg.logMode1.fullStop},
};

textmenuitem_t cfgLogMode2[] = {
    // {caption, descr, valueDescr, enabled, onSelect, valueDescrUpdate}
    {"Back", "Exit this menu", "<<", true, NULL, NULL, NULL},
    {"Log by distance", "Auto log by moving distance", "", true,  //
     &onAutoLogDistSelect, &onAutoLogDistUpdate, &cfg.logMode2.distIdx},
    {"Log by time", "Auto log by elapsed time", "", true,  //
     &onAutoLogTimeSelect, &onAutoLogTimeUpdate, &cfg.logMode2.timeIdx},
    {"Log by speed", "Auto log by exceeded speed", "", true,  //
     &onAutoLogSpeedSelect, &onAutoLogSpeedUpdate, &cfg.logMode2.speedIdx},
    {"Log full action", "Behavior when the logger flash is full", "", true,  //
     &onLogFullActionSelect, &onLogFullActionUpdate, &cfg.logMode2.fullStop},
};

// log format settings sub-menu item list
textmenuitem_t cfgLogFormat[] = {
    // {caption, descr, valueDescr, enabled, onSelect, valueDescrUpdate}
    {"Back", "Exit this menu", "<<", true, NULL, NULL},
    {"Load defaults", "Reset to the default format", "", true,  //
     &onLoadDefaultFormatSelect, &onLoadDefaultFormatUpdate, &cfg.logFormat},
    {"TIME (required fields)", "Date and time data in seconds", "Enabled", false,  //
     NULL, NULL, &cfg.logFormat},
    {"LAT, LON (required fields)", "Latitude and longitude data", "Enabled", false,  //
     NULL, NULL, &cfg.logFormat},
    {"SPEED", "Moving speed data", "", true,  //
     &onRecordSpeedSelect, &onRecordSpeedUpdate, &cfg.logFormat},
    {"ALT", "Altitude data", "", true,  //
     &onRecordAltitudeSelect, &onRecordAltitudeUpdate, &cfg.logFormat},
    {"RCR", "Record reason (needed to put WAYPTs)", "", true,  //
     &onRecordRCRSelect, &onRecordRCRUpdate, &cfg.logFormat},
    {"----", "The fields below are not used by SmallStep", "", false, NULL, NULL, NULL},
    {"TRACK", "Track angle data", "", true,  //
     &onRecordHeadingSelect, &onRecordHeadingUpdate, &cfg.logFormat},
    {"VALID", "Positioning status data (valid/invalid)", "", true,  //
     &onRecordValidSelect, &onRecordValidUpdate, &cfg.logFormat},
    {"DIST", "Moving distance data", "", true,  //
     &onRecordDistanceSelect, &onRecordDistanceUpdate, &cfg.logFormat},
    {"MSEC", "Time data in millisecond", "", true,  //
     &onRecordMillisSelect, &onRecordMillisUpdate, &cfg.logFormat},
    {"DSTA, DAGE", "Differencial GPS data", "Disabled", true,  //
     &onRecordDgpsSelect, &onRecordDgpsUpdate, &cfg.logFormat},
    {"PDOP, HDOP, VDOP", "Dilution of precision data", "Disabled", true,  //
     &onRecordDopSelect, &onRecordDopUpdate, &cfg.logFormat},
    {"NSAT, ELE, AZI, SNR", "Satellite data", "Disabled", true,  //
     &onRecordSatSelect, &onRecordSatUpdate, &cfg.logFormat},
};

textmenuitem_t *cfgResetFormat = &cfgLogFormat[1];

// application settings menu item list
textmenuitem_t cfgMain[] = {
    // {caption, descr, valueDescr, enabled, onSelect, valueDescrUpdate}
    {"Save and exit", "Return to the main menu", "", true, NULL, NULL},
    {"Pairing with a GPS logger", "Discover supported GPS loggers", ">>", true,  //
     &onPairWithLoggerCfgSelect, &onPairWithLoggerCfgUpdate, NULL},
    {"Output settings", "GPX log output options", ">>", true,  //
     &onOutputSubMenuSelect, NULL, NULL},
    {"Log mode preset #1", "Auto-log settings of the logger", ">>", true,  //
     &onLogMode1SubMenuSelect, NULL, NULL},
    {"Log mode preset #2", "Auto-log settings of the logger", ">>", true,  //
     &onLogMode2SubMenuSelect, NULL, NULL},
    {"Log format preset", "Contents to be stored on the logger", ">>", true,  //
     &onLogFormatSubMenuSelect, NULL, NULL},
    {"Beep sound", "Play beep sound when a task is finished", ">>", true,  //
     &onEnableBeepCfgSelect, &onEnableBeepCfgUpdate, NULL},
    {"----", "", "", false, NULL, NULL, NULL},
    {"Format SD card", "Format the inserted SD card", "", true,  //
     &onPerformFormatSelect, NULL, NULL},
    {"Clear carche file", "Delete the download cache file", "", true,  //
     &onClearCacheFileSelect, NULL, NULL},
    {"Clear all settings", "Erase the current settings of SmallStep", "", true,  //
     &onClearSettingsSelect, NULL, NULL},
};

void updateAppHint() {
  // set the Paied logger name and address as the application hint
  char addrStr[16];
  sprintf(addrStr, "%02X%02X-%02X%02X-%02X%02X",                    // xxxx-xxxx-xxxx
          cfg.loggerAddr[0], cfg.loggerAddr[1], cfg.loggerAddr[2],  //
          cfg.loggerAddr[3], cfg.loggerAddr[4], cfg.loggerAddr[5]);
  ui.setAppHints(cfg.loggerName, addrStr);
}

void onBTStatusUpdate(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  switch (event) {
  case ESP_SPP_INIT_EVT:  // SPP is started
    if (param == NULL) {
      ui.setBluetoothStatus(true);
    }
    break;

  case ESP_SPP_OPEN_EVT:  // SPP connection is established
    memcpy(cfg.loggerAddr, param->open.rem_bda, BT_ADDR_LEN);
    break;

  case ESP_SPP_UNINIT_EVT:  // SPP is stopped
    if (param == NULL) {
      ui.setBluetoothStatus(false);
    }
    break;
  }
}

void onProgressUpdate(int32_t current, int32_t max) {
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
    ui.drawDialogText(BLACK, 0, "Pairing has not been done yet.");
    ui.drawDialogText(BLUE, 1, "Pair with your GPS logger now?");

    if (ui.promptOkCancel() == BID_OK) {
      ui.drawDialogText(BLACK, 1, "Pair with your GPS logger now? [ OK ]");
      paired = runPairWithLogger();

      if (paired) ui.promptOk();
    } else {
      ui.drawDialogText(BLACK, 1, "Pair with your GPS logger now? [Cancel]");
      ui.drawDialogText(BLUE, 3, "The pairing operation is not performed.");
    }
  }

  return paired;
}

bool isSDcardAvailable() {
  if (SDcard.card()->sectorCount() == 0) {  // cannot get the sector count
    // print the error message
    ui.drawDialogFrame("SD Card Error");
    ui.drawDialogText(RED, 0, "SD card is not available.");
    ui.drawDialogText(RED, 1, "- SD card is corrupted or is not inserted");
    return false;
  }

  return true;
}

void setBoolDescr(char *descr, bool value) {
  if (value) {
    strcpy(descr, "Enabled");
  } else {
    strcpy(descr, "Disabled");
  }
}

bool connectLogger(uint8_t msgLine) {
  // connect to the logger
  ui.drawDialogText(BLUE, (msgLine + 0), "Connecting to GPS logger...");

  if (!logger.connect(cfg.loggerAddr)) {
    ui.drawDialogText(RED, (msgLine + 0), "Connecting to GPS logger... failed.");
    ui.drawDialogText(RED, (msgLine + 1), "- Make sure BT is enabled on the GPS logger");
    ui.drawDialogText(RED, (msgLine + 2), "- If this problem occurs repeatly, please re-");
    ui.drawDialogText(RED, (msgLine + 3), "  start the logger and SmallStep");
    return false;
  }

  ui.drawDialogText(BLACK, (msgLine), "Connecting to GPS logger... done.");

  return true;
}

uint16_t makeFilename(char *gpxName, time_t startTime) {
  const char FILENAME_FMT[] = "gps_%04d%02d%02d-%02d%02d%02d";

  startTime += 3600 * TIME_OFFSET_VALUES[cfg.timeOffsetIdx];
  struct tm *ltime = localtime(&startTime);

  char baseName[32];
  sprintf(baseName, FILENAME_FMT,
          (ltime->tm_year + 1900),  // year (4 digits)
          (ltime->tm_mon + 1),      // month (1-12)
          ltime->tm_mday,           // day of month (1-31)
          ltime->tm_hour,           // hour (0-23)
          ltime->tm_min,            // min (0-59)
          ltime->tm_sec);           // sec (0-59)

  // 最初のtrkptの時刻を使ってGPXファイルとBINファイルの保存名を決める
  for (uint16_t i = 1; i <= 65535; i++) {
    sprintf(gpxName, "%s_%02d.gpx", baseName, i);

    if (!SDcard.exists(gpxName)) return i;
  }

  return -1;
}

bool runDownloadLog() {
  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;
  if (!isSDcardAvailable()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Download Log");
  ui.drawNavBar(NULL);

  File32 binFile = SDcard.open(TEMP_BIN_NAME, (O_CREAT | O_RDWR));
  File32 gpxFile = SDcard.open(TEMP_GPX_NAME, (O_CREAT | O_RDWR | O_TRUNC));
  if ((!binFile) || (!gpxFile)) {
    if (binFile) binFile.close();
    if (gpxFile) gpxFile.close();

    ui.drawDialogText(RED, 0, "Could not open temporally files.");
    return false;
  }

  if (!connectLogger(0)) return false;

  ui.drawDialogText(BLUE, 1, "Downloading log data...");
  {
    if (!logger.downloadLogData(&binFile, &onProgressUpdate)) {
      binFile.close();
      gpxFile.close();

      ui.drawDialogText(RED, 1, "Downloading log data... failed.");
      ui.drawDialogText(RED, 2, "- Keep your logger close to this device");
      ui.drawDialogText(RED, 3, "- Power cycling may fix this problem");
      return false;
    }

    // disconnect from the logger and close the downloaded data file
    logger.disconnect();
  }
  ui.drawDialogText(BLACK, 1, "Downloading log data... done.");

  ui.drawDialogText(BLUE, 2, "Converting data to GPX file...");
  {
    // change CPU freq. to 240 MHz temporally
    setCpuFrequencyMhz(CPU_FREQ_HIGH);

    // convert the binary file to GPX file and get the summary
    parseopt_t parseopt = {cfg.trackMode, TIME_OFFSET_VALUES[cfg.timeOffsetIdx], cfg.putWaypt};
    MtkParser *parser = new MtkParser(parseopt);
    gpxinfo_t gpxInfo = parser->convert(&binFile, &gpxFile, &onProgressUpdate);
    delete parser;

    // reset CPU freq. to 80 MHz
    setCpuFrequencyMhz(CPU_FREQ_LOW);

    // close the GPX file before rename and rename to the output file name
    binFile.close();
    gpxFile.close();

    // make a unique name for the GPX file
    char gpxName[32];
    makeFilename(gpxName, gpxInfo.startTime);

    if (gpxInfo.trackCount > 0) {
      SDcard.rename(TEMP_GPX_NAME, gpxName);

      // make the output message strings
      char outputstr[48], summarystr[48];
      sprintf(outputstr, "Output file : %s", gpxName);
      sprintf(summarystr, "Summary : %d TRKs, %d TRKPTs, %d WPTs",  //
              gpxInfo.trackCount, gpxInfo.trkptCount, gpxInfo.wayptCount);

      // print the result message
      ui.drawDialogText(BLACK, 2, "Converting data to GPX file... done.");
      ui.drawDialogText(BLUE, 3, outputstr);
      ui.drawDialogText(BLUE, 4, summarystr);
    } else {
      // print the result message
      ui.drawDialogText(BLACK, 2, "Converting data to GPX file... done.");
      ui.drawDialogText(BLUE, 3, "No output file is saved because of there is");
      ui.drawDialogText(BLUE, 4, "no valid record in the log data.");
    }
  }

  return true;
}

void onAppInputIdle() {
  Serial.printf("SmallStep.onIdle: idle shutdown (t=%d)\n", millis());
  SDcard.end();
  M5.Power.powerOFF();
}

/**
 *
 */
void onDownloadLogSelect(mainmenuitem_t *item) {
  bool result = runDownloadLog();
  logger.disconnect();

  playBeep(result, BEEP_DURATION_LONG);
  ui.promptOk();
}

bool runFixRTCtime() {
  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Fix RTC time");
  ui.drawNavBar(NULL);

  // connect to the logger
  if (!connectLogger(0)) return false;

  ui.drawDialogText(BLUE, 1, "Setting RTC datetime...");
  {
    if (!logger.fixRTCdatetime()) {
      ui.drawDialogText(RED, 1, "Setting RTC datetime... failed.");
      ui.drawDialogText(RED, 2, "- Keep GPS logger close to this device");
      return false;
    }
  }
  // print the result message
  ui.drawDialogText(BLACK, 1, "Setting RTC datetime... done.");
  ui.drawDialogText(BLUE, 2, "The logger has been restarted to apply the fix.");
  ui.drawDialogText(BLUE, 3, "Please check the logging status.");
  return true;
}

/**
 *
 */
void onFixRTCtimeSelect(mainmenuitem_t *item) {
  bool result = runFixRTCtime();
  logger.disconnect();

  playBeep(result, BEEP_DURATION_LONG);
  ui.promptOk();
}

/**
 *
 */
void onShowLocationSelect(mainmenuitem_t *item) {
}

bool runPairWithLogger() {
  const String DEVICE_NAMES[] = {"747PRO GPS", "HOLUX_M-241"};
  const int8_t DEVICE_COUNT = 2;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Pair with Logger");
  ui.drawNavBar(NULL);  // disable the navigation bar

  ui.drawDialogText(BLUE, 0, "Discovering GPS logger...");

  for (int i = 0; i < DEVICE_COUNT; i++) {
    // print the device name trying to connect
    char msgbuf1[48], msgbuf2[48];
    sprintf(msgbuf1, "- %s", DEVICE_NAMES[i].c_str());
    ui.drawDialogText(BLUE, (1 + i), msgbuf1);

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
      ui.drawDialogText(BLACK, 0, "Discovering GPS logger... done");
      ui.drawDialogText(BLACK, (1 + i), msgbuf1);
      ui.drawDialogText(BLUE, (2 + i), "Successfully paired with the discovered logger.");
      ui.drawDialogText(BLUE, (3 + i), msgbuf2);

      // update GUI
      updateAppHint();
      ui.drawTitleBar();

      // save the app configuration
      saveAppConfig();
      return true;
    } else {  // failed to connect to the device
      sprintf(msgbuf1, "- %s : not found.", DEVICE_NAMES[i].c_str());
      ui.drawDialogText(BLACK, (1 + i), msgbuf1);
      continue;
    }
  }

  // print the failure message
  ui.drawDialogText(BLACK, 0, "Discovering GPS logger... failed.");
  ui.drawDialogText(RED, (1 + DEVICE_COUNT), "Cannot discover any supported logger.");
  ui.drawDialogText(RED, (2 + DEVICE_COUNT), "- If this problem occurs repeatly, please re-");
  ui.drawDialogText(RED, (3 + DEVICE_COUNT), "  start the logger and SmallStep");

  return false;
}

/**
 *
 */
void onPairWithLoggerSelect(mainmenuitem_t *item) {
  bool result = runPairWithLogger();

  playBeep(result, BEEP_DURATION_LONG);
  ui.promptOk();
}

bool runClearFlash() {
  // return false if the logger is not paired yet
  if (!isLoggerPaired()) return false;

  // draw a message dialog frame and clear the navigation bar
  ui.drawDialogFrame("Erase Log");
  ui.drawNavBar(NULL);

  // prompt the user to confirm the operation
  ui.drawDialogText(BLUE, 0, "Are you sure to erase log data?");
  if (ui.promptOkCancel() == BID_OK) {
    ui.drawDialogText(BLACK, 0, "Are you sure to erase log data? [ OK ]");
    ui.drawNavBar(NULL);
  } else {
    ui.drawDialogText(BLACK, 0, "Are you sure to erase log data?  [Cancel]");
    ui.drawDialogText(BLUE, 1, "The operation is not performed.");
    return false;
  }

  // connect to the logger
  if (!connectLogger(1)) return false;

  ui.drawDialogText(BLUE, 2, "Erasing log data...");
  if (logger.clearFlash(&onProgressUpdate)) {
    ui.drawDialogText(BLACK, 2, "Erasing log data... done.");
  } else {
    ui.drawDialogText(RED, 2, "Erasing log data... timeout.");
    ui.drawDialogText(RED, 3, "Please retry the erase operation");
    return false;
  }

  // reload the logger and break
  ui.drawDialogText(BLUE, 3, "Reloading logger... ");

  if (logger.reloadDevice()) {
    ui.drawDialogText(BLACK, 3, "Reloading logger... done.");
    ui.drawDialogText(BLUE, 4, "Hope you have a nice trip next time :)");
  } else {
    ui.drawDialogText(RED, 3, "Reloading logger... failed.");
    ui.drawDialogText(RED, 4, "Please restart GPS logger manually");
    ui.drawDialogText(RED, 4, "before use it.");
    return false;
  }

  return true;
}

void onClearFlashSelect(mainmenuitem_t *item) {
  bool result = runClearFlash();
  logger.disconnect();

  playBeep(result, BEEP_DURATION_LONG);
  ui.promptOk();
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

  ui.drawDialogText(BLUE, 1, "Updating the log format...");
  if (!logger.getLogFormat(&curLogFormat)) {
    ui.drawDialogText(RED, 1, "Updating the log format... failed");
    ui.drawDialogText(RED, 1, "Cannot get the current log format.");
    return false;
  }

  if (curLogFormat == newLogFormat) {
    char buf[40];
    sprintf(buf, "Log format : 0x%08X", newLogFormat);

    ui.drawDialogText(BLACK, 1, "Updating the log format... unchanged.");
    ui.drawDialogText(BLUE, 2, "The configured log format is already set.");
    ui.drawDialogText(BLUE, 3, buf);
    return true;
  }

  if (!logger.setLogFormat(newLogFormat)) {
    ui.drawDialogText(RED, 1, "Updating the log format... failed");
    ui.drawDialogText(RED, 2, "Failed to change the log format.");
    return false;
  }

  char buf[48];
  sprintf(buf, "from 0x%08X to 0x%08X.", curLogFormat, newLogFormat);
  ui.drawDialogText(BLACK, 1, "Updating the log format... done.");
  ui.drawDialogText(BLUE, 2, "The Log format on the logger is changed");
  ui.drawDialogText(BLUE, 3, buf);

  return true;
}

void onSetLogFormatSelect(mainmenuitem_t *item) {
  bool result = runSetLogFormat();
  logger.disconnect();

  playBeep(result, BEEP_DURATION_LONG);
  ui.promptOk();
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
  recordmode_t newRecMode = (cfg.logMode1.fullStop) ? MODE_FULLSTOP : MODE_OVERWRITE;
  logcriteria_t newLogCri = {LOG_DIST_VALUES[cfg.logMode1.distIdx], LOG_TIME_VALUES[cfg.logMode1.timeIdx],
                             LOG_SPEED_VALUES[cfg.logMode1.speedIdx]};

  ui.drawDialogText(BLUE, 1, "Updating the log mode...");
  if ((!logger.getLogRecordMode(&curRecMode)) || (!logger.getLogCriteria(&curLogCri))) {
    ui.drawDialogText(RED, 1, "Updating the log mode... failed.");
    ui.drawDialogText(RED, 2, "Cannot get the current mode. Please retry.");
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
    ui.drawDialogText(BLACK, 1, "Updating the log mode... unchanged.");
    ui.drawDialogText(BLUE, 2, "The configured log mode is already set.");
    ui.drawDialogText(BLUE, 3, buf[0]);
    ui.drawDialogText(BLUE, 4, buf[1]);
    return true;
  }

  if (!logger.setLogRecordMode(newRecMode) || !logger.setLogCriteria(newLogCri)) {
    ui.drawDialogText(RED, 1, "Updating the log mode... failed.");
    ui.drawDialogText(RED, 2, "Cannot set the new log mode. Please retry.");
    return false;
  }

  ui.drawDialogText(BLACK, 1, "Updating the log mode... done.");
  ui.drawDialogText(BLUE, 2, "The configured log mode is set successfully.");
  ui.drawDialogText(BLUE, 3, buf[0]);
  ui.drawDialogText(BLUE, 4, buf[1]);

  return true;
}

void onSetLogModeSelect(mainmenuitem_t *item) {
  bool result = runSetLogMode();
  logger.disconnect();

  playBeep(result, BEEP_DURATION_LONG);
  ui.promptOk();
}

void onAppSettingSelect(mainmenuitem_t *item) {
  Serial.printf("SmallStep.onAppSettingSelect: open the app settings menu\n");

  // open the configuration menu
  int8_t itemCount = (sizeof(cfgMain) / sizeof(textmenuitem_t));
  ui.openTextMenu("Settings", cfgMain, itemCount);

  // save the app configuration after the menu is closed
  saveAppConfig();
}

void onTrackModeSelect(textmenuitem_t *item) {
  cfg.trackMode = (trackmode_t)(((int)cfg.trackMode + 1) % 3);
}

void onTrackModeUpdate(textmenuitem_t *item) {
  if (cfg.trackMode == TRK_ONE_DAY) {
    strcpy(item->valueDescr, "A track per day");
  } else if (cfg.trackMode == TRK_AS_IS) {
    strcpy(item->valueDescr, "Split as recorded");
  } else {
    strcpy(item->valueDescr, "One track");
  }
}

void onTimezoneSelect(textmenuitem_t *item) {
  uint8_t valCount = sizeof(TIME_OFFSET_VALUES) / sizeof(float);
  cfg.timeOffsetIdx = (cfg.timeOffsetIdx + 1) % valCount;
}

void onTimezoneUpdate(textmenuitem_t *item) {
  sprintf(item->valueDescr, "UTC%+.1f", TIME_OFFSET_VALUES[cfg.timeOffsetIdx]);
}

void onPutWayptSelect(textmenuitem_t *item) {
  cfg.putWaypt = (!cfg.putWaypt);
}

void onPutWayptUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, cfg.putWaypt);
}

void onAutoLogDistSelect(textmenuitem_t *item) {
  uint8_t *cfgVar = (uint8_t *)item->var;
  uint8_t valCount = sizeof(LOG_DIST_VALUES) / sizeof(int16_t);

  *cfgVar = max(0, (*cfgVar + 1) % valCount);
}

void onAutoLogDistUpdate(textmenuitem_t *item) {
  uint8_t *cfgVar = (uint8_t *)(item->var);

  if (LOG_DIST_VALUES[*cfgVar] == 0) {
    strcpy(item->valueDescr, "Disabled");
  } else if ((LOG_DIST_VALUES[*cfgVar] % 10) == 0) {
    sprintf(item->valueDescr, "%d meters", (LOG_DIST_VALUES[*cfgVar] / 10));
  } else {
    sprintf(item->valueDescr, "%d.%d meters",  //
            (LOG_DIST_VALUES[*cfgVar] / 10), (LOG_DIST_VALUES[*cfgVar] % 10));
  }
}

void onAutoLogTimeSelect(textmenuitem_t *item) {
  uint8_t *cfgVar = (uint8_t *)(item->var);
  uint8_t valCount = sizeof(LOG_TIME_VALUES) / sizeof(int16_t);

  *cfgVar = max(0, (*cfgVar + 1) % valCount);
}

void onAutoLogTimeUpdate(textmenuitem_t *item) {
  uint8_t *cfgVar = (uint8_t *)(item->var);

  if (LOG_TIME_VALUES[*cfgVar] == 0) {
    strcpy(item->valueDescr, "Disabled");
  } else if ((LOG_TIME_VALUES[*cfgVar] % 10) == 0) {
    sprintf(item->valueDescr, "%d seconds", (LOG_TIME_VALUES[*cfgVar] / 10));
  } else {
    sprintf(item->valueDescr, "%d.%d seconds",  //
            (LOG_TIME_VALUES[*cfgVar] / 10), (LOG_TIME_VALUES[*cfgVar] % 10));
  }
}

void onAutoLogSpeedSelect(textmenuitem_t *item) {
  uint8_t *cfgVar = (uint8_t *)(item->var);
  uint8_t valCount = sizeof(LOG_SPEED_VALUES) / sizeof(int16_t);

  *cfgVar = max(0, (*cfgVar + 1) % valCount);
}

void onAutoLogSpeedUpdate(textmenuitem_t *item) {
  uint8_t *cfgVar = (uint8_t *)(item->var);

  if (LOG_SPEED_VALUES[*cfgVar] == 0) {
    strcpy(item->valueDescr, "Disabled");
  } else if ((LOG_SPEED_VALUES[*cfgVar] % 10) == 0) {
    sprintf(item->valueDescr, "%d km/h", (LOG_SPEED_VALUES[*cfgVar] / 10));
  } else {
    sprintf(item->valueDescr, "%d.%d km/h",  //
            (LOG_SPEED_VALUES[*cfgVar] / 10), (LOG_SPEED_VALUES[*cfgVar] % 10));
  }
}

void onLogFullActionSelect(textmenuitem_t *item) {
  uint8_t *cfgVar = (uint8_t *)(item->var);
  *cfgVar = (!(*cfgVar));
}

void onLogFullActionUpdate(textmenuitem_t *item) {
  bool *cfgVar = (bool *)(item->var);

  if (*cfgVar) {
    strcpy(item->valueDescr, "Stop logging");
  } else {
    strcpy(item->valueDescr, "Overwrap");
  }
}

void onLoadDefaultFormatSelect(textmenuitem_t *item) {
  cfg.logFormat = DEFAULT_CONFIG.logFormat;

  for (int8_t i = 0; i < sizeof(cfgLogFormat) / sizeof(textmenuitem_t); i++) {
    textmenuitem_t *ci = &cfgLogFormat[i];
    if (ci->onUpdateDescr != NULL) ci->onUpdateDescr(ci);
  }
}

void onLoadDefaultFormatUpdate(textmenuitem_t *item) {
  sprintf(item->valueDescr, "0x%08X", cfg.logFormat);
}

void onRecordRCRSelect(textmenuitem_t *item) {
  switchBitFlags((uint32_t *)(item->var), FMT_RCR);
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordRCRUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, (cfg.logFormat & FMT_RCR));
}

void onRecordValidSelect(textmenuitem_t *item) {
  switchBitFlags((uint32_t *)(item->var), FMT_VALID);
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordValidUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, (cfg.logFormat & FMT_VALID));
}

void onRecordMillisSelect(textmenuitem_t *item) {
  switchBitFlags((uint32_t *)(item->var), FMT_MSEC);
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordMillisUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, (cfg.logFormat & FMT_MSEC));
}

void onRecordSpeedSelect(textmenuitem_t *item) {
  switchBitFlags((uint32_t *)(item->var), FMT_SPEED);
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordSpeedUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, (cfg.logFormat & FMT_SPEED));
}

void onRecordAltitudeSelect(textmenuitem_t *item) {
  switchBitFlags((uint32_t *)(item->var), FMT_HEIGHT);
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordAltitudeUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, (cfg.logFormat & FMT_HEIGHT));
}

void onRecordHeadingSelect(textmenuitem_t *item) {
  switchBitFlags((uint32_t *)(item->var), FMT_TRACK);
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordHeadingUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, (cfg.logFormat & FMT_TRACK));
}

void onRecordDistanceSelect(textmenuitem_t *item) {
  switchBitFlags((uint32_t *)(item->var), FMT_DIST);
  onLoadDefaultFormatUpdate(cfgResetFormat);
}

void onRecordDistanceUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, (cfg.logFormat & FMT_DIST));
}

void onRecordDgpsSelect(textmenuitem_t *item) {
  switchBitFlags((uint32_t *)(item->var), (FMT_DSTA | FMT_DAGE));
  onLoadDefaultFormatUpdate(cfgResetFormat);
};

void onRecordDgpsUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, (cfg.logFormat & FMT_DSTA));
};

void onRecordDopSelect(textmenuitem_t *item) {
  switchBitFlags((uint32_t *)item->var, (FMT_PDOP | FMT_HDOP | FMT_VDOP));
  onLoadDefaultFormatUpdate(cfgResetFormat);
};

void onRecordDopUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, (cfg.logFormat & FMT_PDOP));
};

void onRecordSatSelect(textmenuitem_t *item) {
  switchBitFlags((uint32_t *)item->var, (FMT_NSAT | FMT_ELE | FMT_AZI | FMT_SNR));
  onLoadDefaultFormatUpdate(cfgResetFormat);
};

void onRecordSatUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, (cfg.logFormat & FMT_NSAT));
};

void onPairWithLoggerCfgSelect(textmenuitem_t *item) {
  runPairWithLogger();
  ui.promptOk();
}

void onPairWithLoggerCfgUpdate(textmenuitem_t *item) {
  if (isZeroedBytes(&cfg.loggerAddr, BT_ADDR_LEN)) {
    strcpy(item->valueDescr, "Not paired");
  } else {
    strcpy(item->valueDescr, cfg.loggerName);
  }
}

void onOutputSubMenuSelect(textmenuitem_t *item) {
  // enter the output settigns sub-menu
  int8_t itemCount = (sizeof(cfgOutput) / sizeof(textmenuitem_t));
  ui.openTextMenu("Settings > Output", cfgOutput, itemCount);
}

void onLogMode1SubMenuSelect(textmenuitem_t *item) {
  // enter the log mode settings sub-menu
  int8_t itemCount = (sizeof(cfgLogMode1) / sizeof(textmenuitem_t));
  ui.openTextMenu("Settings > Log Mode #1", cfgLogMode1, itemCount);
}

void onLogMode2SubMenuSelect(textmenuitem_t *item) {
  // enter the log mode settings sub-menu
  int8_t itemCount = (sizeof(cfgLogMode2) / sizeof(textmenuitem_t));
  ui.openTextMenu("Settings > Log Mode #2", cfgLogMode2, itemCount);
}

void onLogFormatSubMenuSelect(textmenuitem_t *item) {
  // enter the log format settings sub-menu
  int8_t itemCount = (sizeof(cfgLogFormat) / sizeof(textmenuitem_t));
  ui.openTextMenu("Settings > Log Format", cfgLogFormat, itemCount);
}

void onEnableBeepCfgSelect(textmenuitem_t *item) {
  cfg.playBeep = (!cfg.playBeep);
}

void onEnableBeepCfgUpdate(textmenuitem_t *item) {
  setBoolDescr(item->valueDescr, cfg.playBeep);
}

void onClearCacheFileSelect(textmenuitem_t *item) {
  if (SDcard.exists(TEMP_BIN_NAME)) SDcard.remove(TEMP_BIN_NAME);
  if (SDcard.exists(TEMP_GPX_NAME)) SDcard.remove(TEMP_GPX_NAME);

  ui.drawDialogFrame("Delete cache file");
  ui.drawNavBar(NULL);
  ui.drawDialogText(BLUE, 0, "Cache files are cleared.");

  ui.promptOk();
}

void onPerformFormatSelect(textmenuitem_t *item) {
  ui.drawDialogFrame("Format SD card");
  ui.drawNavBar(NULL);

  ui.drawDialogText(BLUE, 0, "Are you sure to format the SD card?");
  ui.drawDialogText(RED, 1, "Note : All data on the SD card will be lost.");
  ui.drawDialogText(RED, 2, "  This operation cannot be undone.");

  if (ui.promptOkCancel() == BID_OK) {
    ui.drawDialogText(BLACK, 0, "Are you sure to format the SD card? [ OK ]");
  } else {
    ui.drawDialogText(BLACK, 0, "Are you sure to format the SD card? [Cancel]");
    ui.drawDialogText(BLUE, 4, "The operation is not performed.");

    ui.promptOk();
    return;
  }

  ui.drawDialogText(BLUE, 3, "Formatting the SD card...");
  if (SDcard.format()) {
    ui.drawDialogText(BLACK, 3, "Formatting the SD card... done.");
    ui.drawDialogText(BLUE, 4, "The SD card is formatted successfully.");
  } else {
    ui.drawDialogText(RED, 3, "Formatting the SD card... failed.");
    ui.drawDialogText(RED, 4, "Failed to format the SD card. Please retry.");
    return;
  }

  ui.promptOk();

  SDcard.end();
  M5.Power.reset();
}

void onClearSettingsSelect(textmenuitem_t *item) {
  ui.drawDialogFrame("Clear Settings");
  ui.drawDialogText(BLUE, 0, "Are you sure to clear all settings?");
  ui.drawDialogText(BLACK, 1, "Note : The log data on the paired GPS logger");
  ui.drawDialogText(BLACK, 2, "  and the files in the SD card will NOT be");
  ui.drawDialogText(BLACK, 3, "  deleted by this operation.");

  if (ui.promptOkCancel() == BID_OK) {
    ui.drawDialogText(BLACK, 0, "Are you sure to clear all settings? [ OK ]");
    ui.drawDialogText(BLUE, 5, "Clear all settings...");
    delay(1000);

    // clear the configuration data
    cfg.length = 0;   // write the invalid length to the configuration
    saveAppConfig();  // save the configuration

    // stop the SD card access and reset the device
    SDcard.end();
    M5.Power.reset();
  } else {
    ui.drawDialogText(BLACK, 0, "Are you sure to clear all settings? [Cancel]");
    ui.drawDialogText(BLUE, 5, "The operation is not performed.");

    ui.promptOk();
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

uint32_t switchBitFlags(uint32_t *value, uint32_t flags) {
  *value ^= flags;
  return *value;
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
  EEPROM.write(sizeof(appconfig_t), chk);  // write checksum
  EEPROM.commit();                         // commit the data to the EEPROM

  Serial.printf("SmallStep.saveConfig: configuration is saved to the EEPROM\n");
}

void loadAppConfig(bool loadDefault) {
  uint8_t *pcfg = (uint8_t *)(&cfg);
  uint8_t chk = 0;

  if (!loadDefault) {
    // read configuration data from EEPROM
    for (uint16_t i = 0; i < sizeof(appconfig_t); i++) {
      uint8_t b = EEPROM.read(i);
      *pcfg = b;     // read the configuration data byte
      chk ^= *pcfg;  // calculate the checksum (XOR all bytes)
      pcfg += 1;
    }
  }

  // load the default configuration if loadDefault is set to true or the EEPROM configuration
  // is invalid (the length field is wrong or the checksum is not matched)
  loadDefault |= ((cfg.length != sizeof(appconfig_t)) || (EEPROM.read(sizeof(appconfig_t) != chk)));
  if (loadDefault) {
    memcpy(&cfg, &DEFAULT_CONFIG, sizeof(appconfig_t));

    Serial.printf("SmallStep.loadConfig: default configuration is loaded\n");
    return;
  }

  Serial.printf("SmallStep.loadConfig: EEPROM configuration is loaded\n");
}

void playBeep(bool success, uint16_t duration) {
  uint16_t freq = (success) ? BEEP_FREQ_SUCCESS : BEEP_FREQ_FAILURE;
  if (cfg.playBeep) sb.beep(BEEP_VOLUME, freq, duration);
}

void setup() {
  // set the CPU frequency to 80 MHz
  if (getCpuFrequencyMhz() != CPU_FREQ_LOW) {
    setCpuFrequencyMhz(CPU_FREQ_LOW);
  }

  // start the serial port
  Serial.begin(SERIAL_SPEED);

  // initialize the configuration variable
  memcpy(&cfg, &DEFAULT_CONFIG, sizeof(appconfig_t));

  // initialize libraries
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setBrightness(LCD_BRIGHTNESS);
  M5.Lcd.fillScreen(BLACK);
  EEPROM.begin(sizeof(appconfig_t) + 1);
  SDcard.begin(GPIO_NUM_4, SD_ACCESS_SPEED);
  sb.init();

  // set the bluetooth event handler
  logger.setEventCallback(onBTStatusUpdate);

  // load the configuration data
  M5.update();
  bool resetConfig = ((bool)M5.BtnA.read());
  loadAppConfig(resetConfig);

  // draw the screen
  updateAppHint();
  ui.setAppIcon(ICON_APP, COLOR16(0, 64, 255));
  ui.setAppTitle(APP_NAME);
  ui.setBluetoothStatus(false);
  ui.setSDcardStatus((bool)SDcard.card()->sectorCount());
  ui.drawTitleBar();
  ui.setIdleCallback(&onAppInputIdle, IDLE_TIMEOUT);

  // if left button is pressed, clear the configuration
  if (cfg.firstRun) {
    // show the welcome message
    ui.drawDialogFrame("Hello :)");
    ui.drawDialogText(BLUE, 0, "SmallStep is a configuration and data down-");
    ui.drawDialogText(BLUE, 1, "loading utility for classic MTK GPS loggers.");
    ui.drawDialogText(BLACK, 2, "This software comes with NO WARRANTY.");
    ui.drawDialogText(BLACK, 3, "If you find any bugs, please report it.");
    ui.promptOk();

    // Notice for Holux M-241 users
    ui.drawDialogFrame("For Holux m-241 users");
    ui.drawDialogText(BLACK, 0, "Smallstep can be used for Holux m-241.");
    ui.drawDialogText(BLACK, 1, "Downloading log data, fix RTC time and");
    ui.drawDialogText(BLACK, 2, "clearing flash will work properly.");
    ui.drawDialogText(RED, 3, "However, changing log format and log mode");
    ui.drawDialogText(RED, 4, "do not work for M-241.");
    ui.promptOk();

    cfg.firstRun = false;
    saveAppConfig();
  }

  // enter the main menu (infinite loop in ui.openMainMenu)
  int8_t itemCount = (sizeof(menuMain) / sizeof(mainmenuitem_t));
  ui.openMainMenu(menuMain, itemCount);
}

void loop() {
  // power off the device if reaching here
  Serial.printf("SmallStep.loop: power off the device\n");
  SDcard.end();         // stop SD card access
  M5.Power.powerOFF();  // power off
}
