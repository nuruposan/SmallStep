#include "MtkLogger.h"

#define PROGRESS_STARTED 0
#define PROGRESS_FINISHED 100

MtkLogger::MtkLogger() {
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
    sppStarted = gpsSerial->begin("SmallStep", true);
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
    case 0x0051:  // Transystem i-Blue 737, Qstarz 810,
                  // Polaris iBT-GPS, Holux M1000
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

  // send the NMEA command to the GPS logger
  for (uint16_t i = 0; sendbuf[i] != 0; i++) {
    gpsSerial->write(sendbuf[i]);
  }
  gpsSerial->write('\r');
  gpsSerial->write('\n');
  // Note: DO NOT call gpsSerial->flush() here!!

  // print the debug message
  Serial.printf("Logger.sendNmeaCommand: -> %s\n", sendbuf);

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
    if (timeElapsed > timeout) return false;

    // read charactor until get a NMEA sentence
    if (!gpsSerial->available()) continue;
    if (!buffer->put(gpsSerial->read())) continue;

    // print the debug message to the serial console
    Serial.printf("Logger.waitForNmeaReply: <- %s\n", buffer->getBuffer());

    // exit loop (and return true) if the expected response is received
    if (buffer->match(reply)) break;
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
  const int32_t TIMEOUT = 500;

  // get the log record mode (FULLSTOP or OVERWRITE)
  recordmode_t recordMode = MODE_NONE;
  if (!getLogRecordMode(&recordMode)) return false;

  // get the last address of the log data to be downloaded
  int32_t endAddr = 0;
  switch (recordMode) {
  case MODE_FULLSTOP:
    if (!sendNmeaCommand("PMTK182,2,8")) return false;
    if (!waitForNmeaReply("$PMTK182,3,8,", TIMEOUT)) return false;
    
    buffer->readColumnAsInt(3, &endAddr);
    break;
  case MODE_OVERWRITE:
  default:
    // send PMTK_Q_RELEASE command (query the firmware release info)
    if (!sendNmeaCommand("PMTK605")) return false;
    if (!waitForNmeaReply("$PMTK705,", TIMEOUT)) return false;

    int32_t modelId = 0;
    buffer->readColumnAsInt(2, &modelId);  // read modelID
    endAddr = modelIdToFlashSize(modelId);
    break;
  }

  // set return value
  *address = endAddr;

  Serial.printf("Logger.getLastRecordAddress: 0x%06X\n", endAddr);

  // return true if the last address is obtained
  return (endAddr > 0);
}

bool MtkLogger::downloadLogData(File32 *output, void (*rateCallback)(int8_t)) {
  const int32_t TIMEOUT = 1200;
  int32_t REQ_SIZE = 0x4000;

  bool nextReq = true;
  int32_t recvSize = 0;
  int32_t endAddr  = 0;
  int32_t nextAddr = 0;
  
  // perform the callback to notify the download process is started
  if (rateCallback) rateCallback(PROGRESS_STARTED);

  // get the last address of the log data to be downloaded
  // to calcurate the download progress rate
  if (!getLastRecordAddress(&endAddr)) return false;

  while (gpsSerial->connected()) {
    // break if the download process is finished
    if (nextAddr >= endAddr) break;

    // send the next download request if the flag set
    if (nextReq) {
      // send the download command
      if (!sendDownloadCommand(nextAddr, REQ_SIZE)) return false;

      nextReq = false;
      recvSize = 0;
    }

    // wait for the next data responce
    // abort the process if one of expected responces are NOT received
    if (!waitForNmeaReply("$PMTK182,8,", TIMEOUT)) break;

    // read the starting address of the received data from the 3rd column of the line
    int32_t startAddr = 0;
    buffer->readColumnAsInt(2, &startAddr);

    // ignore this line if the starting address is not the expected value
    if (startAddr != nextAddr) continue;

    // call the callback function to notify the progress
    if (rateCallback) rateCallback(((float)nextAddr / endAddr) * 100);

    // print the debug message to the serial console
    Serial.printf("Logger.download: recv data [startAddr=0x%06X]\n", startAddr);

    uint8_t b = 0;         // variable to store the next byte
    uint16_t ffCount = 0;  // counter for how many 0xFFs are continuous

    // read the data from the buffer and write it to the output file
    // and count the continuous 0xFFs to detect the end of the log data
    buffer->seekToColumn(3);  // move to the 4th column (data column)
    while (buffer->readHexByteFull(&b)) {
      output->write(b);
      ffCount = (b != 0xFF) ? 0 : (ffCount + 1);  // count continuous 0xFF
    }

    // update the next address and the received size
    nextAddr += 0x0800;
    recvSize += 0x0800;
    nextReq = (recvSize >= REQ_SIZE);
    if (ffCount >= 0x200) endAddr = nextAddr + (REQ_SIZE - recvSize);
//    if (ffCount >= 0x200) endAddr = nextAddr;

    // exit loop when download process is finished or timeout occurred
    if (nextAddr >= endAddr) {
      Serial.printf("Logger.download: finished [recvSize=0x%06X]\n", endAddr);
      break;
    }
  } // while (gpsSerial.connected())

  // close the output file, then clear the buffer
  output->flush();
  buffer->clear();

  // finally perform the callback to notify the progress is completed
  if (rateCallback) rateCallback(PROGRESS_FINISHED);

  // return true if the download process is finished successfully
  return (nextAddr >= endAddr);
}

