#pragma once

#include <Arduino.h>

#define BP_BUFFER_SIZE 512

typedef struct bufferpage {
  char buf[BP_BUFFER_SIZE + 1];
  bufferpage *next;
} bufferpage_t;

class NmeaBuffer {
 private:
  bufferpage_t *rootPage;
  bufferpage_t *currentPage;
  uint16_t ptr;
  uint32_t dataLength;
  uint8_t columnCount;
  uint8_t expectedChecksum;
  uint8_t receivedChecksum;

  static bool isValidChar(char ch);
  static uint8_t hexCharToByte(char ch);
  bufferpage_t *allocatePage(bufferpage_t **pg);
  void freePages(bufferpage_t *tgp);
  void appendToBuffer(char ch);
  void updateChecksum(char ch);

 public:
  NmeaBuffer();
  ~NmeaBuffer();

  void clear();
  bool put(const char ch);
  char get();
  char *getBuffer();
  bool readHexByteFull(byte *by);
  bool readHexByteHalf(byte *by);
  bool readColumnAsInt(uint8_t clm, int32_t *value, bool hex);
  bool seekCurToColumn(uint8_t clm);
  bool seekCur(uint32_t sk);
  bool match(const char *str);
};
