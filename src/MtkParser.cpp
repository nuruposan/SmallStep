#include "MtkParser.hpp"

typedef enum _fmtreg {
  REG_TIME = 0x000001,   // uint32_
  REG_VALID = 0x000002,  // uint16
  REG_LAT = 0x000004,    // double / float(M-241)
  REG_LON = 0x000008,    // double / float(M-241)
  REG_ELE = 0x000010,    // float / float24(M-241)
  REG_SPEED = 0x000020,  // float
  REG_TRACK = 0x000040,  // float
  REG_DSTA = 0x000080,   // uint16
  REG_DAGE = 0x000100,   // float
  REG_PDOP = 0x000200,   // uint16
  REG_HDOP = 0x000400,   // uint16
  REG_VDOP = 0x000800,   // uint16
  REG_NSAT = 0x001000,   // uint16
  REG_SID = 0x002000,    // uint32 (SIV)
  REG_ALT = 0x004000,    // int16
  REG_AZI = 0x008000,    // uint16
  REG_SNR = 0x010000,    // uint16
  REG_RCR = 0x020000,    // uint16
  REG_MSEC = 0x040000    // uint16
} formatreg_t;

typedef enum _dspid {
  DSP_CHANGE_FORMAT = 2,
  DSP_AUTOLOG_SECOND = 3,
  DSP_AUTOLOG_METER = 4,
  DSP_AUTOLOG_SPEED = 5,
  DSP_CHANGE_METHOD = 6,
  DSP_LOG_STARTSTOP = 7
} dspid_t;

const uint32_t MtkParser::SIZE_SECTOR = 0x010000;  // 0x010000 = 65536 bytes
const uint32_t MtkParser::SIZE_HEADER = 0x000200;  // 512 bytes

const uint32_t MtkParser::ROLLOVER_TIME = 1554595200;    // 2019-04-07
const uint32_t MtkParser::ROLLOVER_CORRECT = 619315200;  // 1024 weeks

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
  memset(&options, 0, sizeof(parseopt_t));
  memset(&progress, 0, sizeof(parseinfo_t));
}

MtkParser::~MtkParser() {
  //
}

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
}

void MtkParser::putGpxXmlEnd() {
  if (progress.inTrack) {
    putGpxTrackEnd();
  }
  putGpxString("</gpx>\n");
}

void MtkParser::putGpxTrackStart() {
  if (progress.inTrack) putGpxTrackEnd();
  putGpxString("<trk><trkseg>\n");

  progress.inTrack = true;
  progress.totalTracks += 1;

  Serial.print("Parser.putGpxTrackStart: new track\n");
}

void MtkParser::putGpxTrackEnd() {
  if (progress.inTrack) {
    putGpxString("</trkseg></trk>\n");
  }

  progress.inTrack = false;
}

void MtkParser::putGpxTrackPoint(gpsrecord_t *rcd) {
  int32_t prog = 0;
  char buf[40];

  if (!progress.inTrack) putGpxTrackStart();

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
  uint32_t offsetSec = (options.timeOffset * 3600);

  t1 += offsetSec;
  t2 += offsetSec;

  return ((t1 / 86400) != (t2 / 86400));
}

void MtkParser::setRecordFormat(uint32_t fmt) {
  progress.recordFormat = fmt;

  uint8_t readLen = sizeof(uint32_t) * (bool)(fmt & REG_TIME) +
                    sizeof(double) * (bool)(fmt & REG_LAT) +
                    sizeof(double) * (bool)(fmt & REG_LON) +
                    sizeof(float) * (bool)(fmt & REG_ELE) +
                    sizeof(float) * (bool)(fmt & REG_SPEED);

  progress.ignoreLen1 = (sizeof(uint16_t) * (bool)(fmt & REG_VALID));
  progress.ignoreLen2 = (sizeof(float) * (bool)(fmt & REG_TRACK)) +
                        (sizeof(uint16_t) * (bool)(fmt & REG_DSTA)) +
                        (sizeof(float) * (bool)(fmt & REG_DAGE)) +
                        (sizeof(uint16_t) * (bool)(fmt & REG_PDOP)) +
                        (sizeof(uint16_t) * (bool)(fmt & REG_HDOP)) +
                        (sizeof(uint16_t) * (bool)(fmt & REG_VDOP)) +
                        (sizeof(uint16_t) * (bool)(fmt & REG_NSAT));
  progress.ignoreLen3 = (sizeof(int16_t) * (bool)(fmt & REG_ALT)) +
                        (sizeof(uint16_t) * (bool)(fmt & REG_AZI));
  progress.ignoreLen4 = (sizeof(uint16_t) * (bool)(fmt & REG_RCR)) +
                        (sizeof(uint16_t) * (bool)(fmt & REG_MSEC));

  Serial.printf(
      "Parser.setFormat: changed [fmtreg=0x%08X, len={%d, %d, %d, %d}]\n",
      progress.recordFormat, readLen, progress.ignoreLen1, progress.ignoreLen2,
      progress.ignoreLen3);
}

