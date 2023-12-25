#include "MtkParser.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
  memset(&binFile, 0, sizeof(fileinfo_t));
  memset(&gpxFile, 0, sizeof(fileinfo_t));
  memset(&options, 0, sizeof(parseopt_t));
  memset(&progress, 0, sizeof(proginfo_t));
}

MtkParser::~MtkParser() {
  closeBinFile();
  closeGpxFile();
}

uint8_t MtkParser::openBinFile(const char *name) {
  if ((binFile.fp = fopen(name, "r")) == NULL) {
    memset(&binFile, 0, sizeof(fileinfo_t));
    return -1;
  }

  strcpy(binFile.name, name);
  binFile.size = getFileSize(binFile.fp);

  return 0;
}

uint8_t MtkParser::openGpxFile(const char *name) {
  if ((gpxFile.fp = fopen(name, "w")) == NULL) {
    memset(&gpxFile, 0, sizeof(fileinfo_t));
    return -1;
  }

  strcpy(gpxFile.name, name);
  gpxFile.size = getFileSize(gpxFile.fp);

  putGpxXmlStart();

  return 0;
}

void MtkParser::closeBinFile() {
  if (binFile.fp != NULL) {
    fclose(binFile.fp);
    memset(&binFile, 0, sizeof(fileinfo_t));
  }
}

void MtkParser::closeGpxFile() {
  putGpxXmlEnd();

  if (gpxFile.fp != NULL) {
    fclose(gpxFile.fp);
    memset(&gpxFile, 0, sizeof(fileinfo_t));
  }
}

fpos_t MtkParser::getFileSize(FILE *fp) {
  fpos_t pos, sz;

  // get current position, seek to end (=file size), restore position
  fgetpos(fp, &pos);
  fseek(fp, 0, SEEK_END);
  fgetpos(fp, &sz);
  fseek(fp, pos, SEEK_SET);

  return sz;
}

double MtkParser::readBinDouble() {
  FILE *fp = binFile.fp;
  double val;
  uint8_t ba[] = {(uint8_t)fgetc(fp), (uint8_t)fgetc(fp), (uint8_t)fgetc(fp),
                  (uint8_t)fgetc(fp), (uint8_t)fgetc(fp), (uint8_t)fgetc(fp),
                  (uint8_t)fgetc(fp), (uint8_t)fgetc(fp)};
  memcpy(&val, &ba, sizeof(double));

  return val;
}

float MtkParser::readBinFloat24() {
  FILE *fp = binFile.fp;

  float val;
  uint8_t ba[] = {0x00, (uint8_t)fgetc(fp), (uint8_t)fgetc(fp),
                  (uint8_t)fgetc(fp)};
  memcpy(&val, &ba, sizeof(float));

  return val;
}

float MtkParser::readBinFloat() {
  FILE *fp = binFile.fp;

  float val;
  uint8_t ba[] = {(uint8_t)fgetc(fp), (uint8_t)fgetc(fp), (uint8_t)fgetc(fp),
                  (uint8_t)fgetc(fp)};
  memcpy(&val, &ba, sizeof(float));

  return val;
}

int32_t MtkParser::readBinInt8() {
  FILE *fp = binFile.fp;
  return fgetc(fp);
}

int32_t MtkParser::readBinInt16() {
  FILE *fp = binFile.fp;

  int32_t val;
  uint8_t ba[] = {(uint8_t)fgetc(fp), (uint8_t)fgetc(fp), 0x00, 0x00};
  memcpy(&val, &ba, sizeof(float));

  return val;
}

int32_t MtkParser::readBinInt32() {
  FILE *fp = binFile.fp;

  int32_t val;
  uint8_t ba[] = {(uint8_t)fgetc(fp), (uint8_t)fgetc(fp), (uint8_t)fgetc(fp),
                  (uint8_t)fgetc(fp)};
  memcpy(&val, &ba, sizeof(float));

  return val;
}

void MtkParser::putGpxString(const char *line) {
  for (uint8_t i = 0; ((i < 100) && (line[i] != 0)); i++) {
    fputc(line[i], gpxFile.fp);
  }

  fgetpos(gpxFile.fp, &gpxFile.size);
}

void MtkParser::putGpxXmlStart() {
  putGpxString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  putGpxString("<gpx version=\"1.1\" creator=\"SmallStep M5S v0.01\"");
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
}

void MtkParser::putGpxTrackEnd() {
  if (progress.inTrack) {
    putGpxString("</trkseg></trk>\n");
  }

  progress.inTrack = false;
  progress.newTrack = true;
}

