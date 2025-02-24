#include "NmeaBuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RB_CCNT_CHKSUM 100
#define NON_HEXCHAR_VAL 255
#define CHAR_CR '\r'
#define CHAR_LF '\n'

NmeaBuffer::NmeaBuffer() {
  // constructor: initialize member variables
  currentPage = allocatePage(&rootPage);
  clear();
}

NmeaBuffer::~NmeaBuffer() {
  // destuctor: release dynamically allocated buffer
  freePages(rootPage);
}

void NmeaBuffer::freePages(bufferpage_t *pg) {
  // release all pages from the given to the last
  // if NULL is given, do nothing
  bufferpage_t *cp = pg;
  while (cp != NULL) {
    bufferpage_t *np = cp->next;
    free(cp);
    cp = np;
  }
}

bufferpage_t *NmeaBuffer::allocatePage(bufferpage_t **pg) {
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

char *NmeaBuffer::getBuffer() {
  return rootPage->buf;
}

void NmeaBuffer::clear() {
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

bool NmeaBuffer::isValidChar(char ch) {
  // return true if the given chars is usable in PMTK sentence
  return ((ch >= ' ') && (ch <= '~'));
}

uint8_t NmeaBuffer::hexCharToByte(char ch) {
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

void NmeaBuffer::appendToBuffer(char ch) {
  if (ptr < BP_BUFFER_SIZE) {
    currentPage->buf[ptr] = ch;
    ptr += 1;
  }

  if (ptr == BP_BUFFER_SIZE) {
    currentPage = allocatePage(&(currentPage->next));
    ptr = 0;
  }
}

void NmeaBuffer::updateChecksum(char ch) {
  if (columnCount < RB_CCNT_CHKSUM) {  // calculate checksum of sentence text
    if ((ch != '$') && (ch != '*')) {  // exclude '$' and '*' from checksum
      expectedChecksum ^= (byte)ch;
    }
  } else {  // store received checksum value
    receivedChecksum = (receivedChecksum << 4) + hexCharToByte(ch);
  }
}

bool NmeaBuffer::put(char ch) {
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

  if (isValidChar(ch)) {
    // append given char (when buffer available), then
    // calculate checksum of a receiving sentence or extract checksum value
    appendToBuffer(ch);
    updateChecksum(ch);

    dataLength += 1;
  }

  return ((ch == CHAR_LF) && (expectedChecksum == receivedChecksum));
}

char NmeaBuffer::get() {
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

bool NmeaBuffer::readColumnAsInt(uint8_t clm, int32_t *value, bool hex) {
  if (seekCurToColumn(clm) == false) return false;

  *value = 0;
  for (int8_t i = 0; i < 32; i++) {
    char ch = get();
    if ((ch == ',') || (ch == '*') || (ch == 0)) break;

    if (!hex) {
      *value = (*value * 10) + (ch - '0');
    } else {
      *value = (*value << 4) + hexCharToByte(ch);
    }
  }

  return true;
}

bool NmeaBuffer::readHexByteFull(byte *by) {
  byte upper = hexCharToByte(get());
  byte lower = hexCharToByte(get());

  if ((upper == NON_HEXCHAR_VAL) || (lower == NON_HEXCHAR_VAL)) {
    return false;
  }

  *by = (upper << 4) + lower;

  return true;
}

bool NmeaBuffer::readHexByteHalf(byte *by) {
  byte lower = hexCharToByte(get());

  if (lower == NON_HEXCHAR_VAL) {
    return false;
  }

  *by = lower;

  return true;
}

bool NmeaBuffer::match(const char *str) {
  return (strstr(rootPage->buf, str) != NULL);
}

bool NmeaBuffer::seekCurToColumn(uint8_t clm) {
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

bool NmeaBuffer::seekCur(uint32_t sk) {
  for (int i = 0; i < sk; i++) {
    if (get() == 0) return false;
  }

  return true;
}
