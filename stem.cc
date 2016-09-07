#include "stemmer.h"
#include <iostream>

int main(int argc, char *argv[]) {
  std::string strline;
  stemmer *z = create_stemmer();
  size_t linelen = 1024;
  char *line = new char[linelen];
  while (getline(std::cin, strline)) {
    if ( strline.size() >= linelen ) {
      delete[] line;
      while (strline.size() >= linelen) {
        linelen *= 1.5;
      }
      line = new char[linelen];
    }
    strcpy(line,strline.c_str());

    for(char *p = line;*p;++p) {
      char c = *p;
      if (!isalnum(c)) *p = ' '; //TODO: decide whether to include '-'
    }

    stem_str(z,line); //also does to-lower, but leaves symbols in-place
    std::cout << line << "\n";
  }
  free_stemmer(z);
}