bool MtkLogger::fixRTCdatetime() {
  const uint32_t TIMEOUT = 1000;

  // send PMTK_API_SET_RTC_TIME (set date/time)
  if (!sendNmeaCommand("PMTK335,2020,1,1,0,0,0")) return false;
  if (!waitForNmeaReply("$PMTK001,335,3", TIMEOUT)) return false;

  // send PMTK_CMD_HOT_START command (reload request)
  if (!reloadDevice()) return false;

  return true;
}

bool MtkLogger::getFlashSize(int32_t *size) {
  const uint32_t TIMEOUT = 1000;

  // send query firmware release
  if (!sendNmeaCommand("PMTK605")) return false;
  if (!waitForNmeaReply("$PMTK705,", TIMEOUT)) return false;

  // determine the flash size
  int32_t modelId = 0;
  buffer->readColumnAsInt(2, &modelId); // read modelID from the reply
  *size = modelIdToFlashSize(modelId);  // determine flash size from modelID

  // print debug message
  Serial.printf("Logger.getFlashSize: flash size = 0x%08X\n", *size);

  return true;
}

bool MtkLogger::getLogFormat(uint32_t *format) {
  const uint32_t TIMEOUT = 1000;

  if (!sendNmeaCommand("PMTK182,2,2")) return false;
  if (!waitForNmeaReply("$PMTK182,3,2,", TIMEOUT)) return false;

  // read the log format from the reply
  int32_t value = 0;
  buffer->readColumnAsInt(3, &value);
  *format = value;

  // print debug message
  Serial.printf("Logger.getLogFormat: log format = 0x%08X\n", *format);
  
  return true;
}

bool MtkLogger::setLogFormat(uint32_t format) {
  const uint32_t TIMEOUT = 1000;

  char cmdstr[24];
  sprintf(cmdstr, "PMTK182,1,2,%08X", format);
  
  if (!sendNmeaCommand(cmdstr)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,3", TIMEOUT)) return false;

  return true;
}

bool MtkLogger::getLogRecordMode(recordmode_t *recmode) {
  const uint32_t TIMEOUT = 1000;

  int32_t value = 0;
  if (!sendNmeaCommand("PMTK182,2,6")) return false;
  if (!waitForNmeaReply("$PMTK182,3,6,", TIMEOUT)) return false;

  buffer->readColumnAsInt(3, &value);
  Serial.printf("Logger.getLogFullAction: action = %02d (2=STOP, 1=OVERWRAP)\n", value);
  
  *recmode = (recordmode_t)value;

  return (*recmode != MODE_NONE);
}

bool MtkLogger::setLogRecordMode(recordmode_t recmode) {
  const uint32_t TIMEOUT = 1000;

  char cmdstr[24];
  sprintf(cmdstr, "PMTK182,1,6,%d", recmode);

  if (!sendNmeaCommand(cmdstr)) return false;
  if (!waitForNmeaReply("$PMTK001,182,1,3", TIMEOUT)) return false;

  return true;
}


bool MtkLogger::clearFlash(void (*rateCallback)(int8_t)) {
  const uint32_t TIMEOUT = 500;

  // return false if the GPS logger is not connected
  if (!connected()) return false;

  // perform the callback to notify the process is started
  if (rateCallback) rateCallback(PROGRESS_STARTED);

  int32_t flashSize = 0;
  if (!getFlashSize(&flashSize)) return false;

  int32_t progRate = 0;
  uint32_t timeStartAt = millis();
  uint32_t timeEstimated = (flashSize / SIZE_8MBIT) * 8000;

  // send clear 
  if (!sendNmeaCommand("PMTK182,6,1")) return false;

  while (gpsSerial->connected()) {
    if (waitForNmeaReply("$PMTK001,182,6,3", TIMEOUT)) { // successfully finished
      // reload the logger and break
      if (!reloadDevice()) return false;
      break;
    }

    uint32_t timeElapsed = millis() - timeStartAt;
    if (timeElapsed > timeEstimated) {
      Serial.println("Logger.clearFlash: timeout occurred");
      break;
    }

    // perform the callback to notify the current progress rate
    if (rateCallback) rateCallback(((float)timeElapsed / timeEstimated) * 100);
  }

  // perform the callback to notify the process is finiched
  if (rateCallback) rateCallback(PROGRESS_FINISHED);

  return true;
}

void MtkLogger::setEventCallback(esp_spp_cb_t cbfunc) {
  if (cbfunc != NULL) {
    gpsSerial->register_callback(cbfunc);
  }

  eventCallback = cbfunc;
}

bool MtkLogger::reloadDevice() {
  const int32_t TIMEOUT = 3000;

  // send PMTK_CMD_HOT_START command (reload request)
  if (!sendNmeaCommand("PMTK101")) return false;
  if (!waitForNmeaReply("$PMTK010,", TIMEOUT)) return false;

  return true;
}