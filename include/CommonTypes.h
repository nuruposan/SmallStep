#pragma once

typedef enum _fmtreg {
  REG_TIME  = 0x00000001, // uint32
  REG_VALID = 0x00000002, // uint16
  REG_LAT   = 0x00000004, // double / float(M-241)
  REG_LON   = 0x00000008, // double / float(M-241)
  REG_ELE   = 0x00000010, // float / float24(M-241)
  REG_SPEED = 0x00000020, // float
  REG_HEAD  = 0x00000040, // float
  REG_DSTA  = 0x00000080, // uint16
  REG_DAGE  = 0x00000100, // float
  REG_PDOP  = 0x00000200, // uint16
  REG_HDOP  = 0x00000400, // uint16
  REG_VDOP  = 0x00000800, // uint16
  REG_NSAT  = 0x00001000, // uint16
  REG_SID   = 0x00002000, // uint32 (SIV)
  REG_ALT   = 0x00004000, // int16 
  REG_AZI   = 0x00008000, // uint16 
  REG_SNR   = 0x00010000, // uint16 
  REG_RCR   = 0x00020000, // uint16
  REG_MSEC  = 0x00040000, // uint16 
  REG_DIST  = 0x00080000, // double
  REG_FIX   = 0x80000000  
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
