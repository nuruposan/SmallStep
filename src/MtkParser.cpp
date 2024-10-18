#include "MtkParser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

MtkParser::MtkParser() {
  memset(&options, 0, sizeof(parseopt_t));
  memset(&status, 0, sizeof(parsestatus_t));
}

bool MtkParser::isDifferentDate(uint32_t t1, uint32_t t2) {
  uint32_t offsetSec = (options.timeOffset * 3600);

  t1 += offsetSec;
  t2 += offsetSec;

  return ((t1 / 86400) != (t2 / 86400));
}

void MtkParser::setRecordFormat(uint32_t format) {
  status.logFormat = format;

  status.seekOffset1 = (sizeof(uint16_t) * (bool)(format & FMT_VALID));  //
  status.seekOffset2 = (sizeof(float) * (bool)(format & FMT_TRACK)) +    //
                       (sizeof(uint16_t) * (bool)(format & FMT_DSTA)) +  //
                       (sizeof(float) * (bool)(format & FMT_DAGE)) +     //
                       (sizeof(uint16_t) * (bool)(format & FMT_PDOP)) +  //
                       (sizeof(uint16_t) * (bool)(format & FMT_HDOP)) +  //
                       (sizeof(uint16_t) * (bool)(format & FMT_VDOP)) +  //
                       (sizeof(uint16_t) * (bool)(format & FMT_NSAT));   //
  status.seekOffset3 = (sizeof(int16_t) * (bool)(format & FMT_ELE)) +    //
                       (sizeof(uint16_t) * (bool)(format & FMT_AZI)) +   //
                       (sizeof(uint16_t) * (bool)(format & FMT_SNR));    //
  status.seekOffset4 = (sizeof(uint16_t) * (bool)(format & FMT_MSEC)) +  //
                       (sizeof(double) * (bool)(format & FMT_DIST));     //

  Serial.printf("Parser.setFormat: change log format [reg=0x%08X, ignoreLen={%d, %d, %d, %d}]\n", status.logFormat,
                status.seekOffset1, status.seekOffset2, status.seekOffset3, status.seekOffset4);
}

void MtkParser::setOptions(parseopt_t opts) {
  const float OFFSET_MAX = 13.0;   // UTC+13
  const float OFFSET_MIN = -12.0;  // UTC-12

  if (opts.timeOffset > OFFSET_MAX) opts.timeOffset = OFFSET_MAX;
  if (opts.timeOffset < OFFSET_MIN) opts.timeOffset = OFFSET_MIN;

  memcpy(&options, &opts, sizeof(parseopt_t));
}

bool MtkParser::readBinRecord(gpsrecord_t *rcd) {
  // get the current position before reading a record
  uint32_t startPos = in->setMark();

  // clear given variable before use
  memset(rcd, 0, sizeof(gpsrecord_t));

  // store the current format
  rcd->format = status.logFormat;

  // read TIME field if it exists
  if (status.logFormat & FMT_TIME) {
    // get the value of the TIME field and correct the rollover
    rcd->time = in->readInt32();
    if (rcd->time < ROLLOVER_TIME) rcd->time += ROLLOVER_CORRECT;
  }

  // skip VALID field if it exists
  in->seekCur(status.seekOffset1);

  // read LAT, LON, ELE fields if they exist
  if (status.m241Mode) {  // for Holux M-241
    if (rcd->format & FMT_LAT) rcd->latitude = in->readFloat();
    if (rcd->format & FMT_LON) rcd->longitude = in->readFloat();
    if (rcd->format & FMT_HEIGHT) rcd->elevation = in->readFloat24();
  } else {  // for other standard models
    if (rcd->format & FMT_LAT) rcd->latitude = in->readDouble();
    if (rcd->format & FMT_LON) rcd->longitude = in->readDouble();
    if (rcd->format & FMT_HEIGHT) rcd->elevation = in->readFloat();
  }

  // read SPEED field if it exists
  if (rcd->format & FMT_SPEED) rcd->speed = in->readFloat() / 3.60;

  // skip TRACK, DSTA, DAGE, PDOP, HDOP, VDO, NSAT fields if they exist
  in->seekCur(status.seekOffset2);

  // skip SID, ELE + AZI + SNR fields if they exist
  if (rcd->format & FMT_SID) {
    // read the number of SATs in view (lower 8 bit of 32-bit int)
    uint8_t siv = (uint8_t)(in->readInt32() & 0x000000FF);  // number of SATs in view
    siv = (siv == 0xFF) ? 0 : siv;

    // skip SIV x (ELE + AZI + SNR) fields
    in->seekCur(status.seekOffset3 * siv);
  }

  // read RCR field if it exists
  if (rcd->format & FMT_RCR) {
    // get the lower 4 bits of the RCR field
    rcd->reason = (uint8_t)(in->readInt16() & 0b00001111);

    // RCR reason code
    // 0b0001: recorded by the time criteria
    // 0b0010: recorded by the distance criteria
    // 0b0100: recorded by the speed criteria
    // 0b1000: recorded by the user request (button press)
  }

  // skip MSEC, DIST fields if they exist
  in->seekCur(status.seekOffset4);

  // store the size of the record
  rcd->size = (in->position() - startPos);

  // calc current record size from FILE* position and calculate the checksum
  uint8_t checksum = 0;
  in->moveToMark();
  for (uint16_t i = 0; i < rcd->size; i++) {
    checksum ^= (uint8_t)in->readInt8();
  }

  // read the checksum delimiter '*' when the loSgger is not Holux M-241
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
      case DST_CHANGE_FORMAT:  // change format register
        // Serial.printf("Parser.readMarker: Change log format by DSP at 0x%05X [t=%d, v=0x%08X]\n", startPos,
        // dsType, dsVal);

        setRecordFormat(dsVal);
        break;
      case DST_LOG_STARTSTOP:  // log start(0x106), stop(0x0104)
        if ((dsVal == DSV_LOG_START) && (options.trackMode == TRK_AS_IS)) {
          out->endTrack();
          // Serial.printf("Parser.readMarker: Start a new track by DSP at 0x%05X [t=%d, v=0x%04X]\n",
          // startPos, dsType, dsVal);
        }
        break;
      case DST_CHANGE_METHOD:
      case DST_AUTOLOG_DIST:
      case DST_AUTOLOG_TIME:
      case DST_AUTOLOG_SPEED:
        // do nothing
        break;
      default:
        Serial.printf("Parser.readMarker: Unknown DSP type at 0x%05X [t=%d, v=0x%08X]\n", startPos, dsType, dsVal);
        break;
      }
    }
  } else if (matchBinPattern(PTN_M241, sizeof(PTN_M241))) {  // M-241
    matchBinPattern(PTN_M241_SP, sizeof(PTN_M241_SP));       // M-241 fw1.13

    status.m241Mode = true;
    match = true;

    Serial.printf("Parser.readMarker: M-241 marker at 0x%05X\n", startPos);
  } else if (matchBinPattern(PTN_SCT_END, sizeof(PTN_SCT_END))) {  // sector end
    // seek sector position to the next
    status.sectorPos += 1;
    match = true;

    Serial.printf("Parser.readMarker: sector end at 0x%05X\n", startPos);
  }

  return match;
}

