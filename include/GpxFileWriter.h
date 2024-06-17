#pragma once

#include <SdFat.h>

#include "CommonTypes.h"

#define PARSER_DESCR "SmallStep(M5Stack) v20240618"
#define MAX_WAYPTS_PER_TRK 250

class GpxFileWriter {
 private:
  File32 *out;
  bool inXml;
  bool inTrack;
  bool inSegment;
  int32_t trackCount;
  int32_t trkptCount;
  int32_t wayptCount;
  gpsrecord_t waypts[MAX_WAYPTS_PER_TRK];

  void putWaypt(gpsrecord_t rcd);
  void startTrack(char *name);
  void startTrackSegment();
  void startXml();

 public:
  GpxFileWriter(File32 *output);

  int16_t addWaypt(gpsrecord_t rcd);
  void flush();
  void endTrack();
  void endTrackSegment();
  void endXml();
  void putTrkpt(gpsrecord_t rcd);
  int32_t getTrackCount();
  int32_t getTrkptCount();
  int32_t getWayptCount();
};
