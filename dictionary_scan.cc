#include "index_searcher.h"
#include "DocumentScanner.h" //just for termid_t/docid_t
#include <map>
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

int main(int argc, char *argv[]) {
  std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting;
  std::string term;
  termid_t termid;
  termid_t prev_tid = 0;
  serializer dict_ser(std::string(argv[1]) + ".dct", serializer::READ);

  while (dict_ser.read_vbyte64(termid)) {
    char tbuffer[1024];
    if (0 > dict_ser.read_string(tbuffer,1024)) { std::cerr << "fail to read dict term\n"; return 1; } //read ndocs
    termid += prev_tid;
    prev_tid = termid+1;
    term = std::string(tbuffer);
    std::cout << term << "\n";
  }
  return 0;
}
