#include "MtkParser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @fn MtkParser::MtkParser(parseopt_t opts)
 * @brief Constructor of the MtkParser class.
 * @param opts A parseopt_t structure that contains the parsing options to set.
 */
MtkParser::MtkParser(parseopt_t opts) {
  memset(&options, 0, sizeof(parseopt_t));
  memset(&status, 0, sizeof(parsestatus_t));

  setOptions(opts);
}

/**
 * @fn bool isDifferentDate(uint32_t t1, uint32_t t2)
 * @brief Compare timestamps to determine if they are in different dates or same date. The comparison is based in the
 * local time.
 * @return Returns true if the two timestamps are in different dates, otherwise false.
 */
bool MtkParser::isDifferentDate(uint32_t t1, uint32_t t2) {
  uint32_t offsetSec = (options.timeOffset * 3600);

  t1 += offsetSec;
  t2 += offsetSec;

  return ((t1 / 86400) != (t2 / 86400));
}

/**
 * @fn bool MtkParser::setRecordFormat(uint32_t format)
 * @brief Set the record format for the parser. The format may be dynamically changed during the parsing process.
 * The format value is written in the sector header or the DSP pattern in the sector body.
 * @param format A uint32_t value that contains the record format to set.
 */
void MtkParser::setRecordFormat(uint32_t format) {
  // return if format is unchanged
  if (status.logFormat == format) return;

  status.logFormat = format;
  status.ignoreSize1 = (sizeof(uint16_t) * (bool)(format & FMT_VALID));
  status.ignoreSize2 = (sizeof(float) * (bool)(format & FMT_TRACK)) +    //
                       (sizeof(uint16_t) * (bool)(format & FMT_DSTA)) +  //
                       (sizeof(float) * (bool)(format & FMT_DAGE)) +     //
                       (sizeof(uint16_t) * (bool)(format & FMT_PDOP)) +  //
                       (sizeof(uint16_t) * (bool)(format & FMT_HDOP)) +  //
                       (sizeof(uint16_t) * (bool)(format & FMT_VDOP)) +  //
                       (sizeof(uint16_t) * (bool)(format & FMT_NSAT));   //
  status.ignoreSize3 = (sizeof(int16_t) * (bool)(format & FMT_ELE)) +    //
                       (sizeof(uint16_t) * (bool)(format & FMT_AZI)) +   //
                       (sizeof(uint16_t) * (bool)(format & FMT_SNR));    //
  status.ignoreSize4 = (sizeof(uint16_t) * (bool)(format & FMT_MSEC)) +  //
                       (sizeof(double) * (bool)(format & FMT_DIST));     //

  Serial.printf("Parser.setFormat: change format [reg=0x%08X]\n", format);
}

/**
 * @fn void MtkParser::setOptions(parseopt_t opts)
 * @brief Set the options for the parser.
 * @param opts A parseopt_t structure that contains the options to set.
 */
void MtkParser::setOptions(parseopt_t opts) {
  const float OFFSET_MAX = 13.0;   // UTC+13
  const float OFFSET_MIN = -12.0;  // UTC-12

  if (opts.timeOffset > OFFSET_MAX) opts.timeOffset = OFFSET_MAX;
  if (opts.timeOffset < OFFSET_MIN) opts.timeOffset = OFFSET_MIN;

  memcpy(&options, &opts, sizeof(parseopt_t));
}

/**
 * @fn bool MtkParser::readBinRecord(gpsrecord_t *rcd)
 * @brief Read a record from the current posision in the input file. The record is stored in the given gpsrecord_t
 * struct. If the record is valid, the position is moved to the next record. Otherwise, the position is restored to the
 * original position.
 * @param rcd A pointer to the gpsrecord_t structure to store the read record.
 * @return Returns true if the valid record is found, otherwise false.
 */
