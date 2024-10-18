#include "GpxFileWriter.h"

GpxFileWriter::GpxFileWriter(File32 *output) {
  out = output;

  setTimeOffset(0);  // set timeOffsetSec as 0 and timeOffsetStr as "Z"

  inGpx = false;
  inTrack = false;
  inTrkSeg = false;
}

int16_t GpxFileWriter::addWaypt(gpsrecord_t rcd) {
  int16_t i = 0;

  // put data to the last of waypy list (array)
  while ((waypts[i].format != 0) && i < MAX_WAYPTS_PER_TRK) i++;
  if (i < MAX_WAYPTS_PER_TRK) {
    memcpy(&waypts[i], &rcd, sizeof(gpsrecord_t));

    gpxInfo.wayptCount += 1;
    trackInfo.wayptCount += 1;
  }

  return (i < MAX_WAYPTS_PER_TRK) ? (i + 1) : -1;
}

void GpxFileWriter::flush() {
  out->flush();
}

void GpxFileWriter::beginTrack() {
  if (!inGpx) beginGpx();
  if (inTrack) endTrack();

  out->write("<trk>\n");

  inTrack = true;
  inTrkSeg = false;

  gpxInfo.trackCount += 1;
  memset(&trackInfo, 0, sizeof(gpxinfo_t));

  Serial.printf("GpxFileWriter.beginTrack [trkCount=%d]\n", gpxInfo.trackCount);
}

void GpxFileWriter::beginTrackSeg() {
  if (!inTrack) beginTrack();
  if (inTrkSeg) endTrackSeg();

  out->write("<trkseg>\n");
  inTrkSeg = true;

  Serial.printf("GpxFileWriter.startSegment\n");
}

void GpxFileWriter::beginGpx() {
  if (inGpx) return;

  out->seek(0);
  // out->truncate(0);

  out->write(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<gpx version=\"1.1\" creator=\"" PARSER_DESCR
      "\""
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema xmlns=\"https://www.topografix.com/GPX/1/1/gpx.xsd\">\n");

  memset(&gpxInfo, 0, sizeof(gpxinfo_t));
  memset(&trackInfo, 0, sizeof(gpxinfo_t));

  inGpx = true;
  inTrack = false;
  inTrkSeg = false;
  memset(waypts, 0, (sizeof(gpsrecord_t) * MAX_WAYPTS_PER_TRK));
}

void GpxFileWriter::endTrack() {
  if (!inTrack) return;

  endTrackSeg();

  // put the start & end time of the track as the track name
  if (trackInfo.trkptCount > 0) {
    char buf[48];
    uint32_t duration = (trackInfo.endTime - trackInfo.startTime);
    uint16_t drHour = duration / 3600;
    uint16_t drMinute = (duration % 3600) / 60;

    out->write("<name>");
    out->write(timeToString(buf, trackInfo.startTime));
    out->write(" to ");
    out->write(timeToString(buf, trackInfo.endTime));
    sprintf(buf, " (%d hr %d min, %d TRKPTs)",  //
            drHour, drMinute, trackInfo.trkptCount, trackInfo.wayptCount);
    out->write(buf);
    out->write("</name>\n");
  }
  out->write("</trk>\n");

  // put the waypts discovered in the last track
  for (int16_t i = 0; i < MAX_WAYPTS_PER_TRK; i++) {
    if (waypts[i].time == 0) break;

    putWaypt(waypts[i]);
    memset(&waypts[i], 0, sizeof(gpsrecord_t));
  }

  inTrkSeg = false;
  inTrack = false;
}

void GpxFileWriter::endTrackSeg() {
  if (!inTrkSeg) return;

  out->write("</trkseg>\n");
  inTrkSeg = false;
}

void GpxFileWriter::endGpx() {
  if (!inGpx) return;

  endTrack();
  out->write("</gpx>\n");

  inGpx = false;
  inTrack = false;
  inTrkSeg = false;
}

