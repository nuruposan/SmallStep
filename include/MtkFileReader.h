#pragma once

#include <SdFat.h>

class MtkFileReader {
 private:
  static const uint16_t PAGE_SIZE = 1024;  // >= 512 (MtkParser.HEADER_SIZE)
  static const uint16_t PAGE_CENTER = (PAGE_SIZE / 2);
  static const uint16_t BUF_SIZE = (PAGE_SIZE * 2);

  char buf[BUF_SIZE];
  File32 *in;
  uint32_t pos;
  uint16_t ptr;
  uint32_t mpos;

  void read(void *dst, int8_t len);

 public:
  MtkFileReader(File32 *input);

  uint32_t moveToMark();
  uint32_t filesize();
  uint32_t setMark();
  uint32_t position();
  float readFloat();
  float readFloat24();
  double readDouble();
  int8_t readInt8();
  int16_t readInt16();
  int32_t readInt32();
  uint32_t seekCur(uint16_t mv);
};