bool MtkParser::readBinRecord(gpsrecord_t *rcd) {
  // Try to read a record from the input file.
  // Node: A record is a variable-length binary data. The format is specified in the sector header
  // or the DSP pattern in the sector body.

  // get the current position before reading a record
  uint32_t startPos = in->mark();

  // clear given variable before use
  memset(rcd, 0, sizeof(gpsrecord_t));

  // store the current format
  rcd->format = status.logFormat;

  // read TIME field if it exists
  if (status.logFormat & FMT_TIME) {
    // get the value of the TIME field and correct the rollover
    in->readBytes(&rcd->time, sizeof(uint32_t));
    if (rcd->time < ROLLOVER_TIME) rcd->time += ROLLOVER_CORRECT;
  }

  // skip VALID field if exists
  in->seekCur(status.ignoreSize1);

  // read LAT, LON, ELE fields if they exist
  if (status.m241Mode) {  // for Holux M-241
    if (rcd->format & FMT_LAT) rcd->latitude = (double)in->getFloat();
    if (rcd->format & FMT_LON) rcd->longitude = (double)in->getFloat();
    if (rcd->format & FMT_HEIGHT) rcd->altitude = (float)in->getFloat24();
  } else {  // for other standard models
    if (rcd->format & FMT_LAT) in->readBytes(&rcd->latitude, sizeof(double));
    if (rcd->format & FMT_LON) in->readBytes(&rcd->longitude, sizeof(double));
    if (rcd->format & FMT_HEIGHT) in->readBytes(&rcd->altitude, sizeof(float));
  }

  // read SPEED field if it exists
  if (rcd->format & FMT_SPEED) {
    in->readBytes(&rcd->speed, sizeof(float));
    rcd->speed /= 3.60;
  }

  // skip TRACK, DSTA, DAGE, PDOP, HDOP, VDO, NSAT fields if exist
  in->seekCur(status.ignoreSize2);

  // skip SID, ELE + AZI + SNR fields if exist
  if (rcd->format & FMT_SID) {
    // read the number of SATs in view (lower 8 bit of 32-bit int)
    uint32_t siv = 0;
    in->readBytes(&siv, sizeof(uint32_t));
    siv = siv & 0x000000FF;

    // skip SIV x (ELE + AZI + SNR) fields
    siv = (siv == 0xFF) ? 0 : siv;  // 0xFF means there is no SIV
    in->seekCur(status.ignoreSize3 * siv);
  }

  // read RCR field if exists
  if (rcd->format & FMT_RCR) {
    // get the lower 4 bits of RCR field
    in->readBytes(&rcd->reason, sizeof(uint16_t));
    rcd->reason = (rcd->reason & 0x000F);
  }

  // skip MSEC and DIST fields if exist
  in->seekCur(status.ignoreSize4);

  // calc current record size from FILE* position and calculate the checksum
  uint8_t checksum = in->checksum();

  // read checksum delimiter '*' and checksum field
  // Note: Holux M-241 does not have '*' before checksum field
  char chkmkr = '*';
  if (!status.m241Mode) in->readBytes(&chkmkr, sizeof(char));
  uint8_t chkval;
  in->readBytes(&chkval, sizeof(uint8_t));

  // verify the checksum is correct or not
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

/**
 * @fn bool MtkParser::matchBinPattern(const char *ptn, uint8_t len)
 * @brief Match the given pattern from the current position in the input file. If the pattern is matched, the position
 * is moved to the next byte of the pattern. Otherwise, the position is restored to the original position.
 * @param ptn A pointer to the pattern to match.
 * @param len A uint8_t value that contains the length of the pattern.
 * @retuen Returns true if the pattern is matched, otherwise false.
 */
bool MtkParser::matchBinPattern(const char *ptn, uint8_t len) {
  // get the current position before matching the pattern
  in->mark();

  // check if the pattern is matched from the current position
  bool match = true;
  for (int i = 0; ((i < len) && (match)); i++) {
    char val;
    in->readBytes(&val, sizeof(char));

    match = match && (val == ptn[i]);
  }

  // restore the position if the pattern is not matched
  if (!match) in->jump();

  // return the result of the matching
  return match;
}

/**
 * @fn bool MtkParser::readBinMarkers()
 * @brief Read special patterns from the current position in the input file. If any pattern is matched, apply the
 * nessesacy actions and move to the next position. Otherwise, restore the original position and do nothing.
 * @return Returns true if the pattern is matched and the action is applied, otherwise false.
 */
bool MtkParser::readBinMarkers() {
  // Special patterns in the MTK binary log data
  // * Dynamic Settings Pattern (DSP): {0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA TT VV VV VV 0xBB 0xBB 0xBB 0xBB}
  // * Holux M-241 pattern: "HOLUXGR241LOGGER"
  // * Holux M-241 fw1.13 pattern: "HOLUXGR241LOGGER    "
  // * Sector end pattern: {0xFF 0xFF .. 0xFF (16 bytes)}

  bool match = false;
  size_t startPos = in->mark();

  // try to read a DSP from the current position
  if (matchBinPattern(PTN_DSET_AA, sizeof(PTN_DSET_AA))) {  // matched DSP_A
    // read type and value of the DSP
    dspdata_t dsp;
    in->readBytes(&dsp.type, sizeof(uint8_t));
    in->readBytes(&dsp.value, sizeof(uint32_t));

    if (matchBinPattern(PTN_DSET_BB, sizeof(PTN_DSET_BB))) {  // matched DSP_B
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

      default:  // all other types
        break;
      }

      Serial.printf("Parser.readMarker: Found a DSP [t=%d, v=0x%04X] at 0x%06X\n",  //
                    dsp.type, dsp.value, startPos);
      return true;
    }
  }
  // try to read a HOLUX M-241 pattern from the current position
  else if (matchBinPattern(PTN_M241, sizeof(PTN_M241))) {  // M-241
    matchBinPattern(PTN_M241_SP, sizeof(PTN_M241_SP));     // M-241 fw1.13

    if (!status.m241Mode) {
      status.m241Mode = true;
      Serial.printf("Parser.readMarker: Found a m-241 marker at 0x%06X\n", startPos);
    }
    return true;
  }
  // try to read a Sector-end pattern from the current position
  else if (matchBinPattern(PTN_SCT_END, sizeof(PTN_SCT_END))) {  // sector end
    // seek sector position to the next
    status.sectorPos += 1;

    Serial.printf("Parser.readMarker: Found the EoS at 0x%06X\n", startPos);
    return true;
  }

  // if no special pettern found, return false
  return false;
}

