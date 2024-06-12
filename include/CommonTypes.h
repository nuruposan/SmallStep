#pragma once

typedef enum _fmtreg {
  FMT_TIME    = 0x00000001, // uint32
  FMT_VALID   = 0x00000002, // uint16
  FMT_LAT     = 0x00000004, // double / float(M-241)
  FMT_LON     = 0x00000008, // double / float(M-241)
  FMT_HEIGHT  = 0x00000010, // float / float24(M-241)
  FMT_SPEED   = 0x00000020, // float
  FMT_TRACK   = 0x00000040, // float
  FMT_DSTA    = 0x00000080, // uint16
  FMT_DAGE    = 0x00000100, // float
  FMT_PDOP    = 0x00000200, // uint16
  FMT_HDOP    = 0x00000400, // uint16
  FMT_VDOP    = 0x00000800, // uint16
  FMT_NSAT    = 0x00001000, // uint16
  FMT_SID     = 0x00002000, // uint32 (SIV)
  FMT_ELE     = 0x00004000, // int16 
  FMT_AZI     = 0x00008000, // uint16 
  FMT_SNR     = 0x00010000, // uint16 
  FMT_RCR     = 0x00020000, // uint16
  FMT_MSEC    = 0x00040000, // uint16 
  FMT_DIST    = 0x00080000, // double
  FMT_FIXONLY = 0x80000000
} formatreg_t;

typedef struct _gpsrecord {
  uint32_t format;
  uint32_t time;
  double latitude;
  double longitude;
  float elevation;
  float speed;
  uint16_t reason;
  bool valid;
  uint16_t size;
} gpsrecord_t;
