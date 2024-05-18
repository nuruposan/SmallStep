#pragma once

#include <SdFat.h>

#include "CommonTypes.h"
#include "GpxFileWriter.h"
#include "MtkFileReader.h"

typedef enum _btnid {
  DSP_CHANGE_FORMAT = 2,
  DSP_AUTOLOG_SECOND = 3,
  DSP_AUTOLOG_METER = 4,
  DSP_AUTOLOG_SPEED = 5,
  DSP_CHANGE_METHOD = 6,
  DSP_LOG_STARTSTOP = 7
} dspid_t;

typedef enum _trkopt {
  TRK_ONE_DAY = 0,  // divide trasks by local date (default)
  TRK_AS_IS = 1,    // put tracks as recorded
  TRK_SINGLE = 2    // put all trkpts into a single track
} trackmode_t;

typedef struct _parseopt {
  trackmode_t trackMode;
  float timeOffset;
} parseopt_t;

typedef struct _parsest {
  uint16_t sectorPos;
  uint32_t formatReg;
  uint8_t ignoreLen1;
  uint8_t ignoreLen2;
  uint8_t ignoreLen3;
  uint8_t ignoreLen4;
  bool m241Mode;
  gpsrecord_t firstTrkpt;
  gpsrecord_t lastTrkpt;
  uint32_t trackCount;
  uint32_t trkptCount;
} parsestatus_t;

class MtkParser {
 private:
  const uint32_t SIZE_SECTOR = 0x010000;  // 65536 bytes
  const uint32_t SIZE_HEADER = 0x000200;  // 512 bytes

  const uint32_t ROLLOVER_TIME = 1554595200;    // 2019-04-07
  const uint32_t ROLLOVER_CORRECT = 619315200;  // 1024 weeks

  const char PTN_DSET_AA[7] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
  const char PTN_DSET_BB[4] = {0xBB, 0xBB, 0xBB, 0xBB};
  const char PTN_M241[16] = {'H', 'O', 'L', 'U', 'X', 'G', 'R', '2',
                             '4', '1', 'L', 'O', 'G', 'G', 'E', 'R'};
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
  void setRecordFormat(uint32_t fmt);

 public:
  MtkParser();
  void setOptions(parseopt_t);
  bool convert(File32 *input, File32 *output, void (*callback)(int32_t));
  gpsrecord_t getFirstTrkpt();
  gpsrecord_t getLastTrkpt();
  uint32_t getTrackCount();
  uint32_t getTrkptCount();
};
