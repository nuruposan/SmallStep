#pragma once

#include <SdFat.h>
#include <stdlib.h>

#include "CommonTypes.h"

#define PARSER_DESCR "SmallStep/M5Stack v20250829"
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

  gpxinfo_t gpxInfo;
  gpxinfo_t trackInfo;
  gpsrecord_t waypts[MAX_WAYPTS_PER_TRK];

  int32_t offsetSec;
  bool inGpx;
  bool inTrack;
  bool inTrkSeg;

  void beginGpx();
  void beginTrack();
  void beginTrackSeg();
  void putWaypt(gpsrecord_t rcd);
  void putTrackPoint(gpsrecord_t rcd, bool asWpt);
  void putLatLon(gpsrecord_t rcd);
  void putHeight(gpsrecord_t rcd);
  void putSpeed(gpsrecord_t rcd);
  void putTime(gpsrecord_t rcd);

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
