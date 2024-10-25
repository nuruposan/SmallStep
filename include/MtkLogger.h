#pragma once

#include <BluetoothSerial.h>
#include <EEPROM.h>
#include <MtkParser.h>
#include <SdFat.h>

#include "NmeaBuffer.h"

typedef enum _sizeinfo {
  SIZE_REPLY = 0x000800,
  SIZE_8MBIT = 0x100000,
  SIZE_16MBIT = 0x200000,
  SIZE_32MBIT = 0x400000,
} sizeinfo_t;

typedef enum _recordmode {
  MODE_NONE = 0,       // no record mode (error state)
  MODE_OVERWRITE = 1,  // overwrite oldest record when flash is full
  MODE_FULLSTOP = 2,   // stop logging when flash is full
} recordmode_t;

typedef struct _logcriteria {
  int16_t distance;
  int16_t time;
  int16_t speed;
} logcriteria_t;

class MtkLogger {
 private:
  const uint32_t TIMEOUT = 1000;
  const int32_t ACK_WAIT = 100;

  const char *deviceName;
  char address[6];
  bool sppStarted;
  BluetoothSerial *gpsSerial;
  NmeaBuffer *buffer;
  esp_spp_cb_t eventCallback;

  uint8_t calcNmeaChecksum(const char *cmd);
  int32_t resetCache(File32 *cache);
  bool sendNmeaCommand(const char *cmd);
  bool waitForNmeaReply(const char *reply, uint16_t timeout);
  bool sendDownloadCommand(int startPos, int reqSize);
  static int32_t modelIdToFlashSize(uint16_t modelId);
  bool getLastRecordAddress(int32_t *size);

 public:
  MtkLogger(const char *devname);
  ~MtkLogger();

  bool clearFlash(void (*rateCallback)(int32_t, int32_t));
  bool connect(String name);
  bool connect(uint8_t *address);
  bool connected();
  void disconnect();
  bool downloadLogData(File32 *output, void (*rateCallback)(int32_t, int32_t));
  bool fixRTCdatetime();
  bool getFlashSize(int32_t *size);
  bool getLogByDistance(int16_t *dist);
  bool getLogBySpeed(int16_t *speed);
  bool getLogByTime(int16_t *time);
  bool getLogCriteria(logcriteria_t *criteria);
  bool getLogFormat(uint32_t *format);
  bool getLogRecordMode(recordmode_t *recmode);
  bool reloadDevice();
  bool setLogByDistance(int16_t distance);
  bool setLogBySpeed(int16_t speed);
  bool setLogByTime(int16_t time);
  bool setLogCriteria(logcriteria_t criteria);
  bool setLogFormat(uint32_t format);
  bool setLogRecordMode(recordmode_t recmode);
  void setEventCallback(esp_spp_cb_t evtCallback);
};
