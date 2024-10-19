#include "GpxFileWriter.h"

GpxFileWriter::GpxFileWriter(File32 *output) {
  out = output;

  setTimeOffset(0);  // set offsetSec as 0 and timeOffsetStr as "Z"

  inGpx = false;
  inTrack = false;
  inTrkSeg = false;
}

GpxFileWriter::~GpxFileWriter() {
  out->flush();
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
    char buf[64];

    uint32_t duration = (trackInfo.endTime - trackInfo.startTime);
    uint16_t drHour = duration / 3600;
    uint16_t drMin = (duration % 3600) / 60;

    out->write("<name>");
    out->write(timeToString(buf, trackInfo.startTime));
    out->write(" to ");
    out->write(timeToString(buf, trackInfo.endTime));
    if (trackInfo.wayptCount == 0) {
      sprintf(buf, " (%d hours %d minutes, %d TRKPTs)",  //
              drHour, drMin, trackInfo.trkptCount);
    } else {
      sprintf(buf, " (%d hours %d minutes, %d TRKPTs, %d WAYPTs)",  //
              drHour, drMin, trackInfo.trkptCount, trackInfo.wayptCount);
    }
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

gpxinfo_t GpxFileWriter::endGpx() {
  if (inGpx) {
    endTrack();

    if (gpxInfo.trkptCount > 0) {
      char buf[64];

      uint32_t duration = (gpxInfo.endTime - gpxInfo.startTime);
      uint16_t drDay = duration / (24 * 3600);
      uint16_t drHour = (duration % (24 * 3600)) / 3600;
      uint16_t drMin = (duration % 3600) / 60;

      out->write("<name>");
      out->write(timeToString(buf, gpxInfo.startTime));
      out->write(" to ");
      out->write(timeToString(buf, gpxInfo.endTime));
      if (gpxInfo.wayptCount == 0) {
        sprintf(buf, " (%d days %d hours %d minutes, %d TRKs, %d TRKPTs)",  //
                drDay, drHour, drMin, gpxInfo.trackCount, gpxInfo.trkptCount);
      } else {
        sprintf(buf, " (%d days %d hours %d minutes, %d TRKs, %d TRKPTs, %d WAYPTs)",  //
                drDay, drHour, drMin, gpxInfo.trackCount, gpxInfo.trkptCount, gpxInfo.wayptCount);
      }
      out->write(buf);
      out->write("</name>\n");
    }

    out->write("</gpx>\n");
    out->flush();
  }

  inGpx = false;
  inTrack = false;
  inTrkSeg = false;

  return gpxInfo;
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

uint32_t GpxFileWriter::getLastTime() {
  return gpxInfo.endTime;
}

void GpxFileWriter::setTimeOffset(float td) {
  offsetSec = 3600 * td;
}

char *GpxFileWriter::timeToString(char *buf, uint32_t gpsTime) {
  time_t tt = gpsTime + offsetSec;  // include the configured time offset
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
