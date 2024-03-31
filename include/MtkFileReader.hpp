
#include <SdFat.h>

class MtkFileReader {
 private:
  static const uint16_t PAGE_SIZE = 512;  // > 512 (MtkParser.HEADER_SIZE)
  static const uint16_t PAGE_CENTER = (PAGE_SIZE / 2);
  static const uint16_t BUF_SIZE = (PAGE_SIZE * 2);

  char buf[BUF_SIZE];
  File32 *in;
  uint32_t pos;
  uint16_t ptr;
  uint32_t mpos;

  int8_t read();

 public:
  MtkFileReader();
  ~MtkFileReader();
  uint32_t backToMarkPos();
  uint32_t filesize();
  void init(File32 *f);
  uint32_t markPos();
  uint32_t position();
  float readFloat();
  float readFloat24();
  double readDouble();
  int32_t readInt8();
  int32_t readInt16();
  int32_t readInt32();
  uint32_t seekCur(uint16_t mv);
};
