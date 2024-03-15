#pragma once

#include <SD.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum _trackmode {
  TRK_PUT_AS_IS = 0,
  TRK_ONE_DAY = 1,
  TRK_SINGLE = 2
} trackmode_t;

typedef struct _parseopt {
  trackmode_t track;
  float offset;
} parseopt_t;

typedef struct _gpsrecord {
  uint32_t format;
  uint32_t time;
  double latitude;
  double longitude;
  float elevation;
  float speed;
  bool valid;
} gpsrecord_t;

typedef struct _parseinfo {
  uint16_t sector;
  uint32_t format;
  bool m241;
  gpsrecord_t firstRecord;
  gpsrecord_t lastRecord;
  bool newTrack;
  bool inTrack;
} parseinfo_t;

class MtkParser {
 private:
  static const uint32_t SIZE_HEADER;
  static const uint32_t SIZE_SECTOR;

  static const uint32_t ROLLOVER_OCCUR_TIME;
  static const uint32_t ROLLOVER_CORRECT_VALUE;

  static const char PTN_DSET_AA[];
  static const char PTN_DSET_BB[];
  static const char PTN_M241[];
  static const char PTN_M241_SP[];
  static const char PTN_SCT_END[];

  File *in;
  File *out;
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
  static void printFormat_d(uint32_t fmt);
  static void printRecord_d(gpsrecord_t *rcd);
  void putGpxString(const char *line);
  void putGpxXmlStart();
  void putGpxXmlEnd();
  void putGpxTrackStart();
  void putGpxTrackEnd();
  void putGpxTrackPoint(gpsrecord_t *rcd);
  void setFormatRegister(uint32_t fmt);

 public:
  MtkParser();
  ~MtkParser();
  float setTimezone(float offset);
  bool convert(File *input, File *output, void (*callback)(int32_t));
  uint32_t getFirstRecordTime();
  uint32_t getLastRecordTime();
};
