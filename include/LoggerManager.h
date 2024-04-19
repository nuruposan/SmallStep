#pragma once

#include <BluetoothSerial.h>
#include <EEPROM.h>
#include <SdFat.h>

#include "ReceiveBuffer.h"

#define MY_ESP_SPP_START_EVT 98
#define MY_ESP_SPP_STOP_EVT 99

typedef enum _sizeinfo {
  SIZE_REPLY = 0x00000800,
  SIZE_8MBIT = 0x00100000,
  SIZE_16MBIT = 0x00200000,
  SIZE_32MBIT = 0x00400000
} sizeinfo_t;

typedef enum _returncode {
  RC_SUCCESS = 3  // success
} returncode_t;

typedef enum _recordmode {
  MODE_FULLSTOP = 2,  // stop logging when flash is full
  MODE_OVERWRITE = 1  // overwrite oldest record when flash is full
} recordmode_t;

typedef enum _logformat {
  LOG_LAT = 0x00000001,    // latitude
  LOG_LON = 0x00000010,    // longitude
  LOG_TIME = 0x00000100,   // time (unixtime)
  LOG_MSEC = 0x00001000,   // time (msec)
  LOG_ELEV = 0x00010000,   // elevation
  LOG_SPEED = 0x00100000,  // speed
  LOG_HEAD = 0x01000000,   // heading
  LOG_FIX = 0x10000000,    // record reason (button/distance/speed/time)
  LOG_RCR = 0x100000000    // GPS status (no-fix/sps/dgps/estimated)
} logformat_t;

class LoggerManager {
 private:
  char address[6];
  bool sppStarted;
  BluetoothSerial *gpsSerial;
  ReceiveBuffer *buffer;
  esp_spp_cb_t eventCallback;

  uint8_t calcNmeaChecksum(const char *cmd);
  bool sendNmeaCommand(const char *cmd);
  bool sendDownloadCommand(int startPos, int reqSize);
  static int32_t modelIdToFlashSize(uint16_t modelId);
  bool getLastRecordAddress(int32_t *size);

 public:
  LoggerManager();
  ~LoggerManager();

  bool connect(String name);
  bool connect(uint8_t *address);
  bool discover(const char *name, esp_spp_cb_t callback);
  bool connected();
  void disconnect();
  bool downloadLog(File32 *output, void (*callback)(int32_t));
  bool fixRTCdatetime();
  bool clearFlash(void (*callback)(int32_t));
  // int32_t setLogMode(bool overrite);
  // int32_t setLogByDistance(int16_t distance);
  // int32_t setLogBySpeed(int16_t speed);
  // int32_t setLogByTime(int16_t time);
  void setEventCallback(esp_spp_cb_t callback);
};
