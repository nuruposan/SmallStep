#pragma once

#include <Arduino.h>

#define BP_BUFFER_SIZE 512

typedef struct bufferpage {
  char buf[BP_BUFFER_SIZE + 1];
  bufferpage *next;
} bufferpage_t;

class ReceiveBuffer {
 private:
  bufferpage_t *rootPage;
  bufferpage_t *currentPage;
  uint16_t ptr;
  uint32_t dataLength;
  uint8_t columnCount;
  uint8_t expectedChecksum;
  uint8_t receivedChecksum;

  static bool isTextChar(char ch);
  static uint8_t hexCharToByte(char ch);
  bufferpage_t *allocatePage(bufferpage_t **pg);
  void freePages(bufferpage_t *tgp);
  void appendToBuffer(char ch);
  void updateChecksum(char ch);

 public:
  ReceiveBuffer();
  ~ReceiveBuffer();
  void clear();
  bool put(const char ch);
  char get();
  bool readHexByteFull(byte *by);
  bool readHexByteHalf(byte *by);
  bool readColumnAsInt(uint8_t clm, int32_t *num);
  bool seekToColumn(uint8_t clm);
  bool match(const char *str);
};