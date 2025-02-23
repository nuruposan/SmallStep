#pragma once

typedef enum _fmtreg {
  FMT_TIME = 0x00000001,    // type: uint32
  FMT_VALID = 0x00000002,   // type: uint16
  FMT_LAT = 0x00000004,     // type: double or float(M-241)
  FMT_LON = 0x00000008,     // type: double or float(M-241)
  FMT_HEIGHT = 0x00000010,  // type: float or float24(M-241)
  FMT_SPEED = 0x00000020,   // type: float
  FMT_TRACK = 0x00000040,   // type: float
  FMT_DSTA = 0x00000080,    // type: uint16
  FMT_DAGE = 0x00000100,    // type: float
  FMT_PDOP = 0x00000200,    // type: uint16
  FMT_HDOP = 0x00000400,    // type: uint16
  FMT_VDOP = 0x00000800,    // type: uint16
  FMT_NSAT = 0x00001000,    // type: uint16
  FMT_SID = 0x00002000,     // uint32
  FMT_ELE = 0x00004000,     // int16
  FMT_AZI = 0x00008000,     // uint16
  FMT_SNR = 0x00010000,     // uint16
  FMT_RCR = 0x00020000,     // uint16
  FMT_MSEC = 0x00040000,    // uint16
  FMT_DIST = 0x00080000,    // double
  FMT_FIXONLY = 0x80000000
} formatreg_t;

typedef struct _gpsrecord {
  uint32_t format;   // log format (set of formatreg_t values)
  uint32_t time;     // timestamp (unixtime in second)
  double latitude;   // degree of latitude (N: positive, S: negative)
  double longitude;  // degree of longigude (E: positive, W: negative)
  float altitude;    // altutude abobe sea level (meter)
  float speed;       // moving speed (km/h)
  uint16_t reason;   // record resson (dist/time/speed/manual)
  bool valid;        // valid record flag
  uint16_t size;     // record size in BIN file
} gpsrecord_t;