/**
 * @fn gpxinfo_t MtkParser::convert(File32 *input, File32 *output, void (*progressCallback)(int32_t, int32_t))
 * @brief Read a GPS data record from the current position in the input file. The read record is valid, write it as a
 * TRKPT into the output file and move to the next position. Otherwise, move the position to the next byte.
 * @param input
 * @param output
 * @param progressCallback
 */
gpxinfo_t MtkParser::convert(File32 *input, File32 *output, void (*progressCallback)(int32_t, int32_t)) {
  // clear the status before starting the conversion
  memset(&status, 0, sizeof(parsestatus_t));

  // create the input and output file objects
  in = new MtkFileReader(input);
  out = new GpxFileWriter(output);
  out->setTimeOffset(options.timeOffset);  // set the time offset to the output

  // set the initial values for the progress callback
  uint8_t progRate = 0;
  bool fileEnd = false;

  // set the initial values for the sector and data position
  uint32_t startPos = 0;
  uint32_t currentPos = 0;
  uint32_t sectorStart = -1;
  uint32_t dataStart = 0;
  uint32_t sectorEnd = 0;

  // call the progress callback function with the initial values
  if (progressCallback != NULL) progressCallback(0, in->filesize());

  // print the start message to the serial monitor
  Serial.printf("Parser.convert: start (t=%d)\n", millis());

  while (true) {
    if (sectorStart != (SIZE_SECTOR * status.sectorPos)) {
      sectorStart = (SIZE_SECTOR * status.sectorPos);
      dataStart = (sectorStart + SIZE_HEADER);
      sectorEnd = (sectorStart + (SIZE_SECTOR - 1));
    }

    if (dataStart >= in->filesize()) {
      fileEnd = true;
      break;
    }

    if (progressCallback != NULL) {
      uint8_t cpr = 200 * ((float)in->position() / in->filesize());  // callback 200 times during running process

      if (cpr > progRate) {  // if progress rate is increased
        progressCallback(in->position(), in->filesize());
        progRate = cpr;
      }
    }

    if (in->position() <= sectorStart) {
      in->seekCur(sectorStart - in->position());

      uint16_t nos;
      in->readBytes(&nos, sizeof(uint16_t));
      Serial.printf("Parser.convert: begin sector [id=%d, addr=0x%06X, rcds=%d]\n",  //
                    status.sectorPos, sectorStart, (uint16_t)nos);

      uint32_t fmt;
      in->readBytes(&fmt, sizeof(uint32_t));
      setRecordFormat(fmt);
      in->seekCur(dataStart - in->position());
    }

    // try to read the markers (special petterns such as a Dynamic Settings Patterm (DSP),
    // HOLUX M-241 Pattern or Data-end Pattern) first.
    // if the pattern is matched, apply the action and continue to the next position,
    // otherwise, try to read a record data.
    if (readBinMarkers()) continue;

    // read a record data
    int32_t pos = in->position();  // store the current position to restore if the record is invalid
    gpsrecord_t rcd;
    if (!readBinRecord(&rcd)) {  // read the record and if it is invalid
      // no valid pattern or record at the current position, seek 1 byte and continue
      // Note: doing this means the downloaded data is corrupted or the parser is wrong
      in->seekCur(1);
      continue;
    }

    // close the track if the track mode is TRK_ONE_DAY and the current record is a new day
    if ((options.trackMode == TRK_ONE_DAY) && (isDifferentDate(out->getLastTime(), rcd.time))) {
      out->endTrack();
    }

    // write the record data as TRKPT
    out->putTrkpt(rcd);

    // store the current record to write as a waypt if the putWaypts option is enabled and
    // the record is logged by user
    // Note: the waypt is written at the back of the track when it closed
    if ((options.putWaypts) && (rcd.reason & RCR_LOG_BY_USER)) {
      out->addWaypt(rcd);
    }

    // set the sector position to the next if the record is the last in the sector
    if ((in->position() + rcd.size) >= sectorEnd) {
      status.sectorPos += 1;
    }
  }

  // call the progress callback function with the final values
  if (progressCallback != NULL) progressCallback(in->filesize(), in->filesize());

  // get the GPX information before closing the output
  gpxinfo_t gpxInfo = out->endGpx();

  // close the input and output files
  delete in;
  delete out;

  // print the finish message to the serial monitor
  Serial.printf("Parser.convert: finished [trk=%d, trkpt=%d, wpt=%d] (t=%d)\n",  //
                gpxInfo.trackCount, gpxInfo.trkptCount, gpxInfo.wayptCount,      //
                (gpxInfo.endTime - gpxInfo.startTime), millis());

  // return the GPX information
  return gpxInfo;
}
