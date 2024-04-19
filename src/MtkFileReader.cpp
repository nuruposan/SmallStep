#include "MtkFileReader.h"

MtkFileReader::MtkFileReader(File32 *input) {
  // constructor
  memset(buf, 0xFF, BUF_SIZE);

  in = input;
  in->seek(0);
  in->readBytes(buf, PAGE_SIZE);
  pos = 0;
  ptr = 0;
}

void MtkFileReader::read(void *p, int8_t len) {
  int8_t *dst = (int8_t *)p;
  int16_t pt = ptr;

  for (int8_t i = 0; i < len; i++) {
    *dst++ = buf[pt++ % BUF_SIZE];
  }
  seekCur(len);
}

double MtkFileReader::readDouble() {
  double val = 0;
  read(&val, sizeof(double));

  return val;
}

float MtkFileReader::readFloat24() {
  float val = 0;
  int8_t *pval = (int8_t *)(&val) + 1;  // {0x00, 0xXX, 0xXX, 0xXX}
  read(pval, (sizeof(int8_t) * 3));

  return val;
}

float MtkFileReader::readFloat() {
  float val = 0;
  read(&val, sizeof(float));

  return val;
}

int8_t MtkFileReader::readInt8() {
  int8_t val = 0;
  read(&val, sizeof(int8_t));

  return val;
}

int16_t MtkFileReader::readInt16() {
  int16_t val = 0;
  read(&val, sizeof(int16_t));

  return val;
}

int32_t MtkFileReader::readInt32() {
  int32_t val = 0;
  read(&val, sizeof(int32_t));

  return val;
}

uint32_t MtkFileReader::seekCur(uint16_t mv) {
  uint16_t _ptr = ptr;
  pos += mv;
  ptr = pos % BUF_SIZE;

  if (((pos + PAGE_CENTER) >= in->position()) &&
      (in->position() < in->size())) {
    uint16_t pg = (_ptr < PAGE_SIZE) ? PAGE_SIZE : 0;
    memset(&buf[pg], 0xFF, PAGE_SIZE);
    if (in->position() < in->size()) in->readBytes(&buf[pg], PAGE_SIZE);
  }

  return pos;
}

uint32_t MtkFileReader::setMark() {
  mpos = pos;
  return mpos;
}

uint32_t MtkFileReader::moveToMark() {
  pos = mpos;
  ptr = pos % BUF_SIZE;
  return pos;
};

uint32_t MtkFileReader::position() { return pos; }

uint32_t MtkFileReader::filesize() { return in->size(); }
