#include "MtkParser.hpp"

typedef enum _fmtreg {
  REG_TIME = 0x000001,
  REG_VALID = 0x000002,
  REG_LAT = 0x000004,
  REG_LON = 0x000008,
  REG_ELE = 0x000010,
  REG_SPEED = 0x000020,
  REG_TRACK = 0x000040,
  REG_DSTA = 0x000080,
  REG_DAGE = 0x000100,
  REG_PDOP = 0x000200,
  REG_HDOP = 0x000400,
  REG_VDOP = 0x000800,
  REG_NSAT = 0x001000,
  REG_SID = 0x002000,
  REG_ALT = 0x004000,
  REG_AZI = 0x008000,
  REG_SNR = 0x010000,
  REG_RCR = 0x020000,
  REG_MSEC = 0x040000
} formatreg_t;

typedef enum _dspid {
  DSP_CHANGE_FORMAT = 2,
  DSP_AUTOLOG_SECOND = 3,
  DSP_AUTOLOG_METER = 4,
  DSP_AUTOLOG_SPEED = 5,
  DSP_CHANGE_METHOD = 6,
  DSP_LOG_STARTSTOP = 7
} dspid_t;

const uint32_t MtkParser::SIZE_HEADER =
    0x000200;  // 0x0000 to 0x0199 = 512 bytes
const uint32_t MtkParser::SIZE_SECTOR = 0x010000;  // 0x010000 = 65536 bytes

const uint32_t MtkParser::ROLLOVER_OCCUR_TIME = 1554595200;    // 2019-04-07
const uint32_t MtkParser::ROLLOVER_CORRECT_VALUE = 619315200;  // 1024 weeks

const char MtkParser::PTN_DSET_AA[] = {0xAA, 0xAA, 0xAA, 0xAA,
                                       0xAA, 0xAA, 0xAA};
const char MtkParser::PTN_DSET_BB[] = {0xBB, 0xBB, 0xBB, 0xBB};
const char MtkParser::PTN_M241[] = {'H', 'O', 'L', 'U', 'X', 'G', 'R', '2',
                                    '4', '1', 'L', 'O', 'G', 'G', 'E', 'R'};
const char MtkParser::PTN_M241_SP[] = {' ', ' ', ' ', ' '};
const char MtkParser::PTN_SCT_END[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                       0xFF, 0xFF, 0xFF, 0xFF};

MtkParser::MtkParser() {
  Serial.println("MtkParser.constructor\n");

  memset(&options, 0, sizeof(parseopt_t));
  memset(&progress, 0, sizeof(parseinfo_t));
}

MtkParser::~MtkParser() { Serial.println("MtkParser.destructor\n"); }

double MtkParser::readBinDouble() {
  double val;
  uint8_t ba[] = {(uint8_t)in->read(), (uint8_t)in->read(), (uint8_t)in->read(),
                  (uint8_t)in->read(), (uint8_t)in->read(), (uint8_t)in->read(),
                  (uint8_t)in->read(), (uint8_t)in->read()};
  memcpy(&val, &ba, sizeof(double));

  return val;
}

float MtkParser::readBinFloat24() {
  float val;
  uint8_t ba[] = {0x00, (uint8_t)in->read(), (uint8_t)in->read(),
                  (uint8_t)in->read()};
  memcpy(&val, &ba, sizeof(float));

  return val;
}

float MtkParser::readBinFloat() {
  float val;
  uint8_t ba[] = {(uint8_t)in->read(), (uint8_t)in->read(), (uint8_t)in->read(),
                  (uint8_t)in->read()};
  memcpy(&val, &ba, sizeof(float));

  return val;
}

int32_t MtkParser::readBinInt8() { return in->read(); }

int32_t MtkParser::readBinInt16() {
  int32_t val;
  uint8_t ba[] = {(uint8_t)in->read(), (uint8_t)in->read(), 0x00, 0x00};
  memcpy(&val, &ba, sizeof(int16_t));

  return val;
}

int32_t MtkParser::readBinInt32() {
  int32_t val;
  uint8_t ba[] = {(uint8_t)in->read(), (uint8_t)in->read(), (uint8_t)in->read(),
                  (uint8_t)in->read()};
  memcpy(&val, &ba, sizeof(int32_t));

  return val;
}

void MtkParser::putGpxString(const char *line) {
  for (uint8_t i = 0; ((i < 100) && (line[i] != 0)); i++) {
    out->write(line[i]);
  }
}