float MtkParser::setTimeOffset(float offset) {
  const float OFFSET_MAX = 12.0;   // UTC+12
  const float OFFSET_MIN = -12.0;  // UTC-12

  options.timeOffset = offset;
  if (options.timeOffset > OFFSET_MAX) options.timeOffset = OFFSET_MAX;
  if (options.timeOffset < OFFSET_MIN) options.timeOffset = OFFSET_MIN;

  return options.timeOffset;
}

bool MtkParser::readBinRecord(gpsrecord_t *rcd) {
  // get the current position before reading a record
  uint32_t startPos = in->position();

  // clear given variable before use
  memset(rcd, 0, sizeof(gpsrecord_t));

  // store the current record format
  rcd->format = progress.recordFormat;

  // read TIME field  (=UTC unixtime)
  if (progress.recordFormat & REG_TIME) {
    // read value from data and correct GPS Week rollover
    rcd->time = readBinInt32();
    if (rcd->time < ROLLOVER_TIME) rcd->time += ROLLOVER_CORRECT;
  }

  // skip VALID field if it exists
  in->seek(in->position() + progress.ignoreLen1);

  // store LAT/LON/ELE fields
  if (progress.m241Mode) {  // for Holux M-241
    if (rcd->format & REG_LAT) rcd->latitude = readBinFloat();
    if (rcd->format & REG_LON) rcd->longitude = readBinFloat();
    if (rcd->format & REG_ELE) rcd->elevation = readBinFloat24();
  } else {  // for other standard models
    if (rcd->format & REG_LAT) rcd->latitude = readBinDouble();
    if (rcd->format & REG_LON) rcd->longitude = readBinDouble();
    if (rcd->format & REG_ELE) rcd->elevation = readBinFloat();
  }

  // read the SPEED field
  if (rcd->format & REG_SPEED) rcd->speed = readBinFloat() / 3.60;

  // skip all of other fields (TRACK/DSTA/DAGE/PDOP/HDOP/VDO/NSAT/ALT/AZI/SNR)
  in->seek(in->position() + progress.ignoreLen2);
  if (rcd->format & REG_SID) {
    uint16_t siv =
        (uint16_t)(readBinInt32() & 0x0000FFFF);  // number of SATs in view
    in->seek(in->position() + progress.ignoreLen3 +
             (siv * (sizeof(uint16_t) * (bool)(rcd->format & REG_SNR))));
  }
  in->seek(in->position() + progress.ignoreLen4);

  // calc current record size from FILE* position and calculate the checksum
  rcd->size = (in->position() - startPos);
  uint8_t checksum = 0;
  in->seek(startPos);
  for (uint16_t i = 0; i < rcd->size; i++) {
    checksum ^= (uint8_t)readBinInt8();
  }

  // read the checksum delimiter '*' when the logger is not Holux M-241
  // nore: Holux M-241 does not write '*' before checksum value
  char chkmkr = (progress.m241Mode) ? '*' : (char)readBinInt8();

  // verify the checksum is correct or not
  rcd->valid = ((chkmkr == '*') && ((uint8_t)readBinInt8() == checksum));

  // if there is no valid record here, restore the original position
  if (!rcd->valid) in->seek(startPos);

  // return the result of the record reading
  return rcd->valid;
}

bool MtkParser::matchBinPattern(const char *ptn, uint8_t len) {
  // get the current position before matching the pattern
  uint32_t startPos = in->position();

  // check if the pattern is matched from the current position
  bool match = true;
  for (int i = 0; ((i < len) && (match)); i++) {
    match = match && ((char)(readBinInt8()) == ptn[i]);
  }

  // restore the position if the pattern is not matched
  if (!match) in->seek(startPos);

  // return the result of the matching
  return match;
}

