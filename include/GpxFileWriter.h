#pragma once

#include <SdFat.h>
#include <stdlib.h>

#include "CommonTypes.h"

#define PARSER_DESCR "SmallStep/M5Stack v20241019"
#define MAX_WAYPTS_PER_TRK 250

typedef struct _gpxinfo {
  uint32_t startTime;
  uint32_t endTime;
  int32_t trkptCount;  //
  int32_t trackCount;  //
  int32_t wayptCount;  // a number of waypts
} gpxinfo_t;

class GpxFileWriter {
 private:
  File32 *out;

  int32_t offsetSec;

  gpxinfo_t gpxInfo;
  gpxinfo_t trackInfo;

  bool inGpx;
  bool inTrack;
  bool inTrkSeg;
  gpsrecord_t waypts[MAX_WAYPTS_PER_TRK];

  void beginGpx();
  void beginTrack();
  void beginTrackSeg();
  void putWaypt(gpsrecord_t rcd);

 public:
  GpxFileWriter(File32 *output);
  ~GpxFileWriter();

  int16_t addWaypt(gpsrecord_t rcd);
  void endTrack();
  void endTrackSeg();
  gpxinfo_t endGpx();
  uint32_t getLastTime();
  void putTrkpt(gpsrecord_t rcd);
  void setTimeOffset(float tz);
  char *timeToString(char *buf, uint32_t gpsTime);
  char *timeToISO8601(char *buf, uint32_t gpsTime);
};