void MtkParser::putGpxXmlStart() {
  putGpxString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  putGpxString("<gpx version=\"1.1\" creator=\"SmallStep M5Stack v0.01\"");
  putGpxString(" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema\"");
  putGpxString(" xmlns=\"https://www.topografix.com/GPX/1/1/gpx.xsd\">\n");

  progress.inTrack = false;
  progress.newTrack = true;
}

void MtkParser::putGpxXmlEnd() {
  if (progress.inTrack) {
    putGpxTrackEnd();
  }
  putGpxString("</gpx>\n");
}

void MtkParser::putGpxTrackStart() {
  if (progress.inTrack) {
    putGpxTrackEnd();
  }
  putGpxString("<trk><trkseg>\n");

  progress.inTrack = true;
  progress.newTrack = false;

  Serial.print("MtkParser.putGpxTrackStart: start a new track\n");
}

void MtkParser::putGpxTrackEnd() {
  if (progress.inTrack) {
    putGpxString("</trkseg></trk>\n");
  }

  progress.inTrack = false;
  progress.newTrack = true;
}

void MtkParser::putGpxTrackPoint(gpsrecord_t *rcd) {
  int32_t prog = 0;
  char buf[40];

  if (!progress.inTrack || progress.newTrack) {
    putGpxTrackStart();
  }

  putGpxString("<trkpt");
  if (rcd->format & REG_LAT) {
    sprintf(buf, " lat=\"%0.6f\"", rcd->latitude);
    putGpxString(buf);
  }
  if (rcd->format & REG_LON) {
    sprintf(buf, " lon=\"%0.6f\"", rcd->longitude);
    putGpxString(buf);
  }
  putGpxString(">");

  if (rcd->format & REG_TIME) {
    time_t tt = rcd->time;
    struct tm *t = localtime(&tt);

    sprintf(buf, "<time>%04d-%02d-%02dT%02d:%02d:%02dZ</time>",
            (t->tm_year + 1900), (t->tm_mon + 1), t->tm_mday, t->tm_hour,
            t->tm_min, t->tm_sec);
    putGpxString(buf);
  }
  if (rcd->format & REG_ELE) {
    sprintf(buf, "<ele>%0.2f</ele>", rcd->elevation);
    putGpxString(buf);
  }
  if (rcd->format & REG_SPEED) {
    sprintf(buf, "<speed>%0.2f</speed>", rcd->speed);
    putGpxString(buf);
  }

  putGpxString("</trkpt>\n");
}

bool MtkParser::isDifferentDate(uint32_t t1, uint32_t t2) {
  t1 += (options.offset * (3600 * 24));
  t2 += (options.offset * (3600 * 24));

  return ((t1 / 86400) != (t2 / 86400));
}

void MtkParser::printFormat_d(uint32_t fmt) {
  printf("MtkParser.prinfFormat: format=0x%08X {", fmt);
  if (fmt & REG_TIME) printf("TIME");
  if (fmt & REG_VALID) printf(", VALID");
  if (fmt & REG_LAT) printf(", LAT");
  if (fmt & REG_LON) printf(", LON");
  if (fmt & REG_ELE) printf(", ELE");
  if (fmt & REG_SPEED) printf(", SPD");
  if (fmt & REG_TRACK) printf(", TRK");
  if (fmt & REG_DSTA) printf(", DSTA");
  if (fmt & REG_DAGE) printf(", DAGE");
  if (fmt & REG_PDOP) printf(", PDOP");
  if (fmt & REG_HDOP) printf(", HDOP");
  if (fmt & REG_VDOP) printf(", VDOP");
  if (fmt & REG_NSAT) printf(", NSAT");
  if (fmt & REG_SID) printf(", SID");
  if (fmt & REG_ALT) printf(", ALT");
  if (fmt & REG_AZI) printf(", AZI");
  if (fmt & REG_SNR) printf(", SNR");
  if (fmt & REG_RCR) printf(", RCR");
  if (fmt & REG_MSEC) printf(", MSEC");
  printf("}\n");
}

void MtkParser::setFormatRegister(uint32_t fmt) {
  progress.format = fmt;
  printFormat_d(fmt);
}

float MtkParser::setTimezone(float offset) {
  const uint32_t OFFSET_MAX = 12;   // UTC+24
  const uint32_t OFFSET_MIN = -12;  // UTC-24

  if (offset >= OFFSET_MAX) {
    options.offset = OFFSET_MAX;
  } else if (offset <= OFFSET_MIN) {
    options.offset = OFFSET_MIN;
  } else {
    options.offset = offset;
  }

  return options.offset;
}

