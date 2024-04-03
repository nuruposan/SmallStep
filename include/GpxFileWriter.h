#pragma once

#include <SdFat.h>

#include "CommonTypes.h"

class GpxFileWriter {
 private:
  File32 *out;
  bool inTrack;
  int16_t tracks;
  int32_t points;

 public:
  GpxFileWriter(File32 *output);
  ~GpxFileWriter();

  void flush();
  void startTrack();
  void startXml();
  void endTrack();
  void endXml();
  void putTrkpt(gpsrecord_t rcd);
  int32_t getTrackCount();
  int32_t getTrkptCount();
};
