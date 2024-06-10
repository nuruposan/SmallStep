#pragma once

#include <BluetoothSerial.h>
#include <EEPROM.h>
#include <SdFat.h>

#include "NmeaBuffer.h"

typedef enum _sizeinfo {
  SIZE_REPLY = 0x00000800,
  SIZE_8MBIT = 0x00100000,
  SIZE_16MBIT = 0x00200000,
  SIZE_32MBIT = 0x00400000
} sizeinfo_t;

typedef enum _recordmode {
  MODE_NONE = 0,      // no record mode (error state)
  MODE_OVERWRITE = 1, // overwrite oldest record when flash is full
  MODE_FULLSTOP = 2,  // stop logging when flash is full
} recordmode_t;

class MtkLogger {
 private:
  char address[6];
  bool sppStarted;
  BluetoothSerial *gpsSerial;
  NmeaBuffer *buffer;
  esp_spp_cb_t eventCallback;

  uint8_t calcNmeaChecksum(const char *cmd);
  bool sendNmeaCommand(const char *cmd);
  bool waitForNmeaReply(const char *reply, uint16_t timeout);
  bool sendDownloadCommand(int startPos, int reqSize);
  static int32_t modelIdToFlashSize(uint16_t modelId);
  bool getLastRecordAddress(int32_t *size);

 public:
  MtkLogger();
  ~MtkLogger();

  bool clearFlash(void (*rateCallback)(int));
  bool connect(String name);
  bool connect(uint8_t *address);
  bool connected();
  void disconnect();
  bool downloadLogData(File32 *output, void (*rateCallback)(int));
  bool fixRTCdatetime();
  bool getFlashSize(int32_t *size);
  //bool getLogByDistance(int16_t distance);
  //bool getLogBySpeed(int16_t speed);
  //bool getLogByTime(int16_t time);
  bool getLogFormat(uint32_t *format);
  bool getLogRecordMode(recordmode_t *recmode);
  bool reloadDevice();
  // int32_t setLogByDistance(int16_t distance);
  // int32_t setLogBySpeed(int16_t speed);
  // int32_t setLogByTime(int16_t time);
  uint32_t setLogFormat(uint32_t format);
  bool setLogRecordMode(recordmode_t recmode);
  void setEventCallback(esp_spp_cb_t evtCallback);
};