bool MtkParser::convert(File32 *input, File32 *output, void (*progressCallback)(int32_t, int32_t)) {
  // clear all of the status variables
  memset(&status, 0, sizeof(parsestatus_t));

  in = new MtkFileReader(input);
  out = new GpxFileWriter(output);
  out->setTimeOffset(options.timeOffset);

  uint8_t progRate = 0;
  bool fileend = false;

  uint32_t startPos = 0;
  uint32_t currentPos = 0;
  uint32_t sectorStart = -1;
  uint32_t dataStart = 0;
  uint32_t sectorEnd = 0;

  if (progressCallback != NULL) progressCallback(0, in->filesize());

  while (true) {
    if (sectorStart != (SIZE_SECTOR * status.sectorPos)) {
      sectorStart = (SIZE_SECTOR * status.sectorPos);
      dataStart = (sectorStart + SIZE_HEADER);
      sectorEnd = (sectorStart + (SIZE_SECTOR - 1));
    }

    if (dataStart >= in->filesize()) {
      fileend = true;
      break;
    }

    if (progressCallback != NULL) {
      uint8_t cpr = 200 * ((float)in->position() / in->filesize());  // do callback 200 times during running process

      if (cpr > progRate) {  // if progress rate is increased
        progressCallback(in->position(), in->filesize());
        progRate = cpr;
      }
    }

    if (in->position() <= sectorStart) {
      in->seekCur(sectorStart - in->position());
      printf("Parser.convert: sector#%d start at 0x%05X, %d records\n", status.sectorPos, sectorStart,
             (uint16_t)in->readInt16());
      setRecordFormat(in->readInt32());
      in->seekCur(dataStart - in->position());
    }

    if (readBinMarkers()) continue;

    int32_t pos = in->position();
    gpsrecord_t rcd;
    if (!readBinRecord(&rcd)) {
      Serial.printf("Parser.convert: no valid data at 0x%05X\n", in->position());
      in->seekCur(1);
      continue;
    }

    if ((options.trackMode == TRK_ONE_DAY) && (isDifferentDate(status.lastTrkpt.time, rcd.time))) {
      out->endTrack();
    }

    // write the last lecord as TRKPT
    out->putTrkpt(rcd);

    if ((options.putWaypts) && (rcd.reason & RCR_LOG_BY_USER)) {
      out->addWaypt(rcd);
    }

    // store the first and last trkpt
    if (status.firstTrkpt.time == 0) {
      memcpy(&status.firstTrkpt, &rcd, sizeof(gpsrecord_t));
    }
    memcpy(&status.lastTrkpt, &rcd, sizeof(gpsrecord_t));

    if ((in->position() + rcd.size) >= sectorEnd) {
      status.sectorPos += 1;
    }
  }

  if (progressCallback != NULL) progressCallback(in->filesize(), in->filesize());

  out->endGpx();
  out->flush();

  status.trackCount = out->getTrackCount();
  status.trkptCount = out->getTrkptCount();
  status.wayptCount = out->getWayptCount();

  delete in;
  delete out;

  return (fileend);
}

gpsrecord_t MtkParser::getFirstTrkpt() {
  return status.firstTrkpt;
}

gpsrecord_t MtkParser::getLastTrkpt() {
  return status.lastTrkpt;
}

int32_t MtkParser::getTrackCount() {
  return status.trackCount;
}

int32_t MtkParser::getTrkptCount() {
  return status.trkptCount;
}

int32_t MtkParser::getWayptCount() {
  return status.wayptCount;
}
