#pragma once

#include <BluetoothSerial.h>

typedef enum _flashsize {
  SIZE_8MBIT = 0x00100000,
  SIZE_16MBIT = 0x00200000,
  SIZE_32MBIT = 0x00400000,
  SIZE_DEFAULT = 0x00400000
} flashsize_t;

typedef enum _recordmethod {
  RECORD_OVERWRAP = 1,
  RECORD_FULLSTOP = 2
} recordmethod_t;

typedef enum _resultcode { RESULT_FAILED = 2, RESULT_SUCCESS = 3 } resultcode_t;

class LoggerManager {
 private:
  static const long SIZE_LOG_REPLY = 0x00000800;  // 2 Kbyte (FIXED)

  // ReceiveBuffer *buffer;
  bool connected;
  char pinCode[8];
  char address[6];
  BluetoothSerial serial;

  unsigned char calcChecksum(char *cmd, unsigned char len);
  bool sendCommand(char *cmd, unsigned char len);

 public:
  LoggerManager();
  ~LoggerManager();
  bool connect(char *address);
  void disconnect();
  bool isConnected();
  bool isSerialReady();
  //    bool isReplyReady();
  bool extractColumn(unsigned char column, char *buf, unsigned int len);
  unsigned char readSerial();
};
