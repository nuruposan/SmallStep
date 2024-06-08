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
  MODE_FULLSTOP = 2,  // stop logging when flash is full
  MODE_OVERWRITE = 1  // overwrite oldest record when flash is full
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
  bool sendDownloadCommand(int startPos, int reqSize);
  static int32_t modelIdToFlashSize(uint16_t modelId);
  bool getLastRecordAddress(int32_t *size);

 public:
  MtkLogger();
  ~MtkLogger();

  bool connect(String name);
  bool connect(uint8_t *address);
  bool connected();
  void disconnect();
  bool downloadLog(File32 *output, void (*rateCallback)(int));
  bool fixRTCdatetime();
  bool getFlashSize(int32_t *size);
  bool clearFlash(void (*rateCallback)(int));
  // int32_t setLogMode(bool overrite);
  // int32_t setLogByDistance(int16_t distance);
  // int32_t setLogBySpeed(int16_t speed);
  // int32_t setLogByTime(int16_t time);
  void setEventCallback(esp_spp_cb_t evtCallback);
};
