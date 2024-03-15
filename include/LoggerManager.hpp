#pragma once

#include <BluetoothSerial.h>
#include <SD.h>

#include "ReceiveBuffer.hpp"

typedef enum _sizeinfo {
  SIZE_REPLY = 0x00000800,
  SIZE_8MBIT = 0x00100000,
  SIZE_16MBIT = 0x00200000,
  SIZE_32MBIT = 0x00400000
} sizeinfo_t;

class LoggerManager {
 private:
  char address[6];
  bool sppStarted;
  BluetoothSerial *gpsSerial;
  ReceiveBuffer *buffer;

  uint8_t calcNmeaChecksum(const char *cmd, uint8_t len);
  bool sendNmeaCommand(const char *cmd, uint16_t len);
  bool sendDownloadCommand(int startPos, int reqSize);
  static int32_t modelIdToFlashSize(uint16_t modelId);
  bool getLogEndAddress(int32_t *size);

 public:
  LoggerManager();
  ~LoggerManager();

  bool connect(uint8_t *address);
  bool connected();
  void disconnect();
  bool downloadLogData(File *output, void (*callback)(int));
  bool fixRTCdatetime();
  int32_t clearFlashMemory();
  // int32_t setLogMode(bool overrite);
  // int32_t setLogByDistance(int16_t distance);
  // int32_t setLogBySpeed(int16_t speed);
  // int32_t setLogByTime(int16_t time);
};
