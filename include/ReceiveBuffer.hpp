#pragma once

#include <Arduino.h>
#define BP_BUFFER_SIZE 160

typedef struct bufferpage {
  char buf[BP_BUFFER_SIZE + 1];
  bufferpage *next;
} bufferpage_t;

class ReceiveBuffer {
 private:
  bufferpage_t *rPage;
  bufferpage_t *cPage;
  uint8_t ptr;
  uint32_t dLen;
  uint8_t cCnt;
  uint8_t cChk;
  uint8_t rChk;

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
  bool seek(const uint8_t clm);
  bool isChecksumCorrect();
  void print_d();
  bool match(const char *str);
};