bool MtkParser::readBinRecord(gpsrecord_t *rcd) {
  // clear given variable before use
  memset(rcd, 0, sizeof(gpsrecord_t));

  size_t startPos = in->position();
  rcd->format = progress.format;

  // read the UTC (TIME) field if it exists
  if (progress.format & REG_TIME) {  // the record has TIME field
    rcd->time = readBinInt32();

    if (rcd->time < ROLLOVER_OCCUR_TIME) {
      rcd->time += ROLLOVER_CORRECT_VALUE;
    }
  }

  // read LAT/LON/ELE fields if they exists.
  // * M-241 uses different types for these field
  if (progress.m241) {  // for M-241
    if (progress.format & REG_LAT) rcd->latitude = readBinFloat();
    if (progress.format & REG_LON) rcd->longitude = readBinFloat();
    if (progress.format & REG_ELE) rcd->elevation = readBinFloat24();
  } else {  // for other models
    if (progress.format & REG_LAT) rcd->latitude = readBinDouble();
    if (progress.format & REG_LON) rcd->longitude = readBinDouble();
    if (progress.format & REG_ELE) rcd->elevation = readBinFloat();
  }  // if (progress.m241) else

  // read the SPEED field if it exists
  if (progress.format & REG_SPEED) rcd->speed = readBinFloat() / 3.60;

  // ignore all of other fields if they exist
  in->seek(((sizeof(float) * (bool)(progress.format & REG_TRACK)) +
            (sizeof(uint16_t) * (bool)(progress.format & REG_DSTA)) +
            (sizeof(float) * (bool)(progress.format & REG_DAGE)) +
            (sizeof(uint16_t) * (bool)(progress.format & REG_PDOP)) +
            (sizeof(uint16_t) * (bool)(progress.format & REG_HDOP)) +
            (sizeof(uint16_t) * (bool)(progress.format & REG_VDOP)) +
            (sizeof(uint16_t) * (bool)(progress.format & REG_NSAT))),
           SeekCur);
  if (progress.format & REG_SID) {
    uint16_t siv =
        (uint16_t)(readBinInt32() & 0x0000FFFF);  // number of SATs in view
    in->seek(((sizeof(int16_t) * (bool)(progress.format & REG_ALT)) +
              (sizeof(uint16_t) * (bool)(progress.format & REG_AZI)) +
              (sizeof(uint16_t) * (bool)(progress.format & REG_SNR))) *
                 siv,
             SeekCur);
  }  // if (progress.format & REG_SID)

  // get current record size from FILE* position
  size_t currentPos = in->position();
  uint16_t recordSize;

  recordSize = (uint16_t)(currentPos - startPos);

  // calculate checksum value of the current record
  uint8_t checksum = 0;
  in->seek(startPos, SeekSet);
  for (uint16_t i = 0; i < recordSize; i++) {
    checksum ^= (uint8_t)readBinInt8();
  }

  // read the checksum marker charactor ('*') and its value
  char chkmkr = '*';
  if (progress.m241 == false) {
    chkmkr = (char)readBinInt8();
  }

  // verify the checksum and store it
  rcd->valid = ((chkmkr == '*') && ((uint8_t)readBinInt8() == checksum));

  // if data read as record is not valid, seek the pointer to right
  if (rcd->valid == false) {  // checksum is not valid
    // seek pointer +1 byte on the current sector
    in->seek((startPos + 1), SeekSet);

    printf("parser: Non-valid record or pattern at 0x%06X. seek+1\n", startPos);
  }

  return rcd->valid;
}

bool MtkParser::matchBinPattern(const char *ptn, uint8_t len) {
  size_t pos = in->position();

  bool match = true;
  for (int i = 0; ((i < len) && (match)); i++) {
    match = match && ((char)(readBinInt8()) == ptn[i]);
  }

  // if pattern is not match, restore position
  if (match == false) {
    in->seek(pos, SeekSet);
  }

  return match;
}

