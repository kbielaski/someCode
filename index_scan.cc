#include "index_scanner.h"
#include "DocumentScanner.h" //just for termid_t/docid_t
#include <map>
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

int main(int argc, char *argv[]) {
  const char *index_path = "";
  bool quiet = true;
  bool cache = false;

  int argi = 1;
  while (argi < argc) {
    const char *arg = argv[argi++];
    if (!strcmp(arg,"--index")) {
      if (argi >= argc) { std::cerr << "missing option to --index\n"; return 1; }
      index_path = argv[argi++];
    } else if (!strcmp(arg,"--quiet") || !strcmp(arg,"-q")) {
      quiet = true;
    } else if (!strcmp(arg,"--verbose") || !strcmp(arg,"-v")) {
      quiet = false;
    } else if (!strcmp(arg,"--cache")) {
      cache = true;
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
    //index_path = "index";
  }

  std::vector<std::pair<docid_t,std::vector<double> > > posting;
  std::string term;
  bool primary_term;
  index_scanner scanner(index_path,10,cache);
  while(scanner.next_posting(term,primary_term,posting)) {
    std::cout << term;
    for(std::vector<std::pair<docid_t,std::vector<double> > >::const_iterator it = posting.begin(); it != posting.end(); ++it) {
      std::cout << " " << it->first << ":";
      std::vector<double>::const_iterator fit = it->second.begin();
      if (fit != it->second.end()) {
        std::cout << *fit;
        ++fit;
      }
      for (;fit != it->second.end();++fit) {
        std::cout << "," << *fit;
      }
    }
    std::cout << "\n";
  }
  std::cout << "fin.";
}
