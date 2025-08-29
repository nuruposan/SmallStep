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

    // update the number of WAYPTs in the GPX data
    gpxInfo.wayptCount += 1;
    trackInfo.wayptCount += 1;
  }

  return (i < MAX_WAYPTS_PER_TRK) ? (i + 1) : -1;
}

void GpxFileWriter::beginTrack() {
  if (!inGpx) beginGpx();
  if (inTrack) endTrack();

  out->write("<trk>\n");

  // set the flag to indicate the GPX track is started
  inTrack = true;
  inTrkSeg = false;
  memset(waypts, 0, (sizeof(gpsrecord_t) * MAX_WAYPTS_PER_TRK));  // clear the waypt list

  // update the track count in the GPX data
  gpxInfo.trackCount += 1;
  memset(&trackInfo, 0, sizeof(gpxinfo_t));  // clear the track information
}

void GpxFileWriter::beginTrackSeg() {
  if (!inTrack) beginTrack();
  if (inTrkSeg) endTrackSeg();

  out->write("<trkseg>\n");

  // set the flag to indicate the GPX track segment is started
  inTrkSeg = true;

  // update the segment count in the current track
  trackInfo.trackCount += 1;
}

void GpxFileWriter::beginGpx() {
  if (inGpx) return;

  // truncate the existing data in the output file
  out->truncate(0);

  // write the header of the GPX data
  out->write(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<gpx version=\"1.1\" creator=\"" PARSER_DESCR
      "\""
      " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema\""
      " xmlns=\"https://www.topografix.com/GPX/1/1/gpx.xsd\">\n");

  // initialize the GPX and track information
  memset(&gpxInfo, 0, sizeof(gpxinfo_t));
  memset(&trackInfo, 0, sizeof(gpxinfo_t));

  // set the flag to indicate the GPX data is started
  inGpx = true;
  inTrack = false;
  inTrkSeg = false;
}

void GpxFileWriter::endTrack() {
  if (!inTrack) return;

  // close the track segment (if it is opened)
  endTrackSeg();

  // put the name of the track if there is any track data
  if (trackInfo.trkptCount > 0) {
    char buf[64];

    uint32_t duration = (trackInfo.endTime - trackInfo.startTime);
    uint16_t drHour = duration / 3600;
    uint16_t drMin = (duration % 3600) / 60;

    // put the track data summary as the track name
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

  // close the track
  out->write("</trk>\n");

  // put the waypts added in the last track
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
  if (!inGpx) return gpxInfo;

  // close the track (if it is opened)
  endTrack();

  // put the name of the GPX data if there is any track data
  if (gpxInfo.trkptCount > 0) {
    char buf[64];

    uint32_t duration = (gpxInfo.endTime - gpxInfo.startTime);
    uint16_t drDay = duration / (24 * 3600);
    uint16_t drHour = (duration % (24 * 3600)) / 3600;
    uint16_t drMin = (duration % 3600) / 60;

    // put the track data summary as the gpx name
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

  // close the GPX data
  out->write("</gpx>\n");
  out->flush();

  // set the flag to indicate the GPX data is ended
  inGpx = false;
  inTrack = false;
  inTrkSeg = false;

  return gpxInfo;
}

void GpxFileWriter::putTrackPoint(gpsrecord_t rcd, bool asWpt) {
  const char *tag = (asWpt) ? "wpt" : "trkpt";

  if (asWpt) {
    Serial.printf("%s, (%.3f %.3f)\n", tag, rcd.latitude, rcd.longitude);
  }

  out->write("<");
  out->write(tag);
  putLatLon(rcd);
  out->write(">");

  putTime(rcd);
  putHeight(rcd);
  putSpeed(rcd);

  out->write("</");
  out->write(tag);
  out->write(">\n");
}

void GpxFileWriter::putLatLon(gpsrecord_t rcd) {
  char buf[32];

  if (rcd.format & FMT_LAT) {
    sprintf(buf, " lat=\"%.6f\"", rcd.latitude);
    out->write(buf);
  }
  if (rcd.format & FMT_LON) {
    sprintf(buf, " lon=\"%.6f\"", rcd.longitude);
    out->write(buf);
  }
}

void GpxFileWriter::putHeight(gpsrecord_t rcd) {
  char buf[32];

  if (rcd.format & FMT_HEIGHT) {
    sprintf(buf, "<ele>%.2f</ele>", rcd.altitude);
    out->write(buf);
  }
}

void GpxFileWriter::putSpeed(gpsrecord_t rcd) {
  char buf[32];

  if (rcd.format & FMT_SPEED) {
    sprintf(buf, "<speed>%.2f</speed>", rcd.speed);
    out->write(buf);
  }
}

void GpxFileWriter::putTime(gpsrecord_t rcd) {
  char buf[32];

  if (rcd.format & FMT_TIME) {
    out->write("<time>");
    out->write(timeToISO8601(buf, rcd.time));
    out->write("</time>");
  }
}

void GpxFileWriter::putTrkpt(const gpsrecord_t rcd) {
  if (!inTrkSeg) beginTrackSeg();

  // put a TRKPT
  putTrackPoint(rcd, false);

  // update the start/end time of the GPX data and the track
  if (rcd.format & FMT_TIME) {
    if (gpxInfo.startTime == 0) gpxInfo.startTime = rcd.time;
    gpxInfo.endTime = rcd.time;
    if (trackInfo.startTime == 0) trackInfo.startTime = rcd.time;
    trackInfo.endTime = rcd.time;
  }

  // update the number of TRKPTs
  gpxInfo.trkptCount += 1;
  trackInfo.trkptCount += 1;
}

void GpxFileWriter::putWaypt(gpsrecord_t rcd) {
  if (!inGpx) beginGpx();

  // put a WPT
  putTrackPoint(rcd, true);

  // Note: never update the number of WAYPTs here (addWaypt)
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

  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",              // "YYYY-MM-DD hh:mm:ss"
          (t->tm_year + 1900), (t->tm_mon + 1), t->tm_mday,  //
          t->tm_hour, t->tm_min, t->tm_sec);                 //

  return buf;
}

char *GpxFileWriter::timeToISO8601(char *buf, uint32_t gpsTime) {
  time_t tt = gpsTime;
  struct tm *t = localtime(&tt);

  sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02dZ",             // "YYYY-MM-DDThh:mm:ssZ"
          (t->tm_year + 1900), (t->tm_mon + 1), t->tm_mday,  //
          t->tm_hour, t->tm_min, t->tm_sec);                 //

  return buf;
}
