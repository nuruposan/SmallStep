#pragma once

#include <SdFat.h>

class MtkFileReader {
 private:
  static const uint16_t PAGE_SIZE = 4096;  // >= 512 (MtkParser.HEADER_SIZE)
  static const uint16_t PAGE_CENTER = (PAGE_SIZE / 2);
  static const uint16_t BUF_SIZE = (PAGE_SIZE * 2);

  /*
   * Note:
   * Time taken to convert 231,760 bytes of log data that includes 4 TRKs and 7455 TRKPTs
   *
   * CPU freq: 80 MHz, SD card (SPI) 12 MHz, SD card: Sillicon Power 8 GB Class 10
   * PAGE_SIZE 512  -> 27.46 sec
   * PAGE_SIZE 1024 -> 24.97 sec (9% faster than set PAGE_SIZE 512)
   * PAGE_SIZE 2048 -> 24.96 sec (9% faster)
   * PAGE_SIZE 4096 -> 23.34 sec (15% faster)
   * PAGE_SIZE 8192 -> 22.88 sec (17% faster)
   */

  char buf[BUF_SIZE];
  File32 *in;
  uint32_t cpos;
  uint32_t mpos;

 public:
  MtkFileReader(File32 *input);

  uint8_t checksum();
  uint32_t filesize();
  float getFloat();
  float getFloat24();
  uint32_t jump();
  uint32_t mark();
  uint32_t position();
  void readBytes(void *dst, uint8_t len);
  uint32_t seekCur(uint16_t mv);
};
