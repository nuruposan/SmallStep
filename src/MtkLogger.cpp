#include "MtkLogger.h"

#define PROGRESS_STARTED 0
#define PROGRESS_FINISHED 100

/**
 * @fn MtkLogger::MtkLogger(const char *devname)
 * @brief Constructor of the MtkLogger class. Initialize the member variables and allocate the resources.
 * @param devname Name of the device for Bluetooth connection (default: "ESP32"; any name is acceptable).
 */
MtkLogger::MtkLogger(const char *devname) {
  deviceName = (devname == NULL) ? "ESP32" : devname;
  memset(&address, 0, sizeof(address));

  gpsSerial = new BluetoothSerial();
  buffer = new NmeaBuffer();
  sppStarted = false;

  // App may be crashed if gpsSerial.begin() is called in the constructor
  // (probably because of M5stack is not ready yet)
};

/**
 * @fn MtkLogger::~MtkLogger()
 * @brief Destructor of the MtkLogger class. Disconnect the current connection (if SPP is started) and release the
 * resources.
 */
MtkLogger::~MtkLogger() {
  // disconnect the current conection if it is still connected
  if (connected()) {
    disconnect();
    gpsSerial->end();
  }

  delete gpsSerial;
}

/**
 * @fn bool MtkLogger::connect(String name)
 * @brief Connect to the GPS logger of the specified name. The connection is established by the SPP (Serial Port
 * Profile) of Bluetooth Classic.
 * @param name Name of the GPS logger to connect.
 * @return Returns true if the connection is established successfully, otherwise false.
 */
bool MtkLogger::connect(String name) {
  if (!sppStarted) {
    sppStarted = gpsSerial->begin(deviceName, true);
  } else if (connected()) {
    disconnect();
  }

  Serial.printf("Logger.connect: connect to %s\n", name);

  gpsSerial->setTimeout(1000);
  gpsSerial->setPin("0000");

  if (eventCallback != NULL) {
    eventCallback(ESP_SPP_INIT_EVT, NULL);
  }

  bool conn = gpsSerial->connect(name);
  if ((!conn) && (eventCallback != NULL)) {
    eventCallback(ESP_SPP_UNINIT_EVT, NULL);
  }

  return conn;
}

/**
 * @fn bool MtkLogger::connect(uint8_t *addr)
 * @brief Connect to the GPS logger of the specified address. The connection is established by the SPP (Serial Port
 * Profile) of Bluetooth Classic.
 * @param addr Address of the GPS logger to connect.
 * @note The address is a 6-byte array that contains the MAC address of the GPS logger.
 * @return Returns true if the connection is established successfully, otherwise false.
 */