bool MtkParser::readBinMarkers() {
  bool foundMarker = false;
  size_t startPos = in->position();

  if (matchBinPattern(PTN_DSET_AA,
                      sizeof(PTN_DSET_AA))) {  // found DSP_A (0xAA, ...)
    int8_t dsId = readBinInt8();
    int32_t dsVal = readBinInt32();

    if (matchBinPattern(PTN_DSET_BB,
                        sizeof(PTN_DSET_BB))) {  // found DSP_B (0xBB, ...)
      switch (dsId) {
        case DSP_CHANGE_FORMAT:  // change format register
          progress.format = dsVal;
          break;

        case DSP_LOG_STARTSTOP:  // log start/stop
          if (options.track == TRK_PUT_AS_IS) {
            progress.newTrack = true;
          }
          break;
      }

      printf(
          "MtkParser.readMarker: DSP marker at 0x%06X (id=%d, val=0x%04X) \n",
          startPos, dsId, dsVal);
      foundMarker = true;
    }
  } else if (matchBinPattern(PTN_M241,
                             sizeof(PTN_M241))) {  // found M-241 marker
    if (matchBinPattern(PTN_M241_SP,
                        sizeof(PTN_M241_SP))) {  // found 0x20202020 (fw v1.13)
      // just ignore it. nothing to do
    }

    printf("MtkParser.readMarker: Holux M-241 marker at 0x%06X\n", startPos);
    progress.m241 = true;
    foundMarker = true;

  } else if (matchBinPattern(
                 PTN_SCT_END,
                 sizeof(PTN_SCT_END))) {  // found a sector end marker
    // seek sector position to the next
    progress.sector += 1;

    printf("MtkParser.readMarker: End-of-Sector marker at 0x%06X\n", startPos);
    foundMarker = true;
  }

  return foundMarker;
}

void MtkParser::printRecord_d(gpsrecord_t *rcd) {
  printf("MtkParser.printRecord_d: format=%06X", rcd->format);
  if (rcd->format & REG_TIME) printf(", time=%d", rcd->time);
  if (rcd->format & REG_LAT) printf(", lat=%.04f", rcd->latitude);
  if (rcd->format & REG_LON) printf(", lon=%.04f", rcd->longitude);
  if (rcd->format & REG_ELE) printf(", ele=%.02f", rcd->elevation);
  if (rcd->format & REG_SPEED) printf(", spd=%.02f", rcd->speed);
  printf("\n");
}

bool MtkParser::convert(File *input, File *output, void (*callback)(int32_t)) {
  in = input;
  out = output;

  int32_t prog = 0;

  bool fileend = false;

  in->seek(0, SeekSet);
  out->seek(0, SeekSet);

  fpos_t startPos = 0;
  fpos_t currentPos = 0;

  putGpxXmlStart();

  while (true) {
    const fpos_t SECTOR_START = (0x010000 * progress.sector);
    const fpos_t DATA_START = (SECTOR_START + 0x000200);
    const fpos_t SECTOR_END = (SECTOR_START + 0x00FFFF);

    currentPos = in->position();

    if (DATA_START >= in->size()) {
      printf("MtkParser.convert: reached to the end of the input file\n");
      fileend = true;
      break;
    }

    if (currentPos <= SECTOR_START) {
      printf(
          "MtkParser.convert: read the header of the current sector#%d at "
          "0x%06X\n",
          progress.sector + 1, SECTOR_START);

      in->seek(SECTOR_START, SeekSet);
      uint16_t records = (uint16_t)readBinInt16();
      setFormatRegister(readBinInt32());
    }

    if (currentPos < DATA_START) {
      printf(
          "MtkParser.convert: move to the record block of the current sector "
          "at 0x%06X\n",
          DATA_START);
      in->seek(DATA_START, SeekSet);
    }

    if (readBinMarkers()) {
      continue;
    }

    gpsrecord_t rcd;
    if (readBinRecord(&rcd)) {
      if (rcd.time <= ROLLOVER_OCCUR_TIME) {
        rcd.time += ROLLOVER_CORRECT_VALUE;
      }

      progress.newTrack =
          (progress.newTrack ||
           ((options.track == TRK_ONE_DAY) &&
            (isDifferentDate(progress.lastRecord.time, rcd.time))));

      putGpxTrackPoint(&rcd);

      if (progress.firstRecord.time == 0) {
        progress.firstRecord = rcd;
      }
      progress.lastRecord = rcd;
    }

    if (callback != NULL) {
      int32_t _p = 100 * ((float)in->position() / (float)in->size());
      if (_p > prog) {
        callback(_p);
        prog = _p;
      }
    }
  }

  putGpxXmlEnd();

  if (callback != NULL) callback(100);

  out->flush();

  return (fileend);
}

uint32_t MtkParser::getFirstRecordTime() { return progress.firstRecord.time; }
uint32_t MtkParser::getLastRecordTime() { return progress.lastRecord.time; }