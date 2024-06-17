#include "GpxFileWriter.h"

GpxFileWriter::GpxFileWriter(File32 *output) {
  out = output;
  out->seek(0);

  inTrack = false;
  inSegment = false;
  trackCount = 0;
  trkptCount = 0;
  wayptCount = 0;
  memset(waypts, 0, (sizeof(gpsrecord_t) * MAX_WAYPTS_PER_TRK));

  startXml();
}

int16_t GpxFileWriter::addWaypt(gpsrecord_t rcd) {
  int16_t i = 0;
  while ((waypts[i].time != 0) && i < MAX_WAYPTS_PER_TRK) i++;

  if (i < MAX_WAYPTS_PER_TRK) {
    memcpy(&waypts[i], &rcd, sizeof(gpsrecord_t));
  }

  return (i < MAX_WAYPTS_PER_TRK) ? (i + 1) : 0;
}

void GpxFileWriter::flush() {
  out->flush();
}

void GpxFileWriter::startTrack(char *trkname) {
  if (inTrack) endTrack();

  out->write("<trk>\n");
  if (trkname != NULL) {
    char buf[64];
    sprintf(buf, "<name>%s</name>\n", trkname);
    out->write(buf);
  }

  inTrack = true;
  inSegment = false;
  trackCount += 1;
}

void GpxFileWriter::startTrackSegment() {
  out->write("<trkseg>\n");
  inSegment = true;
}

void GpxFileWriter::startXml() {
  out->write(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<gpx version=\"1.1\" creator=\"" PARSER_DESCR
      "\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema xmlns=\"https://www.topografix.com/GPX/1/1/gpx.xsd\">\n");

  inTrack = false;
  inSegment = false;
  trackCount = 0;
  trkptCount = 0;
  wayptCount = 0;
  memset(waypts, 0, (sizeof(gpsrecord_t) * MAX_WAYPTS_PER_TRK));
}

void GpxFileWriter::endTrack() {
  if (inTrack) {
    if (inSegment) endTrackSegment();

    endTrackSegment();
    out->write("</trk>\n");

    for (int16_t i = 0; i < MAX_WAYPTS_PER_TRK; i++) {
      if (waypts[i].time == 0) break;

      putWaypt(waypts[i]);
      memset(&waypts[i], 0, sizeof(gpsrecord_t));
    }
  }

  inSegment = false;
  inTrack = false;
}

void GpxFileWriter::endTrackSegment() {
  if (inSegment) out->write("</trkseg>\n");

  inSegment = false;
}

void GpxFileWriter::endXml() {
  if (inTrack) endTrack();

  out->write("</gpx>\n");

  inTrack = false;
  inSegment = false;
}

void GpxFileWriter::putTrkpt(const gpsrecord_t rcd) {
  char buf[48];

  if (!inTrack) {
    if (rcd.format & FMT_TIME) {
      time_t tt = rcd.time;
      struct tm *t = localtime(&tt);
      sprintf(buf, "Track started at %04d-%02d-%02d %02d:%02d:%02d",  //
              (t->tm_year + 1900), (t->tm_mon + 1), t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

      startTrack(buf);
    } else {
      startTrack(NULL);
    }
  }
  if (!inSegment) startTrackSegment();

  out->write("<trkpt");
  {
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
      time_t tt = rcd.time;
      struct tm *t = localtime(&tt);

      sprintf(buf, "<time>%04d-%02d-%02dT%02d:%02d:%02dZ</time>",  //
              (t->tm_year + 1900), (t->tm_mon + 1), t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
      out->write(buf);
    }

    if (rcd.format & FMT_HEIGHT) {
      sprintf(buf, "<ele>%0.2f</ele>", rcd.elevation);
      out->write(buf);
    }

    if (rcd.format & FMT_SPEED) {
      sprintf(buf, "<speed>%0.2f</speed>", rcd.speed);
      out->write(buf);
    }
  }
  out->write("</trkpt>\n");

  trkptCount += 1;
}

void GpxFileWriter::putWaypt(const gpsrecord_t rcd) {
  char buf[48];

  out->write("<wpt");
  {
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
      time_t tt = rcd.time;
      struct tm *t = localtime(&tt);

      sprintf(buf, "<time>%04d-%02d-%02dT%02d:%02d:%02dZ</time>",  //
              (t->tm_year + 1900), (t->tm_mon + 1), t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
      out->write(buf);
    }

    if (rcd.format & FMT_HEIGHT) {
      sprintf(buf, "<ele>%0.2f</ele>", rcd.elevation);
      out->write(buf);
    }

    if (rcd.format & FMT_SPEED) {
      sprintf(buf, "<speed>%0.2f</speed>", rcd.speed);
      out->write(buf);
    }
  }
  out->write("</wpt>\n");

  wayptCount += 1;
}

int32_t GpxFileWriter::getTrackCount() {
  return trackCount;
}

int32_t GpxFileWriter::getTrkptCount() {
  return trkptCount;
}

int32_t GpxFileWriter::getWayptCount() {
  return wayptCount;
}