void GpxFileWriter::putTrkpt(const gpsrecord_t rcd) {
  if (!inTrkSeg) beginTrackSeg();

  char buf[32];  // general buffer in this func

  out->write("<trkpt");
  if (rcd.format & FMT_LAT) {
    sprintf(buf, " lat=\"%0.6f\"", rcd.latitude);
    out->write(buf);
  }
  if (rcd.format & FMT_LON) {
    sprintf(buf, " lon=\"%0.6f\"", rcd.longitude);
    out->write(buf);
  }
  out->write(">");

  if (rcd.format & FMT_TIME) {
    out->write("<time>");
    out->write(timeToISO8601(buf, rcd.time));
    out->write("</time>");

    if (gpxInfo.startTime == 0) gpxInfo.startTime = rcd.time;
    gpxInfo.endTime = rcd.time;
    if (trackInfo.startTime == 0) trackInfo.startTime = rcd.time;
    trackInfo.endTime = rcd.time;
  }

  if (rcd.format & FMT_HEIGHT) {
    sprintf(buf, "<ele>%0.2f</ele>", rcd.elevation);
    out->write(buf);
  }

  if (rcd.format & FMT_SPEED) {
    sprintf(buf, "<speed>%0.2f</speed>", rcd.speed);
    out->write(buf);
  }

  out->write("</trkpt>\n");

  gpxInfo.trkptCount += 1;
  trackInfo.trkptCount += 1;
}

void GpxFileWriter::putWaypt(const gpsrecord_t rcd) {
  char buf[32];  // general buffer in this func

  if (!inGpx) beginGpx();
  if (inTrack) endTrack();

  out->write("<wpt");
  if (rcd.format & FMT_LAT) {
    sprintf(buf, " lat=\"%0.6f\"", rcd.latitude);
    out->write(buf);
  }

  if (rcd.format & FMT_LON) {
    sprintf(buf, " lon=\"%0.6f\"", rcd.longitude);
    out->write(buf);
  }
  out->write(">");

  if (rcd.format & FMT_TIME) {
    out->write("<time>");
    out->write(timeToISO8601(buf, rcd.time));
    out->write("</time>");
  }

  if (rcd.format & FMT_HEIGHT) {
    sprintf(buf, "<ele>%0.2f</ele>", rcd.elevation);
    out->write(buf);
  }

  if (rcd.format & FMT_SPEED) {
    sprintf(buf, "<speed>%0.2f</speed>", rcd.speed);
    out->write(buf);
  }
  out->write("</wpt>\n");

  gpxInfo.wayptCount += 1;
}

int32_t GpxFileWriter::getTrackCount() {
  return gpxInfo.trackCount;
}

int32_t GpxFileWriter::getTrkptCount() {
  return gpxInfo.trkptCount;
}

int32_t GpxFileWriter::getWayptCount() {
  return gpxInfo.wayptCount;
}

void GpxFileWriter::setTimeOffset(float td) {
  timeOffsetSec = 3600 * td;
  sprintf(timeOffsetStr, "%+02d:%02d", (timeOffsetSec / 3600), (abs(timeOffsetSec) % 3600));
}

char *GpxFileWriter::timeToString(char *buf, uint32_t gpsTime) {
  time_t tt = gpsTime + timeOffsetSec;  // include the configured time offset
  struct tm *t = localtime(&tt);

  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",              //
          (t->tm_year + 1900), (t->tm_mon + 1), t->tm_mday,  // YYYY-MM-DD
          t->tm_hour, t->tm_min, t->tm_sec);                 // hh:mm:ss

  return buf;
}

char *GpxFileWriter::timeToISO8601(char *buf, uint32_t gpsTime) {
  time_t tt = gpsTime;  // must not include the time offset
  struct tm *t = localtime(&tt);

  sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02dZ",             //
          (t->tm_year + 1900), (t->tm_mon + 1), t->tm_mday,  // YYYY-MM-DD
          t->tm_hour, t->tm_min, t->tm_sec);                 // hh:mm:ss

  return buf;
}
