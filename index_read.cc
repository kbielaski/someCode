#include "serializer.h"
#include "DocumentScanner.h" //just for termid_t/docid_t
#include <map>
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int full_read(std::string fname, std::map<termid_t, std::vector<std::pair<docid_t,std::vector<uint32_t> > > > &postings, std::map<termid_t, std::string> &dictionary);

int main(int argc, char *argv[]) {
  std::map<termid_t, std::vector<std::pair<docid_t,std::vector<uint32_t> > > > postings;
  std::map<termid_t, std::string> dictionary;
  if (full_read(std::string(argv[1]),postings,dictionary)) { std::cerr << "sad\n"; return -1; }
}

int full_read(std::string fname, std::map<termid_t, std::vector<std::pair<docid_t,std::vector<uint32_t> > > > &postings, std::map<termid_t, std::string> &dictionary) {
  serializer dict_ser(fname + ".dct", serializer::READ);
  serializer terms_ser(fname + ".trm", serializer::READ);
  serializer postings_ser(fname + ".idx", serializer::READ);

  termid_t prev_tid = 0;
  termid_t tid;
  int nterms = 0;

  while(terms_ser.read_uint64(tid)) { //read termid
    nterms++;
    termid_t term_offset;
    if (!terms_ser.read_uint64(term_offset)) { std::cerr << "fail 0\n"; return -1; } //read termid
    termid_t tempid;
    if (!dict_ser.read_vbyte64(tempid)) { std::cerr << "fail 1\n"; return -1; } //read termid
    tempid += prev_tid;
    prev_tid = tempid+1;
    if (tempid != tid) {
      std::cerr << "termid mismatch " << std::hex << tid << std::dec << " vs " << std::hex << tempid << std::dec << "\n";
      return -1;
    }

    char tbuffer[1024];
    if (0 > dict_ser.read_string(tbuffer,1024)) { std::cerr << "fail 2\n"; return -1; } //read ndocs
    std::string term(tbuffer);
    std::cout << term;
    dictionary[tid] = term;

    //read: 0) #docs 1) doc ids, 2) posting lengths, 3) posting positions
    uint32_t ndocs;
    if (!postings_ser.read_vbyte32(ndocs)) { std::cerr << "fail 3\n"; return -1; }
    std::cout << " ndocs:" << ndocs;

    std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting;
    posting.reserve(ndocs);

    docid_t prev_docid = 0;
    std::cout << " docs:";
    for(uint32_t i=0;i<ndocs;++i) {
      docid_t docid;
      if (!postings_ser.read_vbyte64(docid)) { std::cerr << "fail 4\n"; return -1; } //read docid
      docid += prev_docid;
      prev_docid = docid+1;
      posting.push_back(std::pair<docid_t,std::vector<uint32_t> >(docid,std::vector<uint32_t>()));
      std::cout << docid << ",";
    }

    std::cout << " plens:";
    postings_ser.clear_s9(); //just in case
    for(uint32_t i=0;i<ndocs;++i) {
      uint32_t posting_length;
      int sr = postings_ser.read_s9(posting_length);
      if (sr) { std::cerr << "fail 5. flag " << sr << "\n"; return -1; } //read posting pos
      posting_length += 1;
      posting[i].second.resize(posting_length); //allocate and initialize all the entries
      std::cout << posting_length << ",";
    }

    for(uint32_t i=0;i<ndocs;++i) {
      uint32_t posting_length = posting[i].second.size();
      std::cout << " p:";
      uint32_t prev_pos = 0;
      for(uint32_t j = 0; j < posting_length; ++j) {
        uint32_t pos;
        int sr = postings_ser.read_s9(pos);
        if (sr) { std::cerr << "fail 6. flag " << sr << "\n"; return -1; } //read posting pos
        pos += prev_pos;
        prev_pos = pos + 1;
        posting[i].second[j] = pos;
        std::cout << posting[i].second[j] << ",";
      }
    }
    //if (fb_postings->s9_i != 28) { std::cerr << "fail 7. leftovers " << fb_postings->s9_i << "\n"; return -1; }

    postings[tid] = posting;
    std::cout << "\n";
  }
  std::cout << "done after " << nterms << " terms\n";

  return 0;
}
