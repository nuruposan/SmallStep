#include "MtkFileReader.hpp"

MtkFileReader::MtkFileReader() {
  // constructor
  memset(buf, 0xFF, BUF_SIZE);
}

MtkFileReader::~MtkFileReader() {
  // destructor
}

double MtkFileReader::readDouble() {
  double val;
  int8_t ba[] = {read(), read(), read(), read(),
                 read(), read(), read(), read()};
  memcpy(&val, &ba, sizeof(double));

  return val;
}

float MtkFileReader::readFloat24() {
  float val;
  int8_t ba[] = {0x00, read(), read(), read()};
  memcpy(&val, &ba, sizeof(float));

  return val;
}

float MtkFileReader::readFloat() {
  float val;
  int8_t ba[] = {read(), read(), read(), read()};
  memcpy(&val, &ba, sizeof(float));

  return val;
}

int32_t MtkFileReader::readInt8() {
  //
  return read();
}

int32_t MtkFileReader::readInt16() {
  int32_t val;
  int8_t ba[] = {read(), read(), 0x00, 0x00};
  memcpy(&val, &ba, sizeof(int16_t));

  return val;
}

int32_t MtkFileReader::readInt32() {
  int32_t val;
  int8_t ba[] = {read(), read(), read(), read()};
  memcpy(&val, &ba, sizeof(int32_t));

  return val;
}

void MtkFileReader::init(File32 *f) {
  in = f;

  in->seek(0);
  in->readBytes(buf, PAGE_SIZE);
  pos = 0;
  ptr = 0;
}

int8_t MtkFileReader::read() {
  char ret = buf[ptr];
  seekCur(1);

  return ret;
}

uint32_t MtkFileReader::seekCur(uint16_t mv) {
  uint16_t _ptr = ptr;
  pos += mv;
  ptr = pos % BUF_SIZE;

  if ((pos + PAGE_CENTER) >= in->position()) {
    uint16_t pg = (_ptr < PAGE_SIZE) ? PAGE_SIZE : 0;
    memset(&buf[pg], 0xFF, PAGE_SIZE);
    if (in->position() < in->size()) in->readBytes(&buf[pg], PAGE_SIZE);
  }

  return pos;
}

uint32_t MtkFileReader::markPos() {
  mpos = pos;
  return mpos;
}

uint32_t MtkFileReader::backToMarkPos() {
  pos = mpos;
  ptr = pos % BUF_SIZE;
  return pos;
};

uint32_t MtkFileReader::position() { return pos; }

uint32_t MtkFileReader::filesize() { return in->size(); }
