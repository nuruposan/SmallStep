#include "MtkParser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

MtkParser::MtkParser(parseopt_t opts) {
  memset(&options, 0, sizeof(parseopt_t));
  memset(&status, 0, sizeof(parsestatus_t));

  setOptions(opts);
}

bool MtkParser::isDifferentDate(uint32_t t1, uint32_t t2) {
  uint32_t offsetSec = (options.timeOffset * 3600);

  t1 += offsetSec;
  t2 += offsetSec;

  return ((t1 / 86400) != (t2 / 86400));
}

void MtkParser::setRecordFormat(uint32_t format) {
  status.logFormat = format;

  status.ignoreSize1 = (sizeof(uint16_t) * (bool)(format & FMT_VALID));
  status.ignoreSize2 = (sizeof(float) * (bool)(format & FMT_TRACK)) +    //
                       (sizeof(uint16_t) * (bool)(format & FMT_DSTA)) +  //
                       (sizeof(float) * (bool)(format & FMT_DAGE)) +     //
                       (sizeof(uint16_t) * (bool)(format & FMT_PDOP)) +  //
                       (sizeof(uint16_t) * (bool)(format & FMT_HDOP)) +  //
                       (sizeof(uint16_t) * (bool)(format & FMT_VDOP)) +  //
                       (sizeof(uint16_t) * (bool)(format & FMT_NSAT));
  status.ignoreSize3 = (sizeof(int16_t) * (bool)(format & FMT_ELE)) +   //
                       (sizeof(uint16_t) * (bool)(format & FMT_AZI)) +  //
                       (sizeof(uint16_t) * (bool)(format & FMT_SNR));
  status.ignoreSize4 = (sizeof(uint16_t) * (bool)(format & FMT_MSEC)) +  //
                       (sizeof(double) * (bool)(format & FMT_DIST));

  Serial.printf("Parser.setFormat: change log format [reg=0x%08X]\n", status.logFormat);
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
  uint32_t startPos = in->mark();

  // clear given variable before use
  memset(rcd, 0, sizeof(gpsrecord_t));

  // store the current format
  rcd->format = status.logFormat;

  // read TIME field if it exists
  if (status.logFormat & FMT_TIME) {
    // get the value of the TIME field and correct the rollover
    in->read(&rcd->time, sizeof(uint32_t));
    if (rcd->time < ROLLOVER_TIME) rcd->time += ROLLOVER_CORRECT;
  }

  // skip VALID field if exists
  in->seek(status.ignoreSize1);

  // read LAT, LON, ELE fields if they exist
  if (status.m241Mode) {  // for Holux M-241
    if (rcd->format & FMT_LAT) in->read(&rcd->latitude, 4, (sizeof(double) - 4));
    if (rcd->format & FMT_LON) in->read(&rcd->longitude, 4, (sizeof(double) - 4));
    if (rcd->format & FMT_HEIGHT) in->read(&rcd->elevation, 1, ((sizeof(float) - 1)));
  } else {  // for other standard models
    if (rcd->format & FMT_LAT) in->read(&rcd->latitude, sizeof(double));
    if (rcd->format & FMT_LON) in->read(&rcd->longitude, sizeof(double));
    if (rcd->format & FMT_HEIGHT) in->read(&rcd->elevation, sizeof(float));
  }

  // read SPEED field if it exists
  if (rcd->format & FMT_SPEED) {
    in->read(&rcd->speed, sizeof(float));
    rcd->speed /= 3.60;
  }

  // skip TRACK, DSTA, DAGE, PDOP, HDOP, VDO, NSAT fields if exist
  in->seek(status.ignoreSize2);

  // skip SID, ELE + AZI + SNR fields if exist
  if (rcd->format & FMT_SID) {
    // read the number of SATs in view (lower 8 bit of 32-bit int)
    uint32_t siv = 0;
    in->read(&siv, sizeof(uint32_t));
    siv = siv & 0x000000FF;

    // skip SIV x (ELE + AZI + SNR) fields
    siv = (siv == 0xFF) ? 0 : siv;  // 0xFF means there is no SIV
    in->seek(status.ignoreSize3 * siv);
  }

  // read RCR field if exists
  if (rcd->format & FMT_RCR) {
    // get the lower 4 bits of RCR field
    in->read(&rcd->reason, sizeof(uint16_t));
    rcd->reason = (rcd->reason & 0x000F);
  }

  // skip MSEC and DIST fields if exist
  in->seek(status.ignoreSize4);

  // calc current record size from FILE* position and calculate the checksum
  uint8_t checksum = in->checksum();

  // read the checksum delimiter '*' when the logger is not Holux M-241
  // nore: Holux M-241 does not write '*' before checksum value
  char chkmkr = '*';
  if (!status.m241Mode) in->read(&chkmkr, sizeof(char));

  // verify the checksum is correct or not
  uint8_t chkval;
  in->read(&chkval, sizeof(uint8_t));
  rcd->valid = ((chkmkr == '*') && (chkval == checksum));

  // finally, store the size of the record
  rcd->size = (in->position() - startPos);

  // if there is no valid record here, restore the original position
  if (!rcd->valid) in->jump();

  // put debug message
  // Serial.printf("time=%d, lat=%f, lon=%f, rcr=0x%x, chk={0x%02x/0x%02x}\n",  //
  //              rcd->time, rcd->latitude, rcd->longitude, rcd->reason, chkval, checksum);

  // return the result of the record reading
  return rcd->valid;
}

