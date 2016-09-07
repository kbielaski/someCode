#include "serializer.h"
#include <iostream>
#include <stdlib.h>

void test_write8(serializer &ser,uint8_t v) {
  if (ser.write_uint8(v)) {
    std::cout << v << " ";
  } else {
    std::cerr << "bad uint8 write\n";
    exit(-1);
  }
}

void test_read8(serializer &ser) {
  uint8_t t;
  if (ser.read_uint8(t)) {
    std::cout << t << " ";
  } else {
    std::cerr << "bad uint8 read\n";
    exit(-1);
  }
}

void test_read64(serializer &ser) {
  uint64_t t;
  if (ser.read_uint64(t)) {
    std::cout << t << " ";
  } else {
    std::cerr << "bad uint64 read\n";
    exit(-1);
  }
}

void test_write64(serializer &ser,uint64_t v) {
  if (ser.write_uint64(v)) {
    std::cout << v << " ";
  } else {
    std::cerr << "bad uint64 write\n";
    exit(-1);
  }
}

void test_reads9(serializer &ser) {
  uint32_t t;
  if (0 == ser.read_s9(t)) {
    std::cout << t << " ";
  } else {
    std::cerr << "bad s9 read\n";
    exit(-1);
  }
}

void test_writes9(serializer &ser,uint32_t v) {
  if (ser.write_s9(v)) {
    std::cout << v << " ";
  } else {
    std::cerr << "bad s9 write\n";
    exit(-1);
  }
}

void test_writev64(serializer &ser,uint64_t v) {
  if (ser.write_vbyte64(v)) {
    std::cout << v << " ";
  } else {
    std::cerr << "bad vbyte64 write\n";
    exit(-1);
  }
}

void test_readv64(serializer &ser) {
  uint64_t v;
  if (ser.read_vbyte64(v)) {
    std::cout << v << " ";
  } else {
    std::cerr << "bad vbyte64 read\n";
    exit(-1);
  }
}

void test_s9seq(serializer &ser, uint32_t *v, int count) {
  uint64_t pos=ser.pos();
  std::cout << "writing s9:";
  ser.clear_s9();
  for(int i=0;i<count;++i) {
    std::cout << " " << v[i];
    if (!ser.write_s9(v[i])) {
      std::cerr << "bad s9 write\n";
      die("bad s9 write");
    }
  }
  ser.flush_s9();
  if (!ser.seek(pos)) {
      std::cerr << "bad s9 seek\n";
      die("bad s9 seek");
  }
  std::cout << "\nreading s9:";
  for(int i=0;i<count;++i) {
    uint32_t t;
    int r = ser.read_s9(t);
    if (r != 0) {
      std::cerr << "bad s9 read, flag: " << r << "\n";
      die("bad s9 read");
    }
    std::cout << " " << t;
    if (t != v[i]) {
      std::cerr << "incorrect s9 read\n";
    }
  }
  ser.clear_s9();
  std::cout << " ... success\n";
}

int main(int argc, char *argv[]) {
  serializer ser(-1, serializer::READWRITE, 1024, -1, 0, true);
  std::cout << "writing uint64: ";
  test_write64(ser,12);
  test_write64(ser,1001);
  test_write64(ser,(uint64_t)10000000001);
  std::cout << "\n";

  if (!ser.seek(0)) {
    std::cerr << "bad seek\n";
    exit(-1);
  }

  std::cout << "reading uint64: ";
  test_read64(ser);
  test_read64(ser);
  test_read64(ser);
  std::cout << "\n";

  uint64_t pos = ser.pos();
  //if (!ser.seek(0)) {
  //  std::cerr << "bad seek\n";
  //  exit(-1);
  //}



  std::cout << "writing s9: ";
  test_writes9(ser,12);
  test_writes9(ser,13);
  test_writes9(ser,14);
  ser.flush_s9();
  ser.clear_s9();
  std::cout << "\n";

  if (!ser.seek(pos)) {
    std::cerr << "bad seek\n";
    exit(-1);
  }
  std::cout << "reading s9: ";
  test_reads9(ser);
  test_reads9(ser);
  test_reads9(ser);
  //test_read32(ser);
  std::cout << "\n";


  uint32_t vals[39] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  };
  test_s9seq(ser,vals,39);

  pos = ser.pos();

  std::cout << "writing vbyte64: ";
  test_writev64(ser,13);
  test_writev64(ser,26);
  test_writev64(ser,300);
  std::cout << "\n";
  if (!ser.seek(pos)) {
    std::cerr << "bad seek\n";
    exit(-1);
  }

  std::cout << "reading vbyte64: ";
  test_readv64(ser);
  test_readv64(ser);
  test_readv64(ser);
  std::cout << "\n";

}
