#pragma once

#include <SdFat.h>

#include "CommonTypes.h"
#include "GpxFileWriter.h"
#include "MtkFileReader.h"

#define SIZE_SECTOR 0x010000  // 65536 bytes
#define SIZE_HEADER 0x000200  // 512 bytes

#define ROLLOVER_TIME 1554595200    // 2019-04-07
#define ROLLOVER_CORRECT 619315200  // 1024 weeks

typedef enum _dspid {
  DST_CHANGE_FORMAT = 2,
  DST_AUTOLOG_TIME = 3,
  DST_AUTOLOG_DIST = 4,
  DST_AUTOLOG_SPEED = 5,
  DST_CHANGE_METHOD = 6,
  DST_LOG_STARTSTOP = 7
} dspid_t;

typedef enum _dsvlog {
  DSV_LOG_START = 0x0106,
  DSV_LOG_STOP = 0x0104,
} dsvlog_t;

typedef enum _rcrval {
  RCR_LOG_BY_TIME = 0b0001,   //
  RCR_LOG_BY_DIST = 0b0010,   //
  RCR_LOG_BY_SPEED = 0b0100,  //
  RCR_LOG_BY_USER = 0b1000,   //
} rcrval_t;

typedef enum _trkopt {
  TRK_ONE_DAY = 0,  // divide trasks by local date (default)
  TRK_AS_IS = 1,    // divide tracks by start/stop logging
  TRK_SINGLE = 2    // put all trkpts into a single track
} trackmode_t;

typedef struct _parseopt {
  trackmode_t trackMode;
  float timeOffset;
  bool putWaypts;
} parseopt_t;

typedef struct _parsestatus {
  uint16_t sectorPos;
  uint32_t logFormat;
  bool m241Mode;
  uint8_t seekOffset1;
  uint8_t seekOffset2;
  uint8_t seekOffset3;
  uint8_t seekOffset4;
} parsestatus_t;

class MtkParser {
 private:
  const char PTN_DSET_AA[7] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
  const char PTN_DSET_BB[4] = {0xBB, 0xBB, 0xBB, 0xBB};
  const char PTN_M241[16] = {'H', 'O', 'L', 'U', 'X', 'G', 'R', '2', '4', '1', 'L', 'O', 'G', 'G', 'E', 'R'};
  const char PTN_M241_SP[4] = {' ', ' ', ' ', ' '};
  const char PTN_SCT_END[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  MtkFileReader *in;
  GpxFileWriter *out;
  parseopt_t options;
  parsestatus_t status;

  bool isDifferentDate(uint32_t t1, uint32_t t2);
  bool matchBinPattern(const char *ptn, uint8_t len);
  bool readBinMarkers();
  bool readBinRecord(gpsrecord_t *rcd);
  void setOptions(parseopt_t opts);
  void setRecordFormat(uint32_t fmt);

 public:
  MtkParser(parseopt_t opts);
  gpxinfo_t convert(File32 *input, File32 *output, void (*rateCallback)(int32_t, int32_t));
};
