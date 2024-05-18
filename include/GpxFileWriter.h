#pragma once

#include <SdFat.h>

#include "CommonTypes.h"

class GpxFileWriter {
 private:
  File32 *out;
  bool inXml;
  bool inTrack;
  bool inSegment;
  int16_t tracks;
  int32_t points;

  void startTrack(char *name);
  void startTrackSegment();
  void startXml();

 public:
  GpxFileWriter(File32 *output);

  void flush();
  void endTrack();
  void endTrackSegment();
  void endXml();
  void putTrkpt(gpsrecord_t rcd);
  int32_t getTrackCount();
  int32_t getTrkptCount();
};
