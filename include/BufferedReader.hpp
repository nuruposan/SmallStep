
#include <SdFat.h>

class BufferedReader {
 private:
  static const uint16_t BUF_LEN = 512;
  char buf[BUF_LEN];
  uint16_t readPos;

 public:
  BufferedReader(File32 input);
  ~BufferedReader();
}