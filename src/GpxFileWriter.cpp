#include "GpxFileWriter.h"

GpxFileWriter::GpxFileWriter(File32 *output) {
  out = output;
  out->seek(0);

  inTrack = false;
  inSegment = false;
  tracks = 0;
  points = 0;

  startXml();
}

void GpxFileWriter::flush() {
  out->flush();
}

void GpxFileWriter::startTrack(char *trkname) {
  if (inTrack) endTrack();

  out->write("<trk>\n");
  if (trkname != NULL) {
    char buf[64];
    out->write(sprintf(buf, "<name>%s</name>\n", trkname));
  }
  startTrackSegment();

  inTrack = true;
  tracks += 1;
}

void GpxFileWriter::startTrackSegment() {
  out->write("<trkseg>\n");
  inSegment = true;
}

void GpxFileWriter::startXml() {
  out->write(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<gpx version=\"1.1\" creator=\"SmallStep(M5Stack) v20240515\""
      " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema\""
      " xmlns=\"https://www.topografix.com/GPX/1/1/gpx.xsd\">\n");
  inTrack = false;
}

void GpxFileWriter::endTrack() {
  if (inTrack) {
    endTrackSegment();
    out->write("</trk>\n");
  }

  inTrack = false;
}

void GpxFileWriter::endTrackSegment() {
  if (inSegment) out->write("</trkseg>\n");

  inSegment = false;
}

void GpxFileWriter::endXml() {
  if (inTrack) endTrack();
  out->write("</gpx>\n");
}

void GpxFileWriter::putTrkpt(gpsrecord_t rcd) {
  char buf[48];

  if (!inTrack) {
    if (rcd.format & REG_TIME) {
      time_t tt = rcd.time;
      struct tm *t = localtime(&tt);
      sprintf(buf, "Start at %04d-%02d-%02d %02d:%02d:%02d", (t->tm_year + 1900), (t->tm_mon + 1),
              t->tm_mday, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

      startTrack(buf);
    } else {
      startTrack(NULL);
    }
  }

  out->write("<trkpt");
  if (rcd.format & REG_LAT) {
    sprintf(buf, " lat=\"%0.6f\"", rcd.latitude);
    out->write(buf);
  }
  if (rcd.format & REG_LON) {
    sprintf(buf, " lon=\"%0.6f\"", rcd.longitude);
    out->write(buf);
  }
  out->write(">");

  if (rcd.format & REG_TIME) {
    time_t tt = rcd.time;
    struct tm *t = localtime(&tt);

    sprintf(buf, "<time>%04d-%02d-%02dT%02d:%02d:%02dZ</time>", (t->tm_year + 1900),
            (t->tm_mon + 1), t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    out->write(buf);
  }

  if (rcd.format & REG_ELE) {
    sprintf(buf, "<ele>%0.2f</ele>", rcd.elevation);
    out->write(buf);
  }

  if (rcd.format & REG_SPEED) {
    sprintf(buf, "<speed>%0.2f</speed>", rcd.speed);
    out->write(buf);
  }

  out->write("</trkpt>\n");

  points += 1;
}

int32_t GpxFileWriter::getTrackCount() {
  return tracks;
}

int32_t GpxFileWriter::getTrkptCount() {
  return points;
}
