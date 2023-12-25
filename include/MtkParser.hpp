#pragma once

#include <stdio.h>

typedef struct _fileinfo {
  char name[80];
  FILE *fp;
  fpos_t size;
} fileinfo_t;

typedef enum _trackmode {
  TRK_PUT_AS_IS = 0,
  TRK_ONE_DAY = 1,
  TRK_SINGLE = 2
} trackmode_t;

typedef enum _speedmode {
  SPD_PUT_AS_IS = 0,    // record speed as is
  SPD_CALC_ALWAYS = 1,  // always (re)calclate speed with position and time
  SPD_DROP_FIELD = 2    // do not record speed
} speedmode_t;

typedef enum _timemode {
  TIM_PUT_AS_IS = 0,  // record time as is
  TIM_FIX_ROLLOVER =
      1,  // correct time that is affected the rollover problem and record it
  TIM_DROP_FIELD = 2  // do not record time
} timemode_t;

typedef struct _parseopt {
  bool m241;
  trackmode_t track;
  speedmode_t speed;
  timemode_t time;
  uint32_t offset;
} parseopt_t;

typedef struct _gpsrecord {
  uint32_t format;
  uint32_t time;
  double latitude;
  double longitude;
  float elevation;
  float speed;
  bool valid;
} gpsrecord_t;

typedef struct _proginfo {
  uint16_t sector;
  uint32_t format;
  bool m241bin;
  gpsrecord_t firstRecord;
  gpsrecord_t lastRecord;
  bool newTrack;
  bool inTrack;
} proginfo_t;

class MtkParser {
 private:
  static const uint32_t SIZE_HEADER;
  static const uint32_t SIZE_SECTOR;

  static const uint32_t ROLLOVER_OCCUR_TIME;
  static const uint32_t ROLLOVER_CORRECT_VALUE;

  static const char PTN_DSET_AA[];
  static const char PTN_DSET_BB[];
  static const char PTN_M241[];
  static const char PTN_M241_SP[];
  static const char PTN_SCT_END[];

  fileinfo_t binFile;
  fileinfo_t gpxFile;
  parseopt_t options;
  proginfo_t progress;

  // double calcDistance(gpsrecord_t *r1, gpsrecord_t *r2) {
  static fpos_t getFileSize(FILE *fp);
  bool isSpanningDate(uint32_t t1, uint32_t t2);
  bool matchBinPattern(const char *ptn, uint8_t len);
  double readBinDouble();
  float readBinFloat24();
  float readBinFloat();
  int32_t readBinInt8();
  int32_t readBinInt16();
  int32_t readBinInt32();
  bool readBinMarkers();
  bool readBinRecord(gpsrecord_t *rcd);
  static void printFormatRegister_d(uint32_t fmt);
  static void printGpsRecord_d(gpsrecord_t *rcd);
  void putGpxString(const char *line);
  void putGpxXmlStart();
  void putGpxXmlEnd();
  void putGpxTrackStart();
  void putGpxTrackEnd();
  void putGpxTrackPoint(gpsrecord_t *rcd);
  void setFormatRegister(uint32_t fmt);

 public:
  MtkParser();
  ~MtkParser();
  uint8_t openBinFile(const char *name);
  uint8_t openGpxFile(const char *name);
  // void setBinFileName(const char *name);
  // void setGpxFileName(const char *name);
  uint32_t setTimezoneOffset(uint32_t tzo);
  void closeBinFile();
  void closeGpxFile();
  bool run(uint16_t token);
};
