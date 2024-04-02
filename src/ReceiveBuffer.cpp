#include "ReceiveBuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RB_CCNT_CHKSUM 100
#define NON_HEXCHAR_VAL 255
#define CHAR_CR '\r'
#define CHAR_LF '\n'

ReceiveBuffer::ReceiveBuffer() {
  // constructor: initialize member variables
  currentPage = allocatePage(&rootPage);
  clear();
}

ReceiveBuffer::~ReceiveBuffer() {
  // destuctor: release dynamically allocated buffer
  freePages(rootPage);
}

void ReceiveBuffer::freePages(bufferpage_t *pg) {
  // release all pages from the given to the last
  // if NULL is given, do nothing
  bufferpage_t *cp = pg;
  while (cp != NULL) {
    bufferpage_t *np = cp->next;
    free(cp);
    cp = np;
  }
}

bufferpage_t *ReceiveBuffer::allocatePage(bufferpage_t **pg) {
  // create a new page and fill it with zero
  bufferpage_t *np = (bufferpage_t *)malloc(sizeof(bufferpage_t));
  memset(np, 0, sizeof(bufferpage_t));

  // assign the page to the given argment (if it is NOT NULL)
  if (pg != NULL) {
    *pg = np;
  }

  // return pointer of the new page
  return np;
}

void ReceiveBuffer::clear() {
  // release second and subsequent pages, zero-fill the first page
  freePages(rootPage->next);
  memset(rootPage, 0, sizeof(bufferpage_t));

  // clear member variables
  currentPage = rootPage;
  ptr = 0;
  dataLength = 0;
  columnCount = 0;
  expectedChecksum = 0;
  receivedChecksum = 0;
}

bool ReceiveBuffer::isTextChar(char ch) {
  // return true if the given chars is usable in PMTK sentence
  return ((ch >= ' ') && (ch <= '~'));
}

uint8_t ReceiveBuffer::hexCharToByte(char ch) {
  switch (ch) {
    case '0' ... '9':  // return 0-9 for '0'-'9'
      return (ch - '0');
    case 'a' ... 'f':  // return 10-16 for 'a'-'f'
      return (ch - 'a' + 10);
    case 'A' ... 'F':  // return 10-16 for 'A'-'F'
      return (ch - 'A' + 10);
  }

  // return zero for others ([$*,\r\n] and others)
  return NON_HEXCHAR_VAL;
}

void ReceiveBuffer::appendToBuffer(char ch) {
  if (ptr < BP_BUFFER_SIZE) {
    currentPage->buf[ptr] = ch;
    ptr += 1;
  }

  if (ptr == BP_BUFFER_SIZE) {
    currentPage = allocatePage(&(currentPage->next));
    ptr = 0;
  }
}

void ReceiveBuffer::updateChecksum(char ch) {
  if (columnCount < RB_CCNT_CHKSUM) {  // calculate checksum of sentence text
    if ((ch != '$') && (ch != '*')) {  // exclude '$' and '*' from checksum
      expectedChecksum ^= (byte)ch;
    }
  } else {  // store received checksum value
    receivedChecksum = (receivedChecksum << 4) + hexCharToByte(ch);
  }
}

bool ReceiveBuffer::put(char ch) {
  switch (ch) {
    case '$':  // begining of a new sentense
      clear();
      break;
    case ',':  // delimiter of columns in a sentense
      columnCount += 1;
      break;
    case '*':  // begining of checksum field
      columnCount = RB_CCNT_CHKSUM;
      break;
  }

  if (isTextChar(ch)) {
    // append given char (when buffer available), then
    // calculate checksum of a receiving sentence or extract checksum value
    appendToBuffer(ch);
    updateChecksum(ch);

    dataLength += 1;
  }

  return ((ch == CHAR_LF) && (expectedChecksum == receivedChecksum));
}

char ReceiveBuffer::get() {
  char ch = currentPage->buf[ptr];

  if ((ch != 0) && (ptr < BP_BUFFER_SIZE)) {
    ptr += 1;

    if ((ptr == BP_BUFFER_SIZE) && (currentPage->next != NULL)) {
      currentPage = currentPage->next;
      ptr = 0;
    }
  }

  return ch;
}

bool ReceiveBuffer::readColumnAsInt(uint8_t clm, int32_t *num) {
  if (seekToColumn(clm) == false) return false;

  uint8_t b = 0;
  int32_t n = 0;

  while (readHexByteHalf(&b)) {
    n = (n << 4) + b;
  }
  *num = n;

  return true;
}

bool ReceiveBuffer::readHexByteFull(byte *by) {
  byte upper = hexCharToByte(get());
  byte lower = hexCharToByte(get());

  if ((upper == NON_HEXCHAR_VAL) || (lower == NON_HEXCHAR_VAL)) {
    return false;
  }

  *by = (upper << 4) + lower;

  return true;
}

bool ReceiveBuffer::readHexByteHalf(byte *by) {
  byte lower = hexCharToByte(get());

  if (lower == NON_HEXCHAR_VAL) {
    return false;
  }

  *by = lower;

  return true;
}

bool ReceiveBuffer::match(const char *str) {
  return (strstr(rootPage->buf, str) != NULL);
}

bool ReceiveBuffer::seekToColumn(uint8_t clm) {
  bufferpage_t *pg = rootPage;
  uint16_t pt = 0;
  uint16_t cc = 0;

  while ((cc < clm) && (pg != NULL)) {
    for (pt = 0; pt < BP_BUFFER_SIZE; pt++) {
      if ((cc == clm) || (pg->buf[pt] == 0)) {
        break;
      }

      switch (pg->buf[pt]) {
        case ',':
          cc += 1;
          break;
        case '*':
          cc = RB_CCNT_CHKSUM;
          break;
      }
    }

    if (cc < clm) {
      pg = pg->next;
    }
  }

  if (cc == clm) {
    currentPage = pg;
    ptr = pt;
    columnCount = cc;
  }

  return (cc == clm);
}
