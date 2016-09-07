#ifndef INDEX_GEN_H
#define INDEX_GEN_H

#include<iostream>
#include <map>
#include <set>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "LineScanner.h"
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/obj_mac.h>

//MAX_DIST is an exclusive upper bound (we use circular buffers with modulo)
#define MAX_DIST 11
#define USE_TSET 1
#define USE_XSET 1

#define XSET_WORKER_QUEUE_SIZE 1000

typedef std::pair<docid_t,uint32_t> index_pair;

bool illegalChar(const char &c) {
  return !(std::isspace(c) || std::isalnum(c));
  //return !(c == ' ' || c == '\n' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

std::string make_title_term(const std::string &t) {
  return std::string("TTL|" + t);
}

std::string make_pair(std::string f1, std::string f2) {
  if (f1 < f2) {
    return f1 + "|" + f2;
  } else {
    return f2 + "|" + f1;
  }
}

std::string make_title_pair(std::string f1, std::string f2) {
  if (f1 < f2) {
    return "TTL|" + f1 + "|" + f2;
  } else {
    return "TTL|" + f2 + "|" + f1;
  }
}

//class CstrCmp {
//    public:
//        bool operator()(const char* a, const char* b)
//        {
//            return strcmp(a, b) < 0;
//        }
//};

void usage(const char *path) {
  std::cerr << "Usage: cat <files_to_index> | " << path << " <options>\n"
    << "Takes lines on stdin, and writes index files\n";
}

docid_t compute_term_id(std::string term) {
  const char *buf = term.c_str();
  const size_t buflen = term.size();

  return (docid_t)get_SHA1(buf,buflen);
}

int generate_index(int input_fd, bool debug);
int dump_index(std::string fname, std::map<termid_t, std::vector<index_pair> > &postings, std::map<termid_t, std::string> &dictionary, std::set<std::string> &bigrams);
#endif
