#include "index_searcher.h"
#include <iostream>
#include <string>

uint64_t compute_term_id(std::string term) {
  const char *buf = term.c_str();
  const size_t buflen = term.size();

  return get_SHA1(buf,buflen);
}

int main(int argc, char *argv[]) {
  std::string fname(argv[1]);
  index_searcher searcher(fname);
  if (!searcher.is_open()) {
    std::cerr << "failed to open index '" << argv[1] << "'\n";
  }
  std::vector<std::pair<uint64_t,std::vector<uint32_t> > > posting;
  for(int i=2;i<argc;++i) {
    std::cout << argv[i];
    if (!searcher.lookup_term(compute_term_id(argv[i]),posting)) {
      std::cout << " ndocs:0 docs: plens:\n";
      continue;
    }
    int ndocs=posting.size();
    std::cout << " ndocs:" << ndocs;
    std::cout << " docs:";
    for(int i=0;i<ndocs;++i) {
      std::cout << posting[i].first << ",";
    }

    std::cout << " plens:";
    for(int i=0;i<ndocs;++i) {
      std::cout << posting[i].second.size() << ",";
    }

    for(int i=0;i<ndocs;++i) {
      int posting_length = posting[i].second.size();
      std::cout << " p:";
      for(int j = 0; j < posting_length; ++j) {
        std::cout << posting[i].second[j] << ",";
      }
    }
    std::cout << "\n";
  }
  return 0;
}
