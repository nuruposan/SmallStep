#include "MtkLogger.h"

#define PROGRESS_STARTED 0
#define PROGRESS_FINISHED 100

MtkLogger::MtkLogger(const char *devname) {
  deviceName = (devname == NULL) ? "ESP32" : devname;
  memset(&address, 0, sizeof(address));
  gpsSerial = new BluetoothSerial();
  buffer = new NmeaBuffer();
  sppStarted = false;

  // App may be crashed if gpsSerial.begin() is called in the constructor
  // (probably because of M5stack is not ready yet)
};

MtkLogger::~MtkLogger() {
  // disconnect the current conection if it is still connected
  if (connected()) {
    disconnect();
    gpsSerial->end();
  }

  delete gpsSerial;
}

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

bool MtkLogger::connected() {
  bool conn = (sppStarted && gpsSerial->connected());
  return conn;
}

void MtkLogger::disconnect() {
  if (!connected()) return;

  gpsSerial->disconnect();
  buffer->clear();

  if (eventCallback != NULL) {
    eventCallback(ESP_SPP_UNINIT_EVT, NULL);
  }

  Serial.printf("Logger.disconnect: disconnected\n");
}

int32_t MtkLogger::modelIdToFlashSize(uint16_t modelId) {
  int32_t flashSize = 0;
  switch (modelId) {
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

uint8_t MtkLogger::calcNmeaChecksum(const char *cmd) {
  uint8_t chk = 0;
  for (uint8_t i = 0; cmd[i] > 0; i++) {
    chk ^= (uint8_t)(cmd[i]);
  }

  return chk;
}

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
  // Note: DO NOT call gpsSerial->flush() here!!

  // print the debug message
  Serial.printf("Logger.send: -> %s\n", sendbuf);

  return true;
}

bool MtkLogger::waitForNmeaReply(const char *reply, uint16_t timeout) {
  // return false if the GPS logger is not connected
  if (!connected()) return false;
  if (reply == NULL) return false;

  if (timeout == 0) timeout = 500;  // default timeout is 500 msec

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
      Serial.printf("Logger.recv: <- %.80s\n", buffer->getBuffer());

      break;
    }
  }

  // return true if the expected reply is received
  return true;
}

bool MtkLogger::sendDownloadCommand(int startPos, int reqSize) {
  char cmdstr[32];
  sprintf(cmdstr, "PMTK182,7,%06X,%04X", startPos, reqSize);

  return sendNmeaCommand(cmdstr);
}

bool MtkLogger::getLastRecordAddress(int32_t *address) {
  const int32_t TIMEOUT = 1000;

  // get the log record mode (FULLSTOP or OVERWRITE)
  recordmode_t recordMode = MODE_NONE;
  if (!getLogRecordMode(&recordMode)) return false;

  // get the last address of the log data to be downloaded
  int32_t endAddr = 0;
  switch (recordMode) {
  case MODE_FULLSTOP:
    if (!sendNmeaCommand("PMTK182,2,8")) return false;
    if (!waitForNmeaReply("$PMTK182,3,8,", TIMEOUT)) return false;
    buffer->readColumnAsInt(3, &endAddr, true);

    waitForNmeaReply("$PMTK001,", ACK_WAIT);
    break;
  case MODE_OVERWRITE:
  default:
    if (!getFlashSize(&endAddr)) return false;
    break;
  }

  // set return value
  *address = endAddr;

  // return true if the last address is obtained
  return (endAddr > 0);
}

