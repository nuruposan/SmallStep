#include "MtkParser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

MtkParser::MtkParser() {
  memset(&options, 0, sizeof(parseoption_t));
  memset(&status, 0, sizeof(parsestatus_t));
}

MtkParser::~MtkParser() {
  //
}

bool MtkParser::isDifferentDate(uint32_t t1, uint32_t t2) {
  uint32_t offsetSec = (options.timeOffset * 3600);

  t1 += offsetSec;
  t2 += offsetSec;

  return ((t1 / 86400) != (t2 / 86400));
}

void MtkParser::setRecordFormat(uint32_t fmt) {
  status.formatReg = fmt;

  status.ignoreLen1 = (sizeof(uint16_t) * (bool)(fmt & REG_VALID));
  status.ignoreLen2 = (sizeof(float) * (bool)(fmt & REG_TRACK)) +
                      (sizeof(uint16_t) * (bool)(fmt & REG_DSTA)) +
                      (sizeof(float) * (bool)(fmt & REG_DAGE)) +
                      (sizeof(uint16_t) * (bool)(fmt & REG_PDOP)) +
                      (sizeof(uint16_t) * (bool)(fmt & REG_HDOP)) +
                      (sizeof(uint16_t) * (bool)(fmt & REG_VDOP)) +
                      (sizeof(uint16_t) * (bool)(fmt & REG_NSAT));
  status.ignoreLen3 = (sizeof(int16_t) * (bool)(fmt & REG_ALT)) +
                      (sizeof(uint16_t) * (bool)(fmt & REG_AZI));
  status.ignoreLen4 = (sizeof(uint16_t) * (bool)(fmt & REG_RCR)) +
                      (sizeof(uint16_t) * (bool)(fmt & REG_MSEC));

  Serial.printf(
      "Parser.setFormat: changed [fmtreg=0x%08X, ignlen={%d, %d, %d, %d}]\n",
      status.formatReg, status.ignoreLen1, status.ignoreLen2, status.ignoreLen3,
      status.ignoreLen4);

  // Note: record format is as follows:
  // {TIME, VALID, LAT, LON, ELE, SPEED, TRACK, DSTA, DAGE, PDOP, HDOP, VDOP,
  //  NSAT, SID, (ALT, AZI) x SID, RCR, MSEC} '*' CHKSUM
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
  uint32_t startPos = in->setMark();

  // clear given variable before use
  memset(rcd, 0, sizeof(gpsrecord_t));

  // store the current record format
  rcd->format = status.formatReg;

  // read TIME field  (=UTC unixtime)
  if (status.formatReg & REG_TIME) {
    // read value from data and correct GPS Week rollover
    rcd->time = in->readInt32();
    if (rcd->time < ROLLOVER_TIME) rcd->time += ROLLOVER_CORRECT;
  }

  // skip VALID field if it exists
  in->seekCur(status.ignoreLen1);

  // store LAT/LON/ELE fields
  if (status.m241Mode) {  // for Holux M-241
    if (rcd->format & REG_LAT) rcd->latitude = in->readFloat();
    if (rcd->format & REG_LON) rcd->longitude = in->readFloat();
    if (rcd->format & REG_ELE) rcd->elevation = in->readFloat24();
  } else {  // for other standard models
    if (rcd->format & REG_LAT) rcd->latitude = in->readDouble();
    if (rcd->format & REG_LON) rcd->longitude = in->readDouble();
    if (rcd->format & REG_ELE) rcd->elevation = in->readFloat();
  }

  // read the SPEED field
  if (rcd->format & REG_SPEED) rcd->speed = in->readFloat() / 3.60;

  // skip all of other fields (TRACK/DSTA/DAGE/PDOP/HDOP/VDO/NSAT/ALT/AZI/SNR)
  in->seekCur(status.ignoreLen2);
  if (rcd->format & REG_SID) {
    uint16_t siv =
        (uint16_t)(in->readInt32() & 0x0000FFFF);  // number of SATs in view
    in->seekCur(status.ignoreLen3 +
                (siv * (sizeof(uint16_t) * (bool)(rcd->format & REG_SNR))));
  }
  in->seekCur(status.ignoreLen4);

  // calc current record size from FILE* position and calculate the checksum
  rcd->size = (in->position() - startPos);
  uint8_t checksum = 0;
  in->moveToMark();
  for (uint16_t i = 0; i < rcd->size; i++) {
    checksum ^= (uint8_t)in->readInt8();
  }

  // read the checksum delimiter '*' when the logger is not Holux M-241
  // nore: Holux M-241 does not write '*' before checksum value
  char chkmkr = (status.m241Mode) ? '*' : in->readInt8();

  // verify the checksum is correct or not
  rcd->valid = ((chkmkr == '*') && ((uint8_t)in->readInt8() == checksum));

  // if there is no valid record here, restore the original position
  if (!rcd->valid) in->moveToMark();

  // return the result of the record reading
  return rcd->valid;
}

