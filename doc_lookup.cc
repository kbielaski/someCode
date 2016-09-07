#include "stemmer.h"
#include "index_searcher.h"
#include <iostream>
#include <set>
#include <cmath>
#include <inttypes.h>
#include <stdlib.h>

typedef uint64_t docid_t;
typedef uint64_t termid_t;

void usage(const char *prog) {
  std::cout 
    << "Usage: " << prog << " [ options ] <docnum> <docnum> ...\n"
    << "  --index <base path>  path to the index files (without the extension)\n"
    << "  --corpus <path>  path to file containing original corpus\n"
    << "  <docnum>  numeric id of the document (e.g. line number in the corpus)\n";
}

int main(int argc, char *argv[]) {
  const char *index_path = "";
  const char *corpus_path = "";
  bool quiet = true;
  bool has_corpus = false;

  int argi = 1;
  while (argi < argc) {
    const char *arg = argv[argi++];
    if (!strcmp(arg,"--help")) {
      usage(argv[0]);
      return 0;
    } else if (!strcmp(arg,"--index")) {
      if (argi >= argc) { std::cerr << "missing option to --index\n"; return 1; }
      index_path = argv[argi++];
    } else if (!strcmp(arg,"--corpus")) {
      if (argi >= argc) { std::cerr << "missing option to --corpus\n"; return 1; }
      corpus_path = argv[argi++];
      has_corpus = true;
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

  //if (index_path[0] == '\0' && argi < argc) index_path = argv[argi++];
  
  if (index_path[0] == '\0') {
    //std::cerr << "must specify index path (with any extension, just the base filename of the index)\n";
    //return 1;
    index_path = "index";
  }

  std::string fname(index_path);
  std::string fname_corpus(corpus_path);
  index_searcher searcher(fname,fname_corpus);
  if (!searcher.is_open()) {
    std::cerr << "failed to open index '" << index_path << "'\n";
  }

  while (argi < argc) {
    char *endptr;
    uint64_t docid = strtol(argv[argi++],&endptr,10);
    if (!(*argv[argi-1] != '\0' && *endptr == '\0')) {
      std::cerr << "bad docid \"" << argv[argi-1] << "\". must be numeric\n";
      return 1;
    }


    std::cout << "docid( " << docid << " ): fpos( " << searcher.get_doc_fpos(docid)
      << " ), title_words( " << searcher.get_doc_title_length(docid)
      << " ), body_words( " << searcher.get_doc_body_length(docid)
      << " ), title_terms( " << searcher.get_doc_title_terms(docid)
      << " ), body_terms( " << searcher.get_doc_body_terms(docid)
      << " )\n";
    if (has_corpus) {
      std::string doc;
      if (searcher.get_raw_document(docid,doc)) {
        std::cout << doc << "\n";
      } else {
        std::cerr << "couldn't read doc from corpus\n";
      }
    }
  }
}

