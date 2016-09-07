#include "serializer.h"
#include "DocumentScanner.h" //just for termid_t/docid_t
#include <map>
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

uint64_t compute_term_id(std::string term) {
  const char *buf = term.c_str();
  const size_t buflen = term.size();

  return serializer::get_SHA1(buf,buflen);
}


int full_read(std::string fname, std::map<termid_t, std::vector<std::pair<docid_t,std::vector<uint32_t> > > > &postings, std::map<termid_t, std::string> &dictionary);
int bigram_read(std::string fname, std::map<termid_t, std::vector<std::pair<docid_t,std::vector<uint32_t> > > > &postings, std::map<termid_t, std::string> &dictionary);
std::vector<std::pair<docid_t, uint32_t> > intersect_terms(std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting1,std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting2, uint32_t distcap);

int main(int argc, char *argv[]) {
  std::map<termid_t, std::vector<std::pair<docid_t,std::vector<uint32_t> > > > postings;
  std::map<termid_t, std::string> dictionary;
  if (full_read(std::string(argv[1]),postings,dictionary)) { std::cerr << "sad 1\n"; return -1; }
  std::cout << "finished reading index. now reading bigrams\n";
  if (bigram_read(std::string(argv[1]),postings,dictionary)) { std::cerr << "sad 2\n"; return -1; }
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
    //std::cout << " ndocs:" << ndocs;

    std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting;
    posting.reserve(ndocs);

    docid_t prev_docid = 0;
    //std::cout << " docs:";
    for(uint32_t i=0;i<ndocs;++i) {
      docid_t docid;
      if (!postings_ser.read_vbyte64(docid)) { std::cerr << "fail 4\n"; return -1; } //read docid
      docid += prev_docid;
      prev_docid = docid+1;
      posting.push_back(std::pair<docid_t,std::vector<uint32_t> >(docid,std::vector<uint32_t>()));
      //std::cout << docid << ",";
    }

    //std::cout << " plens:";
    postings_ser.clear_s9(); //just in case
    for(uint32_t i=0;i<ndocs;++i) {
      uint32_t posting_length;
      int sr = postings_ser.read_s9(posting_length);
      if (sr) { std::cerr << "fail 5. flag " << sr << "\n"; return -1; } //read posting pos
      posting_length += 1;
      posting[i].second.resize(posting_length); //allocate and initialize all the entries
      std::cout << " " << posting[i].first << ":" << posting_length;
      //std::cout << posting_length << ",";
    }

    for(uint32_t i=0;i<ndocs;++i) {
      uint32_t posting_length = posting[i].second.size();
      //std::cout << " p:";
      uint32_t prev_pos = 0;
      for(uint32_t j = 0; j < posting_length; ++j) {
        uint32_t pos;
        int sr = postings_ser.read_s9(pos);
        if (sr) { std::cerr << "fail 6. flag " << sr << "\n"; return -1; } //read posting pos
        pos += prev_pos;
        prev_pos = pos + 1;
        posting[i].second[j] = pos;
        //std::cout << posting[i].second[j] << ",";
      }
    }
    //if (fb_postings->s9_i != 28) { std::cerr << "fail 7. leftovers " << fb_postings->s9_i << "\n"; return -1; }

    postings[tid] = posting;
    std::cout << "\n";
  }
  std::cout << "done after " << nterms << " terms\n";

  return 0;
}

std::vector<std::pair<docid_t, uint32_t> > intersect_terms(std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting1,std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting2, uint32_t distcap) {
  std::vector<std::pair<docid_t,std::vector<uint32_t> > >::const_iterator it1 = posting1.begin();
  std::vector<std::pair<docid_t,std::vector<uint32_t> > >::const_iterator it2 = posting2.begin();
  std::vector<std::pair<docid_t, uint32_t> > ret;
  while(it1 != posting1.end() && it2 != posting2.end()) {
    if (it1->first < it2->first) {
      ++it1;
    } else if (it2->first < it1->first) {
      ++it2;
    } else {
      std::vector<uint32_t>::const_iterator pit1 = it1->second.begin();
      std::vector<uint32_t>::const_iterator pit2 = it2->second.begin();
      uint32_t dist = (uint32_t)-1;
      while (pit1 != it1->second.end() && pit2 != it2->second.end()) {
        if (*pit1 < *pit2) {
          if ((*pit2 - *pit1) < dist) dist = *pit2 - *pit1;
          ++pit1;
        } else {
          if ((*pit1 - *pit2) < dist) dist = *pit1 - *pit2;
          ++pit2;
        }
      }
      if (dist <= distcap) {
        ret.push_back(std::pair<docid_t,uint32_t>(it1->first, dist));
      }
      ++it1;
      ++it2;
    }
  }
  return ret;
}

int bigram_read(std::string fname, std::map<termid_t, std::vector<std::pair<docid_t,std::vector<uint32_t> > > > &postings, std::map<termid_t, std::string> &dictionary) {
  serializer bigrams_ser(fname + ".bgm", serializer::READ);
  char tbuffer[1024];
  while (bigrams_ser.read_tok(tbuffer, '|', 1024)) {
    bool title = false;
    if (0 == strcmp(tbuffer,"TTL")) {
      title = true;
      if (!bigrams_ser.read_tok(tbuffer, '|', 1024)) { std::cerr << "bad stuff\n"; return -1; }
    }
    std::string first(tbuffer);
    if (!bigrams_ser.read_tok(tbuffer, '\n', 1024)) { std::cerr << "bad stuff\n"; return -1; }
    std::string second(tbuffer);
    docid_t first_id;
    docid_t second_id;
    if (title) {
      first_id = compute_term_id("TTL|" + first);
      second_id = compute_term_id("TTL|" + second);
    } else {
      first_id = compute_term_id(first);
      second_id = compute_term_id(second);
    }

    std::vector<std::pair<docid_t, uint32_t> > min_dists = intersect_terms(postings[first_id],postings[second_id],10);
    std::cout << ( title ? "TTL|" : "" ) << first << "|" << second;

    for(std::vector<std::pair<docid_t, uint32_t> >::const_iterator it = min_dists.begin(); it != min_dists.end(); ++it) {
      std::cout << " " << it->first << ":" << it->second;
    }
    std::cout << "\n";
  }
  return 0;
}