bool MtkParser::matchBinPattern(const char *ptn, uint8_t len) {
  // get the current position before matching the pattern
  in->setMark();

  // check if the pattern is matched from the current position
  bool match = true;
  for (int i = 0; ((i < len) && (match)); i++) {
    match = match && ((char)(in->readInt8()) == ptn[i]);
  }

  // restore the position if the pattern is not matched
  if (!match) in->moveToMark();

  // return the result of the matching
  return match;
}

bool MtkParser::readBinMarkers() {
  bool match = false;
  size_t startPos = in->setMark();

  if (matchBinPattern(PTN_DSET_AA, sizeof(PTN_DSET_AA))) {  // DSP_A
    // read type and value of the DSP
    uint8_t dsType = in->readInt8();
    uint32_t dsVal = in->readInt32();

    if (matchBinPattern(PTN_DSET_BB, sizeof(PTN_DSET_BB))) {  // DSP_B
      match = true;

      // do action according to the DSP type
      switch (dsType) {
        case DSP_CHANGE_FORMAT:  // change format register
          setRecordFormat(dsVal);
          break;
        case DSP_LOG_STARTSTOP:  // log start/stop
          if (options.trackMode == TRK_AS_IS) {
            out->endTrack();
          }
          break;
      }

      Serial.printf("Parser.readMarker: DSP at 0x%05X [t=%d, v=0x%04X]\n",
                    startPos, dsType, dsVal);
    }
  } else if (matchBinPattern(PTN_M241, sizeof(PTN_M241))) {  // M-241
    matchBinPattern(PTN_M241_SP, sizeof(PTN_M241_SP));       // M-241 fw1.13

    status.m241Mode = true;
    match = true;

    Serial.printf("Parser.readMarker: M-241 marker at 0x%05X\n", startPos);
  } else if (matchBinPattern(PTN_SCT_END,
                             sizeof(PTN_SCT_END))) {  // sector end
    // seek sector position to the next
    status.sectorPos += 1;
    match = true;

    Serial.printf("Parser.readMarker: sector end at 0x%05X\n", startPos);
  }

  return match;
}

bool MtkParser::convert(File32 *input, File32 *output,
                        void (*callback)(int32_t)) {
  // clear all of the status variables before use
  memset(&status, 0, sizeof(parsestatus_t));

  in = new MtkFileReader(input);
  out = new GpxFileWriter(output);

  int32_t progRate = 0;
  bool fileend = false;

  out->startXml();

  uint32_t startPos = 0;
  uint32_t currentPos = 0;
  uint32_t sectorStart = -1;
  uint32_t dataStart = 0;
  uint32_t sectorEnd = 0;

  while (true) {
    if (sectorStart != (SIZE_SECTOR * status.sectorPos)) {
      sectorStart = (SIZE_SECTOR * status.sectorPos);
      dataStart = (sectorStart + SIZE_HEADER);
      sectorEnd = (sectorStart + (SIZE_SECTOR - 1));
    }

    if (dataStart >= in->filesize()) {
      //      printf("Parser.convert: finished [tracks=%d, trkpts=%d]\n",
      //             status.totalTracks, status.totalRecords);
      fileend = true;
      break;
    }

    if (in->position() <= sectorStart) {
      in->seekCur(sectorStart - in->position());
      printf("Parser.convert: sector#%d start at 0x%05X, %d records\n",
             status.sectorPos, sectorStart, (uint16_t)in->readInt16());
      setRecordFormat(in->readInt32());
      in->seekCur(dataStart - in->position());
    }

    if (readBinMarkers()) continue;

    int32_t pos = in->position();
    gpsrecord_t rcd;
    if (!readBinRecord(&rcd)) {
      Serial.printf("Parser.convert: no valid data at 0x%05X\n",
                    in->position());
      in->seekCur(1);
      continue;
    }

    if ((options.trackMode == TRK_ONE_DAY) &&
        (isDifferentDate(status.lastRecord.time, rcd.time))) {
      out->startTrack();
    }

    Serial.printf("%05X: ", pos);
    out->putTrkpt(rcd);

    if (status.firstRecord.time == 0) status.firstRecord = rcd;
    status.lastRecord = rcd;

    if ((in->position() + rcd.size) >= sectorEnd) {
      status.sectorPos += 1;
    }

    if (callback != NULL) {
      int32_t _pr = 100 * ((float)in->position() / in->filesize());
      if (_pr > progRate) {
        callback(_pr);
        progRate = _pr;
      }
    }
  }

  if (callback != NULL) callback(100);

  out->endXml();
  out->flush();

  delete in;
  delete out;

  return (fileend);
}

uint32_t MtkParser::getFirstRecordTime() { return status.firstRecord.time; }
uint32_t MtkParser::getLastRecordTime() { return status.lastRecord.time; }
