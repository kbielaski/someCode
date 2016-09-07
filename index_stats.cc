#include "stemmer.h"
#include "index_scanner.h"
#include <iostream>
#include <set>
#include <cmath>
#include <inttypes.h>

typedef uint64_t docid_t;
typedef uint64_t termid_t;

int main(int argc, char *argv[]) {

  const char *index_path = "\0";
  bool quiet = true;
  bool do_count_bigrams = false;

  int argi = 1;
  while (argi < argc) {
    const char *arg = argv[argi++];
    if (!strcmp(arg,"--index")) {
      if (argi >= argc) { std::cerr << "missing option to --index\n"; return 1; }
      index_path = argv[argi++];
    } else if (!strcmp(arg,"--bigrams")) {
      do_count_bigrams = true;
    } else if (!strcmp(arg,"--quiet") || !strcmp(arg,"-q")) {
      quiet = true;
    } else if (!strcmp(arg,"--verbose") || !strcmp(arg,"-v")) {
      quiet = false;
    } else if (!strcmp(arg,"--")) {
      break;
    } else if (arg[0] == '-') {
      std::cerr << "unknown option '" << arg << "'\n"; return 1;
    } else {
      argi--;
      break;
    }
  }

  if (index_path[0] == '\0' && argi < argc) index_path = argv[argi++];
  
  if (index_path[0] == '\0') {
    std::cerr << "must specify index path (with any extension, just the base filename of the index)\n";
    return 1;
  }

  if (argi < argc) {
    std::cerr << "unkown extra program arguments\n";
    return 1;
  }

  std::string fname(index_path);
  //index_searcher searcher(fname);
  //if (!searcher.is_open()) {
  //  std::cerr << "failed to open index '" << argv[1] << "'\n";
  //}
  //
  //serializer bigrams_ser(std::string(index_path) + ".bgm", serializer::READ);

  //std::cout << "total docs: " << searcher.get_ndocs() << "\n"
  //  << "total words: " << searcher.get_nwords() << "\n"
  //  << "total body words: " << searcher.get_nbody_words() << "\n"
  //  << "total title words: " << searcher.get_ntitle_words() << "\n"
  //  << "total unique terms: " << searcher.get_nterms() << "\n"
  //  << "total unique body terms: " << searcher.get_nbody_terms() << "\n"
  //  << "total unique title terms: " << searcher.get_ntitle_terms() << "\n";

  //
  //uint64_t nbodypostingentries = 0;
  //uint64_t ntitlepostingentries = 0;
  //for( std::map<docid_t, std::tuple<uint64_t, uint32_t,uint32_t,uint32_t,uint32_t> >::const_iterator it = searcher.docs.cbegin(); it != searcher.docs.cend(); ++it) {
  //  ntitlepostingentries += std::get<3>(it->second);
  //  nbodypostingentries += std::get<4>(it->second);
  //}

  //std::cout
  //  << "total body posting entries: " << nbodypostingentries << "\n"
  //  << "total title posting entries: " << ntitlepostingentries << "\n";

  //uint64_t bigram_count = 0;
  //int ret = 0;
  ////char *tbuffer = new char[1025];
  //char tbuffer[1024];

  //while ((ret = bigrams_ser.read_tok(tbuffer, '\n', 1024)) > 0) { bigram_count++; }
  //if (ret < 0) {
  //std::cout
  //  << "bigram read error\n";
  //}

  //std::cout
  //  << "total bigram entries: " << bigram_count << "\n";

  index_scanner scanner(fname,10,true);
  if (!scanner.index.is_open()) {
    std::cerr << "failed to open index '" << argv[1] << "'\n";
  }
  std::cout << "total docs: " << scanner.index.get_ndocs() << "\n"
    << "total words: " << scanner.index.get_nwords() << "\n"
    << "total body words: " << scanner.index.get_nbody_words() << "\n"
    << "total title words: " << scanner.index.get_ntitle_words() << "\n"
    << "total unique terms: " << scanner.index.get_nterms() << "\n"
    << "total unique body terms: " << scanner.index.get_nbody_terms() << "\n"
    << "total unique title terms: " << scanner.index.get_ntitle_terms() << "\n";

  uint64_t nbodypostingentries = 0;
  uint64_t ntitlepostingentries = 0;
  for( std::map<docid_t, std::tuple<uint64_t, uint32_t,uint32_t,uint32_t,uint32_t> >::const_iterator it = scanner.index.docs.cbegin(); it != scanner.index.docs.cend(); ++it) {
    ntitlepostingentries += std::get<3>(it->second);
    nbodypostingentries += std::get<4>(it->second);
  }

  std::cout
    << "total body posting entries: " << nbodypostingentries << "\n"
    << "total title posting entries: " << ntitlepostingentries << "\n";

  if (do_count_bigrams) {
    uint64_t nbody_bigrams;
    uint64_t ntitle_bigrams;
    uint64_t nbody_bigram_entries;
    uint64_t ntitle_bigram_entries;
    if (scanner.count_bigram_postings(true, nbody_bigrams, ntitle_bigrams, nbody_bigram_entries, ntitle_bigram_entries)) {
      std::cout
        << "total body bigrams: " << nbody_bigrams << "\n"
        << "total title bigrams: " << ntitle_bigrams << "\n"
        << "total body bigram posting entries: " << nbody_bigram_entries << "\n"
        << "total title bigram posting entries: " << ntitle_bigram_entries << "\n";
    } else {
      std::cerr << "failed to read bigrams\n";
    }
  }
}