void MtkParser::putGpxTrackPoint(gpsrecord_t *rcd) {
  char buf[40];

  if ((progress.inTrack == false) || (progress.newTrack == true)) {
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

  if ((rcd->format & REG_TIME) && (options.time != TIM_DROP_FIELD)) {
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
  if ((rcd->format & REG_SPEED) && (options.speed != SPD_DROP_FIELD)) {
    sprintf(buf, "<speed>%0.2f</speed>", rcd->speed);
    putGpxString(buf);
  }

  putGpxString("</trkpt>\n");
}

bool MtkParser::isSpanningDate(uint32_t t1, uint32_t t2) {
  t1 += options.offset;
  t2 += options.offset;
  bool span = ((t1 / 86400) != (t2 / 86400));

  return span;
}

void MtkParser::printFormatRegister_d(uint32_t fmt) {
  printf("**debug** Change format=0x%08X {", fmt);
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
  printFormatRegister_d(fmt);
}

uint32_t MtkParser::setTimezoneOffset(uint32_t tzo) {
  const uint32_t OFFSET_MAX = (3600 * +24);  // UTC+24
  const uint32_t OFFSET_MIN = (3600 * -24);  // UTC-24

  if (tzo >= OFFSET_MAX) {
    options.offset = OFFSET_MAX;
  } else if (tzo <= OFFSET_MIN) {
    options.offset = OFFSET_MIN;
  } else {
    options.offset = tzo;
  }

  return options.offset;
}

bool MtkParser::readBinRecord(gpsrecord_t *rcd) {
  // clear given variable before use
  memset(rcd, 0, sizeof(gpsrecord_t));

  fpos_t startPos;
  rcd->format = progress.format;
  fgetpos(binFile.fp, &startPos);

  // read the UTC (TIME) field if it exists
  if (progress.format & REG_TIME) {  // the record has TIME field
    rcd->time = readBinInt32();

    if ((options.time == TIM_FIX_ROLLOVER) &&
        (rcd->time < ROLLOVER_OCCUR_TIME)) {
      rcd->time += ROLLOVER_CORRECT_VALUE;
    }
  }

  // read LAT/LON/ELE fields if they exists.
  // * M-241 uses different types for these field
  if (progress.m241bin == true) {  // for M-241
    if (progress.format & REG_LAT) rcd->latitude = readBinFloat();
    if (progress.format & REG_LON) rcd->longitude = readBinFloat();
    if (progress.format & REG_ELE) rcd->elevation = readBinFloat24();
  } else {  // for other models (isM241 == false)
    if (progress.format & REG_LAT) rcd->latitude = readBinDouble();
    if (progress.format & REG_LON) rcd->longitude = readBinDouble();
    if (progress.format & REG_ELE) rcd->elevation = readBinFloat();
  }  // if (progress.m241bin == true) else

  // read the SPEED field if it exists
  if (progress.format & REG_SPEED) rcd->speed = readBinFloat() / 3.60;

  // ignore all of other fields if they exist
  fseek(binFile.fp,
        ((sizeof(float) * (bool)(progress.format & REG_TRACK)) +
         (sizeof(uint16_t) * (bool)(progress.format & REG_DSTA)) +
         (sizeof(float) * (bool)(progress.format & REG_DAGE)) +
         (sizeof(uint16_t) * (bool)(progress.format & REG_PDOP)) +
         (sizeof(uint16_t) * (bool)(progress.format & REG_HDOP)) +
         (sizeof(uint16_t) * (bool)(progress.format & REG_VDOP)) +
         (sizeof(uint16_t) * (bool)(progress.format & REG_NSAT))),
        SEEK_CUR);
  if (progress.format & REG_SID) {
    uint16_t siv =
        (uint16_t)(readBinInt32() & 0x0000FFFF);  // number of SATs in view
    fseek(binFile.fp,
          ((sizeof(int16_t) * (bool)(progress.format & REG_ALT)) +
           (sizeof(uint16_t) * (bool)(progress.format & REG_AZI)) +
           (sizeof(uint16_t) * (bool)(progress.format & REG_SNR))) *
              siv,
          SEEK_CUR);
  }  // if (progress.format & REG_SID)

  // get current record size from FILE* position
  fpos_t currentPos;
  uint16_t recordSize;
  fgetpos(binFile.fp, &currentPos);
  recordSize = (uint16_t)(currentPos - startPos);

  // calculate checksum value of the current record
  uint8_t checksum = 0;
  fseek(binFile.fp, startPos, SEEK_SET);
  for (uint16_t i = 0; i < recordSize; i++) {
    checksum ^= (uint8_t)readBinInt8();
  }

  // read the checksum marker charactor ('*') and its value
  char chkmkr = '*';
  if (progress.m241bin == false) {
    chkmkr = (char)readBinInt8();
  }

  // verify the checksum and store it
  rcd->valid = ((chkmkr == '*') && ((uint8_t)readBinInt8() == checksum));

  // if data read as record is not valid, seek the pointer to right
  if (rcd->valid == false) {  // checksum is not valid
    // seek pointer +1 byte on the current sector
    fseek(binFile.fp, (startPos + 1), SEEK_SET);

    printf("**debug** No valid record found at 0x%06llX. Moving position +1\n",
           startPos);
  }

  return rcd->valid;
}

bool MtkParser::matchBinPattern(const char *ptn, uint8_t len) {
  fpos_t pos;
  fgetpos(binFile.fp, &pos);

  bool match = true;
  for (int i = 0; ((i < len) && (match)); i++) {
    match = match && ((char)(readBinInt8()) == ptn[i]);
  }

  // if pattern is not match, restore position
  if (match == false) {
    fseek(binFile.fp, pos, SEEK_SET);
  }

  return match;
}

bool MtkParser::readBinMarkers() {
  bool foundMarker = false;
  fpos_t startPos;
  fgetpos(binFile.fp, &startPos);

  if (matchBinPattern(PTN_DSET_AA,
                      sizeof(PTN_DSET_AA))) {  // found DSP (0xAA, 0xAA, ...)
    int8_t dsId = readBinInt8();
    int32_t dsVal = readBinInt32();

    if (matchBinPattern(PTN_DSET_BB,
                        sizeof(PTN_DSET_BB))) {  // found DSP (0xBB, 0xBB, ...)
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

      printf("**debug** Found DSP (id=%d, value=0x%04X) at 0x%06llX\n", dsId,
             dsVal, startPos);
      foundMarker = true;
    }
  } else if (matchBinPattern(PTN_M241,
                             sizeof(PTN_M241))) {  // found Holux M-241 pattern
    if (matchBinPattern(PTN_M241_SP,
                        sizeof(PTN_M241_SP))) {  // found DSP (0x20, 0x20, ...)
      // just ignore it. nothing to do
    }

    printf("**debug** Found Holux M-241 marker at 0x%06llX\n", startPos);
    progress.m241bin = true;
    foundMarker = true;

  } else if (matchBinPattern(
                 PTN_SCT_END,
                 sizeof(PTN_SCT_END))) {  // found a sector end marker
    // seek sector position to the next
    progress.sector += 1;

    printf("**debug** Found Sector-End marker at 0x%06llX. Moving sector +1\n",
           startPos);
    foundMarker = true;
  }

  return foundMarker;
}

void MtkParser::printGpsRecord_d(gpsrecord_t *rcd) {
  printf("**debug** GpsRecord: FMT=%08X", rcd->format);
  if (rcd->format & REG_TIME) printf(", TIME=%d", rcd->time);
  if (rcd->format & REG_LAT) printf(", LAT=%.04f", rcd->latitude);
  if (rcd->format & REG_LON) printf(", LON=%.04f", rcd->longitude);
  if (rcd->format & REG_ELE) printf(", ELE=%.02f", rcd->elevation);
  if (rcd->format & REG_SPEED) printf(", SPD=%.02f", rcd->speed);
  printf("\n");
}

/*
double MtkParser::calcDistance(gpsrecord_t *r1, gpsrecord_t *r2) {
    const double EARTH_R = 6378137;  // unit: meter
    double nsDist = EARTH_R * ((M_PI/180) * (r1->lat - r2->lat));
    double ewDist = EARTH_R * cos((M_PI/180) * (r1->lat * (r1->lon - r2->lon)));
    double elDist = r1->elevation - r2->elevation;

    double dist = sqrt(pow(nsDist, 2) + pow(ewDist, 2) + pow(elDist, 2));
}
*/

bool MtkParser::run(uint16_t token) {
  if ((binFile.fp == NULL) || (gpxFile.fp == NULL)) {
    return true;
  }

  bool fileend = false;

  fpos_t startPos, currentPos = 0;
  fgetpos(binFile.fp, &startPos);

  while (currentPos < (startPos + token)) {
    const fpos_t SECTOR_START = (0x010000 * progress.sector);
    const fpos_t DATA_START = (SECTOR_START + 0x000200);
    const fpos_t SECTOR_END = (SECTOR_START + 0x00FFFF);

    fgetpos(binFile.fp, &currentPos);

    if (DATA_START >= binFile.size) {
      printf("**debug** Reached to end of BIN file\n");
      fileend = true;
      break;
    }

    if (currentPos <= SECTOR_START) {
      printf("**debug** Reading header of sector#%d at 0x%06llX\n",
             progress.sector + 1, SECTOR_START);

      fseek(binFile.fp, SECTOR_START, SEEK_SET);
      uint16_t records = (uint16_t)readBinInt16();
      setFormatRegister(readBinInt32());
    }

    if (currentPos < DATA_START) {
      printf("**debug** Moving position to data part at 0x%06llX\n",
             DATA_START);
      fseek(binFile.fp, DATA_START, SEEK_SET);
    }

    if (readBinMarkers() == true) {
      continue;
    }

    gpsrecord_t rcd;
    if (readBinRecord(&rcd) == true) {
      progress.newTrack =
          (progress.newTrack ||
           ((options.track == TRK_ONE_DAY) &&
            (isSpanningDate(progress.lastRecord.time, rcd.time))));

      putGpxTrackPoint(&rcd);

      if (progress.firstRecord.time == 0) {
        progress.firstRecord = rcd;
      }
      progress.lastRecord = rcd;
    }
  }

  fflush(gpxFile.fp);

  return (!fileend);
}
