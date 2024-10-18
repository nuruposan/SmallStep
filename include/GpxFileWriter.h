#pragma once

#include <SdFat.h>
#include <stdlib.h>

#include "CommonTypes.h"

#define PARSER_DESCR "SmallStep(M5Stack) v20241025"
#define MAX_WAYPTS_PER_TRK 250

typedef struct _gpxinfo {
  uint32_t startTime;  // start time of a GPX file or a track
  uint32_t endTime;    // end time of a GPX or a track
  int32_t trkptCount;  //
  int32_t trackCount;  // 
  int32_t wayptCount;  // a number of waypts  
} gpxinfo_t;

class GpxFileWriter {
 private:
  File32 *out;

  int32_t timeOffsetSec;
  char timeOffsetStr[8];  // [+-]\d\d:\d\d

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

  int16_t addWaypt(gpsrecord_t rcd);
  void flush();
  void endTrack();
  void endTrackSeg();
  void endGpx();
  int32_t getTrackCount();
  int32_t getTrkptCount();
  int32_t getWayptCount();
  void putTrkpt(gpsrecord_t rcd);
  void setTimeOffset(float tz);
  char *timeToString(char *buf, uint32_t gpsTime);
  char *timeToISO8601(char *buf, uint32_t gpsTime);
};
