#pragma once

#include <SdFat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum _trackmode {
  TRK_ONE_DAY = 0,  // divide trasks by local date (default)
  TRK_AS_IS = 1,    // put tracks as recorded
  TRK_SINGLE = 2    // put all trkpts into a single track
} trackmode_t;

typedef struct _parseopt {
  trackmode_t trackMode;
  float timeOffset;
} parseopt_t;

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

typedef struct _parseinfo {
  uint16_t sectorId;
  uint32_t recordFormat;
  uint8_t ignoreLength1;
  uint8_t ignoreLength2;
  bool m241Mode;
  //  bool newTrack;
  bool inTrack;
  gpsrecord_t firstRecord;
  gpsrecord_t lastRecord;
  uint32_t totalTracks;
  uint32_t totalRecords;
} parseinfo_t;

class MtkParser {
 private:
  static const uint32_t SIZE_HEADER;
  static const uint32_t SIZE_SECTOR;

  static const uint32_t ROLLOVER_TIME;
  static const uint32_t ROLLOVER_CORRECT;

  static const char PTN_DSET_AA[];
  static const char PTN_DSET_BB[];
  static const char PTN_M241[];
  static const char PTN_M241_SP[];
  static const char PTN_SCT_END[];

  File32 *in;
  File32 *out;
  parseopt_t options;
  parseinfo_t progress;

  bool isDifferentDate(uint32_t t1, uint32_t t2);
  bool matchBinPattern(const char *ptn, uint8_t len);
  double readBinDouble();
  float readBinFloat24();
  float readBinFloat();
  int32_t readBinInt8();
  int32_t readBinInt16();
  int32_t readBinInt32();
  bool readBinMarkers();
  bool readBinRecord(gpsrecord_t *rcd);
  void putGpxString(const char *line);
  void putGpxXmlStart();
  void putGpxXmlEnd();
  void putGpxTrackStart();
  void putGpxTrackEnd();
  void putGpxTrackPoint(gpsrecord_t *rcd);
  void setRecordFormat(uint32_t fmt);

 public:
  MtkParser();
  ~MtkParser();
  float setTimeOffset(float offset);
  bool convert(File32 *input, File32 *output, void (*callback)(int32_t));
  uint32_t getFirstRecordTime();
  uint32_t getLastRecordTime();
};