bool MtkParser::matchBinPattern(const char *ptn, uint8_t len) {
  // get the current position before matching the pattern
  in->mark();

  // check if the pattern is matched from the current position
  bool match = true;
  for (int i = 0; ((i < len) && (match)); i++) {
    char val;
    in->read(&val, sizeof(char));

    match = match && (val == ptn[i]);
  }

  // restore the position if the pattern is not matched
  if (!match) in->jump();

  // return the result of the matching
  return match;
}

bool MtkParser::readBinMarkers() {
  bool match = false;
  size_t startPos = in->mark();

  if (matchBinPattern(PTN_DSET_AA, sizeof(PTN_DSET_AA))) {  // DSP_A
    // read type and value of the DSP
    dspdata_t dsp;
    in->read(&dsp.type, sizeof(uint8_t));
    in->read(&dsp.value, sizeof(uint32_t));

    if (matchBinPattern(PTN_DSET_BB, sizeof(PTN_DSET_BB))) {  // DSP_B
      match = true;

      // do action according to the DSP type
      switch (dsp.type) {
      case DST_CHANGE_FORMAT:  // change format register
        setRecordFormat(dsp.value);
        break;
      case DST_LOG_STARTSTOP:  // log start(0x106), stop(0x0104)
        if ((dsp.value == DSV_LOG_START) && (options.trackMode == TRK_AS_IS)) {
          out->endTrack();
        }
        break;
      case DST_CHANGE_METHOD:
      case DST_AUTOLOG_DIST:
      case DST_AUTOLOG_TIME:
      case DST_AUTOLOG_SPEED:
        // just ingnore these types
        break;
      default:
        Serial.printf("Parser.readMarker: Unknown DSP type at 0x%06X [t=%d, v=0x%08X]\n",  //
                      startPos, dsp.type, dsp.value);
        break;
      }
    }
  } else if (matchBinPattern(PTN_M241, sizeof(PTN_M241))) {  // M-241
    matchBinPattern(PTN_M241_SP, sizeof(PTN_M241_SP));       // M-241 fw1.13

    status.m241Mode = true;
    match = true;

    Serial.printf("Parser.readMarker: M-241 marker at 0x%06X\n", startPos);
  } else if (matchBinPattern(PTN_SCT_END, sizeof(PTN_SCT_END))) {  // sector end
    // seek sector position to the next
    status.sectorPos += 1;
    match = true;

    Serial.printf("Parser.readMarker: sector end at 0x%06X\n", startPos);
  }

  return match;
}

gpxinfo_t MtkParser::convert(File32 *input, File32 *output, void (*progressCallback)(int32_t, int32_t)) {
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
      in->seek(sectorStart - in->position());

      uint16_t nos;
      in->read(&nos, sizeof(uint16_t));
      printf("Parser.convert: sector#%d start at 0x%06X, %d records\n",  //
             status.sectorPos, sectorStart, (uint16_t)nos);

      uint32_t fmt;
      in->read(&fmt, sizeof(uint32_t));
      setRecordFormat(fmt);
      in->seek(dataStart - in->position());
    }

    if (readBinMarkers()) continue;

    int32_t pos = in->position();
    gpsrecord_t rcd;
    if (!readBinRecord(&rcd)) {
      Serial.printf("Parser.convert: no valid data at 0x%06X\n", in->position());
      in->seek(1);
      continue;
    }

    if ((options.trackMode == TRK_ONE_DAY) && (isDifferentDate(out->getLastTime(), rcd.time))) {
      out->endTrack();
    }

    // write the last lecord as TRKPT
    out->putTrkpt(rcd);

    if ((options.putWaypts) && (rcd.reason & RCR_LOG_BY_USER)) {
      out->addWaypt(rcd);
    }

    if ((in->position() + rcd.size) >= sectorEnd) {
      status.sectorPos += 1;
    }
  }

  if (progressCallback != NULL) progressCallback(in->filesize(), in->filesize());

  gpxinfo_t gpxInfo = out->endGpx();

  delete in;
  delete out;

  return gpxInfo;
}