int32_t MtkLogger::resetCache(File32 *cache) {
  const uint32_t CHECK_LEN = 0x100;
  uint32_t binFileSize = cache->fileSize();

  if (binFileSize < SIZE_SECTOR) {
    cache->truncate(0);
    return 0;
  }

  if (!sendDownloadCommand(0, SIZE_REPLY)) return -1;
  if (!waitForNmeaReply("$PMTK182,8,", 3000)) return -1;

  cache->seekCur(SIZE_HEADER);
  buffer->seekToColumn(3);  // move to the 4th column (data column)
  buffer->seek(SIZE_HEADER * 2);

  // 最初のデータフィールドの先頭0x100バイト(0x200-0x2FF)が同一なら前回の続きと見なす
  bool canResume = true;
  char fbuf[CHECK_LEN];
  cache->readBytes(fbuf, sizeof(fbuf));
  for (int16_t i = 0; i < sizeof(fbuf) - 1; i++) {
    byte dlby;
    buffer->readHexByteFull(&dlby);
    canResume &= (fbuf[i] == (char)dlby);
  }

  // ACKメッセージを待つ (間に合わずタイムアウトになっても問題ない)
  waitForNmeaReply("$PMTK001,", ACK_WAIT);

  // キャッシュが以前のものではない場合0を返す
  if (!canResume) {
    cache->truncate(0);
    return 0;
  }

  // レジューム開始位置を決定
  // * 後ろから2つ目のセクターの先頭2バイトが0xFFFF(セクタ内のレコード数が未確定)であればそのセクターを再開位置とする
  // * 0xFFFF以外(そのセクターのレコード数が確定済み)であれば次の最終セクターをダウンロード再開位置とする
  // (0x00, 0x00, ... 0x00, 0x00 がセンター境界にきている場合、最終の一つ手前のセクターの末尾はまだ更新される)
  int16_t sectors = cache->fileSize() / SIZE_SECTOR;
  int32_t resumeAddr = 0;
  for (int16_t i = 0; i < sectors; i++) {
    cache->seek(resumeAddr);
    uint16_t records = (cache->read() << 8) + (cache->read());

    if (records == 0xFFFF) break;

    resumeAddr += SIZE_SECTOR;
  }

  Serial.printf("Logger.resetCache: using %d bytes of cache\n", resumeAddr);

  // finally seek to the potision to resume
  cache->truncate(resumeAddr);
  return resumeAddr;
}

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

  Serial.printf("Logger.download: started [start=0x%06X, end=0x%06X, resume=%d] (t=%d)\n",  //
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
    uint8_t by = 0;           // variable to store the next byte
    uint16_t ffCount = 0;     // counter for how many 0xFFs are continuous
    buffer->seekToColumn(3);  // move to the 4th column (data column)
    while ((!dataEnd) && (buffer->readHexByteFull(&by))) {
      output->write(by);
      ffCount = (by != 0xFF) ? 0 : (ffCount + 1);  // count continuous 0xFF
      dataEnd = (ffCount >= SIZE_HEADER);
    }  // while ((!dataEnd) ...)

    // update the next address and the received size
    nextAddr += SIZE_REPLY;
    recvSize += SIZE_REPLY;
    nextReq = (recvSize >= REQ_SIZE);
    if (dataEnd) {
      endAddr = nextAddr + (REQ_SIZE - recvSize);
      Serial.printf("Logger.download: found the end of data\n");
    }

    // wait for the next ACK responce if the size received is enough to send the next request
    if (recvSize >= REQ_SIZE) waitForNmeaReply("$PMTK001,", ACK_WAIT);
  }  // while (gpsSerial.connected())

  // close the output file, then clear the buffer
  output->flush();
  buffer->clear();

  // finally perform the callback to notify the progress is completed
  if ((dataEnd) && (progressCallback)) progressCallback(endAddr, endAddr);

  // return true if the download process is finished successfully
  return ((dataEnd) || (nextAddr >= endAddr));
}

bool MtkLogger::fixRTCdatetime() {
  // send PMTK_API_SET_RTC_TIME (set date/time)
  if (!sendNmeaCommand("PMTK335,2020,1,1,0,0,0")) return false;
  if (!waitForNmeaReply("$PMTK001,335,3", TIMEOUT)) return false;

  // send PMTK_CMD_HOT_START command (reload request)
  if (!reloadDevice()) return false;

  return true;
}

bool MtkLogger::getFlashSize(int32_t *size) {
  // send query firmware release
  if (!sendNmeaCommand("PMTK605")) return false;
  if (!waitForNmeaReply("$PMTK705,", TIMEOUT)) return false;

  // determine the flash size
  int32_t modelId = 0;
  buffer->readColumnAsInt(2, &modelId, true);  // read modelID from the reply
  *size = modelIdToFlashSize(modelId);         // determine flash size from modelID

  // print debug message
  Serial.printf("Logger.getFlashSize: flash size = 0x%08X\n", *size);

  waitForNmeaReply("$PMTK001,", ACK_WAIT);
  return true;
}

bool MtkLogger::getLogFormat(uint32_t *format) {
  if (!sendNmeaCommand("PMTK182,2,2")) return false;
  if (!waitForNmeaReply("$PMTK182,3,2,", TIMEOUT)) return false;

  // read the log format from the reply
  int32_t value = 0;
  buffer->readColumnAsInt(3, &value, true);
  *format = value;

  // print debug message
  Serial.printf("Logger.getLogFormat: log format = 0x%08X\n", *format);

  waitForNmeaReply("$PMTK001,", ACK_WAIT);
  return true;
}

