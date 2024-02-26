#pragma once

#include <Arduino.h>
#define BP_BUFFER_SIZE 160

typedef struct bufferpage {
  char buf[BP_BUFFER_SIZE + 1];
  bufferpage *next;
} bufferpage_t;

class ReceiveBuffer {
 private:
  bufferpage_t *rootPage;
  bufferpage_t *currentPage;
  uint8_t ptr;
  uint32_t dataLength;
  uint8_t columnCount;
  uint8_t calculatedChecksum;
  uint8_t receivedChecksum;

  static bool isTextChar(const char ch);
  static uint8_t hexCharToByte(const char ch);
  bufferpage_t *allocatePage(bufferpage_t **pg);
  void freePages(bufferpage_t *tgp);
  void appendToBuffer(const char ch);
  void updateChecksum(const char ch);

 public:
  ReceiveBuffer();
  ~ReceiveBuffer();
  void clear();
  bool put(const char ch);
  char get();
  bool readHexByteFull(byte *by);
  bool readHexByteHalf(byte *by);
  bool readColumnAsInt(const uint8_t clm, int32_t *num);
  bool seekToColumn(const uint8_t clm);
  bool isChecksumCorrect();
  bool match(const char *str);
};
