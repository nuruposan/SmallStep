
#include "LoggerManager.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LoggerManager::LoggerManager() {
  memset(&address, 0, sizeof(address));
  memset(&pinCode, 0, sizeof(pinCode));
};

LoggerManager::~LoggerManager() {
  if (connected == true) {
    // TODO: disconnect from GPS logger
  }
}

bool LoggerManager::connect(char addr[6]) { return false; }

void LoggerManager::disconnect() {}

byte LoggerManager::calcChecksum(char *cmd, byte len) {
  byte chk = 0;
  for (byte i = 0; i < len; i++) {
    chk ^= (byte)(cmd[i]);
  }

  return chk;
}

bool LoggerManager::sendCommand(char *cmd, byte len) {
  int len2 = (int)(len) + 8;
  char buf[len2];

  unsigned char chk = calcChecksum(cmd, len);

  memset(buf, 0, len2);
  sprintf(buf, "$%s*%02X\r\n", cmd, chk);

  for (short i = 0; (i < len2) && (buf[i] != 0); i++) {
    //    SerialBT.write(buf[i]);
  }

  return true;
}
