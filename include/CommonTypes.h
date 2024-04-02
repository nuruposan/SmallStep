#pragma once

typedef enum _fmtreg {
  REG_TIME = 0x000001,   // uint32_
  REG_VALID = 0x000002,  // uint16
  REG_LAT = 0x000004,    // double / float(M-241)
  REG_LON = 0x000008,    // double / float(M-241)
  REG_ELE = 0x000010,    // float / float24(M-241)
  REG_SPEED = 0x000020,  // float
  REG_TRACK = 0x000040,  // float
  REG_DSTA = 0x000080,   // uint16
  REG_DAGE = 0x000100,   // float
  REG_PDOP = 0x000200,   // uint16
  REG_HDOP = 0x000400,   // uint16
  REG_VDOP = 0x000800,   // uint16
  REG_NSAT = 0x001000,   // uint16
  REG_SID = 0x002000,    // uint32 (SIV)
  REG_ALT = 0x004000,    // int16
  REG_AZI = 0x008000,    // uint16
  REG_SNR = 0x010000,    // uint16
  REG_RCR = 0x020000,    // uint16
  REG_MSEC = 0x040000    // uint16
} formatreg_t;

typedef struct _gpsrecord {
  uint32_t format;
  uint32_t time;
  double latitude;
  double longitude;
  float elevation;
  float speed;
  bool valid;
  uint8_t size;
} gpsrecord_t;
