#include "MtkFileReader.h"

MtkFileReader::MtkFileReader(File32 *input) {
  // constructor
  memset(buf, 0xFF, BUF_SIZE);

  in = input;
  in->seek(0);
  in->readBytes(buf, PAGE_SIZE);

  cpos = 0;
  mpos = 0;
}

void MtkFileReader::read(void *p, uint8_t len) {
  int8_t *dst = (int8_t *)p;

  for (uint32_t i = cpos; i < (cpos + len); i++) {
    *dst++ = buf[i % BUF_SIZE];  // copy value
  }
  seek(len);
}

void MtkFileReader::read(void *p, uint8_t offset, uint8_t len) {
  int8_t *dst = (int8_t *)p + offset;

  read(dst, len);
}

uint32_t MtkFileReader::seek(uint16_t mv) {
  uint16_t pt = cpos % BUF_SIZE;
  cpos += mv;

  if (((cpos + PAGE_CENTER) >= in->position()) && (in->position() < in->size())) {
    uint16_t pg = (pt < PAGE_SIZE) ? PAGE_SIZE : 0;

    memset(&buf[pg], 0xFF, PAGE_SIZE);
    if (in->position() < in->size()) {
      in->readBytes(&buf[pg], PAGE_SIZE);
    }
  }

  return cpos;
}

uint32_t MtkFileReader::mark() {
  mpos = cpos;  // store the marking position

  return mpos;
}

uint8_t MtkFileReader::checksum() {
  uint8_t chk = 0;
  uint16_t pt = mpos % BUF_SIZE;
  uint16_t len = cpos - mpos;

  for (uint32_t i = mpos; i < (mpos + len); i++) {
    chk ^= buf[i % BUF_SIZE];
  }

  return chk;
}

uint32_t MtkFileReader::jump() {
  cpos = mpos;

  return cpos;
};

uint32_t MtkFileReader::position() {
  return cpos;
}

uint32_t MtkFileReader::filesize() {
  return in->size();
}