bool MtkLogger::setLogFormat(uint32_t format) {
  char cmdstr[24];
  sprintf(cmdstr, "PMTK182,1,2,%08X", format);

  if (!sendNmeaCommand(cmdstr)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,3", TIMEOUT)) return false;

  return true;
}

bool MtkLogger::getLogRecordMode(recordmode_t *recmode) {
  if (!sendNmeaCommand("PMTK182,2,6")) return false;
  if (!waitForNmeaReply("$PMTK182,3,6,", TIMEOUT)) return false;

  int32_t value = 0;
  buffer->readColumnAsInt(3, &value, true);
  *recmode = (recordmode_t)value;

  waitForNmeaReply("$PMTK001,", ACK_WAIT);
  return (*recmode != MODE_NONE);
}

bool MtkLogger::setLogRecordMode(recordmode_t recmode) {
  char cmdstr[24];
  sprintf(cmdstr, "PMTK182,1,6,%d", recmode);

  if (!sendNmeaCommand(cmdstr)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,3", TIMEOUT)) return false;

  return true;
}

bool MtkLogger::getLogCriteria(logcriteria_t *criteria) {
  if (!getLogByDistance(&criteria->distance)) return false;
  if (!getLogByTime(&criteria->time)) return false;
  if (!getLogBySpeed(&criteria->speed)) return false;

  return true;
}

bool MtkLogger::setLogCriteria(logcriteria_t criteria) {
  if (!setLogByDistance(criteria.distance)) return false;
  if (!setLogByTime(criteria.time)) return false;
  if (!setLogBySpeed(criteria.speed)) return false;

  return true;
}

bool MtkLogger::getLogByDistance(int16_t *dist) {
  if (!sendNmeaCommand("PMTK182,2,4")) return false;
  if (!waitForNmeaReply("$PMTK182,3,4,", TIMEOUT)) return false;

  int32_t value = 0;
  buffer->readColumnAsInt(3, &value, false);
  *dist = value;

  Serial.printf("Logger.getLogByDistance: %d\n", (*dist / 10));

  waitForNmeaReply("$PMTK001,", ACK_WAIT);
  return true;
}
/**
 * Set log by distance parameter of the connected logger
 * @param time the distance value to set. the unit is in seconds (100 => 100 meters)
 */

bool MtkLogger::setLogByDistance(int16_t distance) {
  if (distance < 0) distance = 0;

  char buf[24];
  sprintf(buf, "PMTK182,1,4,%d", distance);  // log by distance

  if (!sendNmeaCommand(buf)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,", TIMEOUT)) return false;

  return true;
}

bool MtkLogger::getLogByTime(int16_t *time) {
  if (!sendNmeaCommand("PMTK182,2,3")) return false;
  if (!waitForNmeaReply("$PMTK182,3,3,", TIMEOUT)) return false;

  int value = 0;
  buffer->readColumnAsInt(3, &value, false);
  *time = value;

  Serial.printf("Logger.getLogByTime: %d.%d sec\n", (*time / 10), (*time % 10));

  waitForNmeaReply("$PMTK001,", ACK_WAIT);
  return true;
}

/**
 * Set log by speed of the connected logger
 * @param speed the speed value to set. the unit is in 0.1km/h (1000 => 100 km/h)
 */
bool MtkLogger::setLogBySpeed(int16_t speed) {
  if (speed < 0) speed = 0;

  char buf[24];
  sprintf(buf, "PMTK182,1,5,%d", speed);  // log by speed

  if (!sendNmeaCommand(buf)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,", TIMEOUT)) return false;

  waitForNmeaReply("$PMTK001,", ACK_WAIT);
  return true;
}

/**
 *
 */
bool MtkLogger::getLogBySpeed(int16_t *speed) {
  if (!sendNmeaCommand("PMTK182,2,5")) return false;
  if (!waitForNmeaReply("$PMTK182,3,5,", TIMEOUT)) return false;

  int32_t value = 0;
  buffer->readColumnAsInt(3, &value, false);
  *speed = value;

  Serial.printf("Logger.getLogBySpeed: %d km/h\n", (*speed / 10));

  waitForNmeaReply("$PMTK001,", ACK_WAIT);
  return true;
}

/**
 * Set log by time parameter of the connected logger
 * @param time the time value to set. the unit is in seconds (300 => 300 seconds)
 */
bool MtkLogger::setLogByTime(int16_t time) {
  if (time < 0) time = 0;

  char buf[24];
  sprintf(buf, "PMTK182,1,3,%d", time);  // log by time

  if (!sendNmeaCommand(buf)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,", TIMEOUT)) return false;

  return true;
}

/**
 * Clear the flash memory of the connected logger.
 * This function takes some time and calls a callback function periodically during the process.
 * @param rateCallback a function pointer to a callback function
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

  return true;
}

void MtkLogger::setEventCallback(esp_spp_cb_t cbfunc) {
  if (cbfunc != NULL) {
    gpsSerial->register_callback(cbfunc);
  }

  eventCallback = cbfunc;
}

bool MtkLogger::reloadDevice() {
  const int32_t RESET_TIMEOUT = 3000;

  // send PMTK_CMD_HOT_START command (reload request)
  if (!sendNmeaCommand("PMTK101")) return false;
  if (!waitForNmeaReply("$PMTK010,", RESET_TIMEOUT)) return false;

  return true;
}