bool MtkParser::readBinMarkers() {
  bool match = false;
  size_t startPos = in->position();

  if (matchBinPattern(PTN_DSET_AA, sizeof(PTN_DSET_AA))) {  // DSP_A
    // read type and value of the DSP
    uint8_t dsType = readBinInt8();
    uint32_t dsVal = readBinInt32();

    if (matchBinPattern(PTN_DSET_BB, sizeof(PTN_DSET_BB))) {  // DSP_B
      match = true;

      // do action according to the DSP type
      switch (dsType) {
        case DSP_CHANGE_FORMAT:  // change format register
          setRecordFormat(dsVal);
          break;

        case DSP_LOG_STARTSTOP:  // log start/stop
          if (options.trackMode == TRK_AS_IS) {
            putGpxTrackEnd();
          }
          break;
      }

      Serial.printf("Parser.readMarker: DSP at 0x%05X [T=%d, V=0x%04X]\n",
                    startPos, dsType, dsVal);
    }
  } else if (matchBinPattern(PTN_M241, sizeof(PTN_M241))) {  // M-241
    matchBinPattern(PTN_M241_SP, sizeof(PTN_M241_SP));       // M-241 fw1.13

    progress.m241Mode = true;
    match = true;

    Serial.printf("Parser.readMarker: M-241 marker at 0x%05X\n", startPos);
  } else if (matchBinPattern(PTN_SCT_END,
                             sizeof(PTN_SCT_END))) {  // sector end
    // seek sector position to the next
    progress.sectorId += 1;
    match = true;

    Serial.printf("Parser.readMarker: sector end at 0x%05X\n", startPos);
  }

  return match;
}

bool MtkParser::convert(File32 *input, File32 *output,
                        void (*callback)(int32_t)) {
  // clear all of the progress variables before use
  memset(&progress, 0, sizeof(parseinfo_t));

  in = input;
  out = output;

  int32_t progRate = 0;
  bool fileend = false;

  in->seek(0);
  out->seek(0);

  putGpxXmlStart();

  uint32_t startPos = 0;
  uint32_t currentPos = 0;
  uint32_t sectorStart = -1;
  uint32_t dataStart = 0;
  uint32_t sectorEnd = 0;

  while (true) {
    if (sectorStart != (SIZE_SECTOR * progress.sectorId)) {
      sectorStart = (SIZE_SECTOR * progress.sectorId);
      dataStart = (sectorStart + SIZE_HEADER);
      sectorEnd = (sectorStart + (SIZE_SECTOR - 1));
    }

    currentPos = in->position();

    if (dataStart >= in->size()) {
      printf("Parser.convert: finished [tracks=%d, trkpts=%d]\n",
             progress.totalTracks, progress.totalRecords);
      fileend = true;
      break;
    }

    if (currentPos <= sectorStart) {
      in->seek(sectorStart);
      printf("Parser.convert: sector#%d at 0x%05X, %d records\n",
             progress.sectorId, sectorStart, (uint16_t)readBinInt16());
      setRecordFormat(readBinInt32());
    }

    if (currentPos < dataStart) {
      printf("Parser.convert: move cursor to 0x%05X\n", dataStart);
      in->seek(dataStart);
    }

    if (readBinMarkers()) continue;

    gpsrecord_t rcd;
    readBinRecord(&rcd);
    if (!rcd.valid) {
      Serial.printf("Parser.convert: no valid data at 0x%05X\n",
                    in->position());
      in->seek(in->position() + 1);

      continue;
    }

    if ((options.trackMode == TRK_ONE_DAY) &&
        (isDifferentDate(progress.lastRecord.time, rcd.time))) {
      putGpxTrackStart();
    }
    putGpxTrackPoint(&rcd);

    if (progress.firstRecord.time == 0) progress.firstRecord = rcd;
    progress.lastRecord = rcd;
    progress.totalRecords += 1;
    if ((in->position() + rcd.size) >= sectorEnd) {
      progress.sectorId += 1;
    }

    if (callback != NULL) {
      int32_t _pr = 100 * ((float)in->position() / in->size());
      if (_pr > progRate) {
        callback(_pr);
        progRate = _pr;
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