bool MtkLogger::connect(uint8_t *addr) {
  if (!sppStarted) {
    sppStarted = gpsSerial->begin("SmallStep", true);
  } else if (connected()) {
    disconnect();
  }

  Serial.printf(
      "Logger.connect: "
      "connect to logger %02X%02X-%02X%02X-%02X%02X%\n",
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  // register event callback function
  if (eventCallback != NULL) {
    eventCallback(ESP_SPP_INIT_EVT, NULL);
  }

  // start the SPP as master and set PIN code
  gpsSerial->setTimeout(1000);
  gpsSerial->setPin("0000");

  bool conn = gpsSerial->connect(addr);
  if ((!conn) && (eventCallback != NULL)) {
    eventCallback(ESP_SPP_UNINIT_EVT, NULL);
  }

  // connect to the GPS logger and return the result
  return conn;
}

/**
 * @fn bool MtkLogger::connected()
 * @brief Check if the GPS logger is connected or not.
 * @return Returns true if the GPS logger is connected, otherwise false.
 */
bool MtkLogger::connected() {
  bool conn = (sppStarted && gpsSerial->connected());
  return conn;
}

/**
 * @fn void MtkLogger::disconnect()
 * @brief Disconnect the current connection to the GPS logger.
 */
void MtkLogger::disconnect() {
  if (!connected()) return;

  gpsSerial->disconnect();
  buffer->clear();

  if (eventCallback != NULL) {
    eventCallback(ESP_SPP_UNINIT_EVT, NULL);
  }

  Serial.printf("Logger.disconnect: disconnected\n");
}

/**
 * @fn int32_t MtkLogger::firmwareIdToFlashSize(uint16_t firmwareId)
 * @brief Return the flash size of the GPS logger model specified by the firmware ID. The firmware ID is a 16-bit value
 * that is could be obtained by the PMTK_Q_RELEASE command (PMTK605).
 * @param firmwareId Firmware ID of the GPS logger model.
 * @return Returns the flash size of the GPS logger model. If the model is unknown, it is assumed as 32Mbit.
 */
int32_t MtkLogger::firmwareIdToFlashSize(uint16_t firmwareId) {
  int32_t flashSize = 0;
  switch (firmwareId) {
  // 8Mbit flash models
  case 0x1388:  // 757/ZI v1
  case 0x5202:  // 757/ZI v2
    flashSize = SIZE_8MBIT;
    break;

  // 16Mbit flash models
  case 0x0002:  // Qstarz 815
  case 0x001b:  // Transystem i-Blue 747
  case 0x001d:  // BT-Q1000, BGL-32
  case 0x0021:  // Holux M-241
  case 0x0023:  // Holux M-241
  case 0x0043:  // Holux M-241 (FW 1.13)
  case 0x0051:  // Transystem i-Blue 737, Qstarz 810, Polaris iBT-GPS, Holux M1000
  case 0x0131:  // EB-85A
    flashSize = SIZE_16MBIT;
    break;

  // 32Mbit flash models
  case 0x0000:  // Holux M-1200E
  case 0x0004:  // Transystem747 A+ GPS Trip Recorder
  case 0x0005:  // Qstarz BT-Q1000P
  case 0x0006:  // Transystem 747 A+ GPS Trip Recorder
  case 0x0008:  // Pentagram PathFinder P 3106
  case 0x000F:  // Transystem 747 A+ GPS Trip Recorder
  case 0x8300:  // Qstarz BT-1200
  default:      // assume 4Mbit for unknown models
    flashSize = SIZE_32MBIT;
    break;
  }

  return flashSize;
}

/**
 * @fn uint8_t MtkLogger::calcNmeaChecksum(const char *cmd)
 * @brief Calculate the checksum of the given NMEA command string. The checksum is calculated by XORing all bytes of the
 * string.
 * @param cmd A pointer to the NMEA command string without the leading '$' and the trailing '*' and checksum.
 * @return Returns the calculated checksum value of the given NMEA command.
 */
uint8_t MtkLogger::calcNmeaChecksum(const char *cmd) {
  uint8_t chk = 0;
  for (uint8_t i = 0; cmd[i] > 0; i++) {
    chk ^= (uint8_t)(cmd[i]);
  }

  return chk;
}

/**
 * @fn bool MtkLogger::sendNmeaCommand(const char *cmd)
 * @brief Send the given NMEA command to the GPS logger. The command is sent as a NMEA sentence with the leading '$' and
 * the trailing '*' and checksum.
 * @param cmd A pointer to the NMEA command string without the leading '$' and the trailing '*' and checksum.
 * @return Returns true if the command is sent successfully, otherwise false.
 */
bool MtkLogger::sendNmeaCommand(const char *cmd) {
  // return false if the GPS logger is not connected
  if (!connected()) return false;

  char sendbuf[128];
  sprintf(sendbuf, "$%s*%02X", cmd, calcNmeaChecksum(cmd));

  gpsSerial->flush();

  // send the NMEA command to the GPS logger
  for (uint16_t i = 0; sendbuf[i] != 0; i++) {
    gpsSerial->write(sendbuf[i]);
  }
  gpsSerial->write('\r');
  gpsSerial->write('\n');
  // Note: DO NOT gpsSerial->flush() here. It is unnecessary and cause a timeout error.

  // print the debug message
  Serial.printf("Logger.send: -> %s\n", sendbuf);

  return true;
}

/**
 * @fn bool MtkLogger::waitForNmeaReply(const char *reply, uint16_t timeout)
 * @brief Wait for the expected reply from the GPS logger. The reply is expected to be a NMEA sentence that matches the
 * given reply string.
 * @param reply A pointer to the expected reply string with or without the leading '$'.
 * @param timeout A uint16_t value that contains the timeout period in milliseconds.
 * @return Returns true if the expected reply is received within the timeout period, otherwise false.
 */
bool MtkLogger::waitForNmeaReply(const char *reply, uint16_t timeout) {
  // return false if the GPS logger is not connected
  if (!connected()) return false;
  if (reply == NULL) return false;

  if (timeout == 0) timeout = MSG_TIMEOUT;  // default timeout is MSG_TIMEOUT

  // set the timeout period
  uint32_t timeStartAt = millis();

  // wait for the reply from the GPS logger
  while (gpsSerial->connected()) {
    // exit loop if the timeout reached
    uint32_t timeElapsed = millis() - timeStartAt;
    if (timeElapsed > timeout) {
      Serial.printf("Logger.recv: ** timeout occured **\n");
      return false;
    }

    // read charactor until get a NMEA sentence
    if (!gpsSerial->available()) continue;
    if (!buffer->put(gpsSerial->read())) continue;

    // exit loop (and return true) if the expected response is detected
    if (buffer->match(reply)) {
      // put debug message
      Serial.printf("Logger.recv: <- %.64s\n", buffer->getBuffer());

      break;
    }
  }

  // return true if the expected reply is received
  return true;
}

/**
 * @fn bool MtkLogger::sendDownloadCommand(int startPos, int reqSize)
 * @brief Send the download command to the GPS logger to request the lod data from the specified address.
 * After sending the download command, the GPS logger sends multiple response beginning with "$PMTK182,8," prefix.
 * The response contains the starting address (in 3rd column) and the ASCII-encoded log data (in 4th column).
 * @param startPos A int value that contains the start address of the log data to download.
 * @param reqSize A int value that contains the size of the log data to download (It is recommended this value be a
 * multiple of 0x800. MTK logger sends the data in 0x800-byte blocks).
 * @return Returns true if the download command is sent successfully, otherwise false.
 */
bool MtkLogger::sendDownloadCommand(int startPos, int reqSize) {
  char cmdstr[32];
  sprintf(cmdstr, "PMTK182,7,%06X,%04X", startPos, reqSize);

  return sendNmeaCommand(cmdstr);
}

/**
 * @fn bool MtkLogger::getLastRecordAddress(int32_t *address)
 * @brief Determine the last address of the log data to be downloaded. The address is the address of the last log data
 * (FULLSTOP mode) or the flash size (OVERWRITE mode).
 * @return Returns true if the last address is obtained successfully, otherwise false.
 */
bool MtkLogger::getLastRecordAddress(int32_t *address) {
  const int32_t TIMEOUT = 1000;

  // Get the log record mode (FULLSTOP or OVERWRITE)
  recordmode_t recordMode = MODE_NONE;
  if (!getLogRecordMode(&recordMode)) return false;

  // Get the last address of the log data to be downloaded
  int32_t endAddr = 0;
  switch (recordMode) {
  case MODE_FULLSTOP:
    if (!sendNmeaCommand("PMTK182,2,8")) return false;
    if (!waitForNmeaReply("$PMTK182,3,8,", TIMEOUT)) return false;
    buffer->readColumnAsInt(3, &endAddr, true);

    waitForNmeaReply("$PMTK001,", ACK_TIMEOUT);
    break;
  case MODE_OVERWRITE:
  default:
    if (!getFlashSize(&endAddr)) return false;
    break;
  }

  // Set return value
  *address = endAddr;

  Serial.printf("Logger.lastAddr: 0x%06X\n", endAddr);

  // Return true if the last address is obtained
  return (endAddr > 0);
}

/**
 * @fn bool MtkLogger::resetCache(File32 *cache)
 * @brief Determine the resume position of the download process. The resume position is determined in units of data
 * sectors (multiple of 65536 bytes). If the cache file is invalid, the file is truncated and the function returns 0.
 * @param cache A pointer to the cache file object to store the downloaded data.
 * @return Returns the resume position of the download process in bytes. The content after the resume position in
 * the cache file is automatically truncated.
 */
int32_t MtkLogger::resetCache(File32 *cache) {
  const uint32_t CHECK_LEN = 0x200;
  uint32_t binFileSize = cache->fileSize();

  // Truncate the cache file if the file size is less than a sector size
  if (binFileSize < SIZE_SECTOR) {
    cache->truncate(0);
    return 0;
  }

  if (!sendDownloadCommand(0, SIZE_REPLY)) return -1;
  if (!waitForNmeaReply("$PMTK182,8,", 3000)) return -1;

  cache->seekCur(SIZE_HEADER);
  buffer->seekCurToColumn(3);  // move to the 4th column (data column)
  buffer->seekCur(SIZE_HEADER * 2);

  // Check the first 0x200 bytes of the cache file is the same as the received data.
  // If they are the same, the cache file is valid and can be used to resume the download.
  bool canResume = true;
  char fbuf[CHECK_LEN];
  cache->readBytes(fbuf, sizeof(fbuf));
  for (int16_t i = 0; i < sizeof(fbuf) - 1; i++) {
    byte dlby;
    buffer->readHexByteFull(&dlby);
    canResume &= (fbuf[i] == (char)dlby);
  }

  // Wait for the ACK responce of the download command
  waitForNmeaReply("$PMTK001,", ACK_TIMEOUT);

  // If the cache file is not valid, truncate the file and return 0
  if (!canResume) {
    cache->truncate(0);
    return 0;
  }

  // Determine the resume position of the download.
  // Check the number of records in each sector from the beginning, and if the value is 0xFFFF,
  // and if the value is 0xFFFF, determine that downloading is necessary from that sector.
  int32_t resumeAddr = 0;
  while (resumeAddr < cache->fileSize()) {
    cache->seek(resumeAddr);
    uint16_t records = (cache->read() << 8) + (cache->read());

    if (records == 0xFFFF) break;

    resumeAddr += SIZE_SECTOR;
  }

  Serial.printf("Logger.resetCache: using %d bytes of cache\n", resumeAddr);

  // Finally, truncate the content of the cache file after the resume position
  cache->truncate(resumeAddr);
  return resumeAddr;
}

/**
 * @fn book MtkLogger::downloadLogData(File32 *output, void (*progressCallback)(int32_t, int32_t))
 * @brief Download the log data from the GPS logger and store it in the given output / cache file.
 * The callback function is called to notify the progress of the download process.
 * @param output A pointer to the output file object to store the downloaded log data.
 * @param progressCallback A pointer to the callback function that is called to notify the progress of the download
 * process. The function should have two int32_t arguments that contain the current position and the end position of the
 * download process.
 * @return Returns true if the download process is finished successfully, otherwise false.
 */
bool MtkLogger::downloadLogData(File32 *output, void (*progressCallback)(int32_t, int32_t)) {
  const int8_t MAX_RETRIES = 3;
  const int32_t REQ_SIZE = 0x4000;
  const int32_t LOG_TIMEOUT1 = 3000;
  const int32_t LOG_TIMEOUT2 = 1000;

  bool nextReq = true;
  bool dataEnd = false;
  int32_t nextAddr = 0;  // the address of next data block to receive
  int32_t recvSize = 0;  // received data size for the last request
  int32_t endAddr = 0;   // the last address to download
  int8_t retries = 0;    // retry count
  int16_t timeout = LOG_TIMEOUT1;

  Serial.printf("Logger.download: initalizing\n");

  // get the last address to be downloaded (endAddr).
  // the address is the address of the last og data (STOP mode) or the flash size (OVERWRAP mode).
  // this value will be used for calculating the download progress rate.
  if (!getLastRecordAddress(&endAddr)) return false;
  endAddr = REQ_SIZE * ((endAddr / REQ_SIZE) + 1);

  // perform the callback to notify the download process is started
  if (progressCallback) progressCallback(nextAddr, endAddr);

  if ((nextAddr = resetCache(output)) == -1) return false;
  output->truncate(nextAddr);

  // perform the callback to notify the download process is started
  if (progressCallback) progressCallback(0, endAddr);

  Serial.printf("Logger.download: start [start=0x%06X, end=0x%06X, resume=%d] (t=%d)\n",  //
                nextAddr, endAddr, (nextAddr != 0), millis());

  while (gpsSerial->connected()) {
    // break if the download process is finished
    if (nextAddr >= endAddr) {
      // print the success message and
      Serial.printf("Logger.download: finished [end=0x%06X] (t=%d)\n",  //
                    endAddr, millis());
      break;
    }

    // send the next download request if the flag set
    if (nextReq) {
      // send the download command
      if (!sendDownloadCommand(nextAddr, REQ_SIZE)) return false;

      // reset the flag and the received size
      nextReq = false;
      recvSize = 0;
      timeout = LOG_TIMEOUT1;
    }

    // wait for the next data responce
    // abort the process if the expected responces are NOT received
    if (!waitForNmeaReply("$PMTK182,8,", timeout)) {
      Serial.printf("Logger.download: waiting for complete the request\n");

      uint16_t remainBlocks = ((REQ_SIZE - recvSize) / SIZE_REPLY) - 1;
      uint16_t waitTime = remainBlocks * LOG_TIMEOUT2;
      waitForNmeaReply("$PMTK001,", waitTime);

      if ((dataEnd) || (retries >= MAX_RETRIES)) break;

      nextReq = true;
      retries++;

      Serial.printf("Logger.download: retrying (%d/%d)\n", retries, MAX_RETRIES);
      continue;
    }

    timeout = LOG_TIMEOUT2;

    // read the starting address of the received data from the 3rd column of the line
    int32_t startAddr = 0;
    buffer->readColumnAsInt(2, &startAddr, true);

    // ignore this line if the starting address is not the expected value
    if (startAddr != nextAddr) continue;

    // perform the callback function to notify the progress
    if (progressCallback) progressCallback(nextAddr, endAddr);

    // read the data from the buffer and write it to the output file
    // and count the continuous 0xFFs to detect the end of the log data
    // (the download data will be ignored if endFlag is set)
    uint8_t by = 0;              // variable to store the next byte
    uint16_t ffCount = 0;        // counter for how many 0xFFs are continuous
    buffer->seekCurToColumn(3);  // move to the 4th column (data column)
    while ((!dataEnd) && (buffer->readHexByteFull(&by))) {
      output->write(by);

      ffCount = (by != 0xFF) ? 0 : (ffCount + 1);  // count continuous 0xFF
      dataEnd = (ffCount >= SIZE_HEADER);
    }  // while ((!dataEnd) ...)

    // update the next address and the received size
    nextAddr += SIZE_REPLY;
    recvSize += SIZE_REPLY;
    nextReq = (recvSize >= REQ_SIZE);

    // update the end address if dataEnd flag is set
    if (dataEnd) endAddr = nextAddr + (REQ_SIZE - recvSize);

    // wait for the next ACK responce if the size received is enough to send the next request
    if (recvSize >= REQ_SIZE) waitForNmeaReply("$PMTK001,", ACK_TIMEOUT);
  }  // while (gpsSerial.connected())

  // close the output file, then clear the buffer
  output->flush();
  buffer->clear();

  // finally perform the callback to notify the progress is completed
  if ((dataEnd) && (progressCallback)) progressCallback(endAddr, endAddr);

  // return true if the download process is finished successfully
  return ((dataEnd) || (nextAddr >= endAddr));
}

/**
 * @fn bool MtkLogger::fixRTCdatetime()
 * @brief Fix the RTC date and time of the GPS logger. The date and time are set to 2020-01-01 00:00:00 temporarily.
 * After setting the date and time, the GPS logger is restarted to apply the fix.
 * @return Returns true if the RTC date and time are fixed successfully, otherwise false.
 * @note After the fix, the GPS logger will recognize the current date and time from the GPS signal correctly.
 */
bool MtkLogger::fixRTCdatetime() {
  // send PMTK_API_SET_RTC_TIME (set date/time)
  if (!sendNmeaCommand("PMTK335,2020,1,1,0,0,0")) return false;
  if (!waitForNmeaReply("$PMTK001,335,3", MSG_TIMEOUT)) return false;

  // send PMTK_CMD_HOT_START command (reload request)
  if (!reloadDevice()) return false;

  return true;
}

/**
 * @fn bool MtkLogger::getFlashSize(int32_t *size)
 * @brief Get the flash size of the connected logger. The size is determined by the firmware release ID of the logger.
 * @param size A pointer to the variable to store the flash size.
 * @return True if the command is done successfully, otherwise false.
 */
bool MtkLogger::getFlashSize(int32_t *size) {
  // send a query command to get the firmware release ID
  if (!sendNmeaCommand("PMTK605")) return false;
  if (!waitForNmeaReply("$PMTK705,", MSG_TIMEOUT)) return false;

  // determine the flash size
  int32_t fwRelID = 0;
  buffer->readColumnAsInt(2, &fwRelID, true);  // read modelID from the reply
  *size = firmwareIdToFlashSize(fwRelID);      // determine flash size from modelID

  // print debug message
  Serial.printf("Logger.flashSize: 0x%06X\n", *size);

  waitForNmeaReply("$PMTK001,", ACK_TIMEOUT);
  return true;
}

/**
 * @fn bool MtkLogger::getLogFormat(uint32_t *format)
 * @brief Get the log format of the connected logger.
 * @param format A pointer to the variable to store the log format. The format is a combination of the values of the
 * logformat_t enumeration (e.g. FMT_FIXONLY | FMT_TIME | FMT_LON | FMT_LAT | FMT_HEIGHT | FMT_SPEED).
 * @return True if the command is done successfully, otherwise false.
 */
bool MtkLogger::getLogFormat(uint32_t *format) {
  if (!sendNmeaCommand("PMTK182,2,2")) return false;
  if (!waitForNmeaReply("$PMTK182,3,2,", MSG_TIMEOUT)) return false;

  // read the log format from the reply
  int32_t value = 0;
  buffer->readColumnAsInt(3, &value, true);
  *format = value;

  // print debug message
  Serial.printf("Logger.format: 0x%08X\n", *format);

  waitForNmeaReply("$PMTK001,", ACK_TIMEOUT);
  return true;
}

/**
 * @fn bool MtkLogger::setLogFormat(uint32_t format)
 * @brief Set the log format of the connected logger.
 * @param format The log format to set. The format is a combination of the values of the logformat_t enumeration
 * (e.g. FMT_FIXONLY | FMT_TIME | FMT_LON | FMT_LAT | FMT_HEIGHT | FMT_SPEED).
 * @note M-241 does not support this command. The command will be accepted but the format will not be changed.
 * @return True if the command is done successfully, otherwise false.
 */
bool MtkLogger::setLogFormat(uint32_t format) {
  char cmdstr[24];
  sprintf(cmdstr, "PMTK182,1,2,%08X", format);

  if (!sendNmeaCommand(cmdstr)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,3", MSG_TIMEOUT)) return false;

  return true;
}

/**
 * @fn bool MtkLogger::getLogRecordMode(recordmode_t *recmode)
 * @brief Get the logging record mode of the connected logger. The record mode is MODE_OVERWRITE or MODE_FULLSTOP.
 * @param recmode A pointer to the variable to store the record mode.
 * @return True if the command is done successfully, otherwise false.
 */
bool MtkLogger::getLogRecordMode(recordmode_t *recmode) {
  if (!sendNmeaCommand("PMTK182,2,6")) return false;
  if (!waitForNmeaReply("$PMTK182,3,6,", MSG_TIMEOUT)) return false;

  int32_t value = 0;
  buffer->readColumnAsInt(3, &value, true);
  *recmode = (recordmode_t)value;

  waitForNmeaReply("$PMTK001,", ACK_TIMEOUT);

  return (*recmode != MODE_NONE);
}

/**
 * @fn bool MtkLogger::setLogRecordMode(recordmode_t recmode)
 * @brief Set the logging record mode of the connected logger.
 * @param recmode The record mode to set. The mode can be set is MODE_OVERWRITE or MODE_FULLSTOP.
 * @return True if the command is done successfully, otherwise false.
 */
bool MtkLogger::setLogRecordMode(recordmode_t recmode) {
  char cmdstr[24];
  sprintf(cmdstr, "PMTK182,1,6,%d", recmode);

  if (!sendNmeaCommand(cmdstr)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,3", MSG_TIMEOUT)) return false;

  return true;
}

/**
 * @fn bool MtkLogger::getLogCriteria(logcriteria_t *criteria)
 * @brief Get the logging criteria of the connected logger. The criteria is a combination of distance, time, and speed.
 * @param criteria A pointer to the variable to store the log criteria.
 * @return true if the command is done successfully, otherwise false.
 */
bool MtkLogger::getLogCriteria(logcriteria_t *criteria) {
  if (!getLogByDistance(&criteria->distance)) return false;
  if (!getLogByTime(&criteria->time)) return false;
  if (!getLogBySpeed(&criteria->speed)) return false;

  return true;
}

/**
 * @fn bool MtkLogger::setLogCriteria(logcriteria_t criteria)
 * @brief Set the logging criteria of the connected logger. The criteria is a combination of distance, time, and speed.
 * @param criteria The log criteria to set.
 * @return True if the command is done successfully, otherwise false.
 */
bool MtkLogger::setLogCriteria(logcriteria_t criteria) {
  if (!setLogByDistance(criteria.distance)) return false;
  if (!setLogByTime(criteria.time)) return false;
  if (!setLogBySpeed(criteria.speed)) return false;

  return true;
}

/**
 * @fn bool MtkLogger::getLogByDistance(int16_t *dist)
 * @brief Get log by distance parameter of the connected logger.
 * @param time A pointer to the variable to store the distance value. the unit is in meters (100 means 100 meters)
 * @return true if the command is done successfully, otherwise false.
 */
bool MtkLogger::getLogByDistance(int16_t *dist) {
  if (!sendNmeaCommand("PMTK182,2,4")) return false;
  if (!waitForNmeaReply("$PMTK182,3,4,", MSG_TIMEOUT)) return false;

  int32_t value = 0;
  buffer->readColumnAsInt(3, &value, false);
  *dist = value;

  Serial.printf("Logger.logDist: %d meter\n", (*dist / 10));

  waitForNmeaReply("$PMTK001,", ACK_TIMEOUT);
  return true;
}
/**
 * @fn bool MtkLogger::setLogByDistance(int16_t distance)
 * @brief Set log by distance parameter of the connected logger.
 * @param time The distance value to set. The unit is in seconds (100 means 100 meters). If the specified value is
 * negative, it will be set to 0 (disabled).
 * @return True if the command is done successfully, otherwise false.
 */
bool MtkLogger::setLogByDistance(int16_t distance) {
  if (distance < 0) distance = 0;

  char buf[24];
  sprintf(buf, "PMTK182,1,4,%d", distance);  // log by distance

  if (!sendNmeaCommand(buf)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,", MSG_TIMEOUT)) return false;

  return true;
}

/**
 * @fn bool MtkLogger::getLogByTime(int16_t *time)
 * @brief Get log by time parameter of the connected logger.
 * @param time A pointer to the variable to store the time value. The unit is in seconds (300 means 300 seconds)
 * @return True if the command is done successfully, otherwise false.
 */
bool MtkLogger::getLogByTime(int16_t *time) {
  if (!sendNmeaCommand("PMTK182,2,3")) return false;
  if (!waitForNmeaReply("$PMTK182,3,3,", MSG_TIMEOUT)) return false;

  int value = 0;
  buffer->readColumnAsInt(3, &value, false);
  *time = value;

  Serial.printf("Logger.logTime: %d.%d sec\n", (*time / 10), (*time % 10));

  waitForNmeaReply("$PMTK001,", ACK_TIMEOUT);
  return true;
}

/**
 * @fn bool MtkLogger::setLogBySpeed(int16_t speed)
 * @brief Set log by speed of the connected logger.
 * @param speed The speed value to set. The unit is in 0.1km/h (1000 means 100 km/h). If the specified value is
 * negative, it will be set to 0 (disabled).
 * @return true if the command is done successfully, otherwise false.
 */
bool MtkLogger::setLogBySpeed(int16_t speed) {
  if (speed < 0) speed = 0;

  char buf[24];
  sprintf(buf, "PMTK182,1,5,%d", speed);  // log by speed

  if (!sendNmeaCommand(buf)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,", MSG_TIMEOUT)) return false;

  waitForNmeaReply("$PMTK001,", ACK_TIMEOUT);
  return true;
}

/**
 * @fn bool MtkLogger::getLogBySpeed(int16_t *speed)
 * @brief Get log by speed parameter of the connected logger.
 * @param speed A pointer to the variable to store the speed value. The unit is in 0.1km/h (1000 means 100 km/h).
 * If the specified value is negative, it will be set to 0 (disabled).
 * @return true if the command is done successfully, otherwise false.
 */
bool MtkLogger::getLogBySpeed(int16_t *speed) {
  if (!sendNmeaCommand("PMTK182,2,5")) return false;
  if (!waitForNmeaReply("$PMTK182,3,5,", MSG_TIMEOUT)) return false;

  int32_t value = 0;
  buffer->readColumnAsInt(3, &value, false);
  *speed = value;

  Serial.printf("Logger.logSpeed: %d km/h\n", (*speed / 10));

  waitForNmeaReply("$PMTK001,", ACK_TIMEOUT);
  return true;
}

/**
 * @fn bool MtkLogger::setLogByTime(int16_t time)
 * @brief Set log by time parameter of the connected logger
 * @param time the time value to set. the unit is in seconds (300 means 300 seconds). if the value is negative,
 * it will be set to 0 (disabled).
 * @return true if the command is done successfully, otherwise false.
 */
bool MtkLogger::setLogByTime(int16_t time) {
  if (time < 0) time = 0;

  char buf[24];
  sprintf(buf, "PMTK182,1,3,%d", time);  // log by time

  if (!sendNmeaCommand(buf)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,", MSG_TIMEOUT)) return false;

  return true;
}

/**
 * @brief Send a command to clear the flash memory of the connected logger.
 * This function takes some time and calls a callback function periodically during the process.
 * @param rateCallback a function pointer to a callback function
 * @return true if the flash memory is cleared successfully, otherwise false.
 */
bool MtkLogger::clearFlash(void (*rateCallback)(int32_t, int32_t)) {
  const uint32_t CALLBACK_INTERVAL = 500;
  const uint32_t TIME_PER_8MBIT = 8000;  // uint: msec

  // return false if the GPS logger is not connected
  if (!connected()) return false;

  int32_t flashSize = 0;
  if (!getFlashSize(&flashSize)) return false;

  uint32_t timeStartAt = millis();
  uint32_t timeElapsed = 0;
  uint32_t timeEstimated = (flashSize / SIZE_8MBIT) * TIME_PER_8MBIT;

  // perform the callback to notify the process is started
  if (rateCallback) rateCallback(0, (timeEstimated / 1000));

  // send clear command to the loggger
  if (!sendNmeaCommand("PMTK182,6,1")) return false;

  while (gpsSerial->connected()) {
    if (waitForNmeaReply("$PMTK001,182,6,3", CALLBACK_INTERVAL)) {  // successfully finished
      break;
    }

    timeElapsed = millis() - timeStartAt;
    if (timeElapsed > timeEstimated) {
      Serial.println("Logger.clearFlash: timeout occurred");
      break;
    }

    // perform the callback to notify the current progress rate
    if (rateCallback) rateCallback((timeElapsed / 1000), (timeEstimated / 1000));
  }

  // perform the callback to notify the process is finiched
  if (rateCallback) {
    timeElapsed = millis() - timeStartAt;
    rateCallback((timeElapsed / 1000), (timeElapsed / 1000));
  }

  Serial.println("Logger.clearFlash: done");

  return true;
}

/**
 * @fn void MtkLogger::setEventCallback(esp_spp_cb_t cbfunc)
 * @brief Set the event callback (bluetooth event handler) function. The function will be called when Bluetooth SPP
 * status is changed.
 * @param cbfunc a function pointer to the event callback function.
 */
void MtkLogger::setEventCallback(esp_spp_cb_t cbfunc) {
  if (cbfunc != NULL) {
    gpsSerial->register_callback(cbfunc);
  }

  eventCallback = cbfunc;
}

/**
 * @fn bool MtkLogger::reloadDevice()
 * @brief Send a hot start command to the connected logger to reload the device.
 * @return true if the command is done successfully, otherwise false.
 */
bool MtkLogger::reloadDevice() {
  const int32_t RESET_TIMEOUT = 3000;

  // send PMTK_CMD_HOT_START command (reload request)
  if (!sendNmeaCommand("PMTK101")) return false;
  if (!waitForNmeaReply("$PMTK010,", RESET_TIMEOUT)) return false;

  Serial.println("Logger.reload: done");

  return true;
}
