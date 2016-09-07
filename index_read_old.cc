#include "coding.h"
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
  struct file_buffer *fb_dict = (struct file_buffer*)malloc(sizeof(struct file_buffer));
  struct file_buffer *fb_terms = (struct file_buffer*)malloc(sizeof(struct file_buffer));
  struct file_buffer *fb_postings = (struct file_buffer*)malloc(sizeof(struct file_buffer));

  std::string dict_path = fname + ".dct";
  std::string terms_path = fname + ".trm";
  std::string postings_path = fname + ".idx";

  int fd_dict = open(dict_path.c_str(), O_RDONLY, 0);
  int fd_terms = open(terms_path.c_str(), O_RDONLY, 0);
  int fd_postings = open(postings_path.c_str(), O_RDONLY, 0);
  if (fd_dict == -1 || fd_terms == -1 || fd_postings == -1) {
    std::cerr << "error opening index files\n";
    return -1;
  }
  fbuffer_init(fb_dict, fd_dict, FBUFFER_READ);
  fbuffer_init(fb_terms, fd_terms, FBUFFER_READ);
  fbuffer_init(fb_postings, fd_postings, FBUFFER_READ);

  termid_t prev_tid = 0;
  termid_t tid;
  int nterms = 0;

  while(!fbuffer_read_uint64(fb_terms,&tid)) { //read termid
    nterms++;
    termid_t term_offset;
    if (fbuffer_read_uint64(fb_terms,&term_offset)) { std::cerr << "fail 0\n"; return -1; } //read termid
    termid_t tempid;
    if (fbuffer_read_vbyte64(fb_dict,&tempid)) { std::cerr << "fail 1\n"; return -1; } //read termid
    tempid += prev_tid;
    prev_tid = tid+1;
    if (tempid != tid) {
      std::cerr << "termid mismatch " << std::hex << tid << std::dec << " vs " << std::hex << tempid << std::dec << "\n";
      return -1;
    }

    char tbuffer[1024];
    if (fbuffer_read_string(fb_dict,tbuffer,1024) < 0) { std::cerr << "fail 2\n"; return -1; } //read ndocs
    std::string term(tbuffer);
    std::cout << term;
    //std::cout << " " << tid;
    dictionary[tid] = term;

    //read: 0) #docs 1) doc ids, 2) posting lengths, 3) posting positions
    uint32_t ndocs;
    if (fbuffer_read_vbyte(fb_postings,&ndocs)) { std::cerr << "fail 3\n"; return -1; }
    std::cout << " ndocs:" << ndocs;

    std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting;
    posting.reserve(ndocs);

    docid_t prev_docid = 0;
    std::cout << " docs:";
    for(uint32_t i=0;i<ndocs;++i) {
      docid_t docid;
      if (fbuffer_read_vbyte64(fb_postings,&docid)) { std::cerr << "fail 4\n"; return -1; } //read docid
      docid += prev_docid;
      prev_docid = docid+1;
      posting.push_back(std::pair<docid_t,std::vector<uint32_t> >(docid,std::vector<uint32_t>()));
      std::cout << docid << ",";
    }

    std::cout << " plens:";
    for(uint32_t i=0;i<ndocs;++i) {
      uint32_t posting_length;
      if (fbuffer_read_s9(fb_postings,&posting_length)) { std::cerr << "fail 5\n"; return -1; } //read posting length
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
        int sr = fbuffer_read_s9(fb_postings,&pos);
        if (sr) { std::cerr << "fail 6. flag " << sr << "\n"; return -1; } //read posting pos
        pos += prev_pos;
        prev_pos = pos + 1;
        posting[i].second[j] = pos;
        std::cout << posting[i].second[j] << ",";
      }
    }
    if (fb_postings->s9_i != 28) { std::cerr << "fail 7. leftovers " << fb_postings->s9_i << "\n"; return -1; }

    postings[tid] = posting;
    std::cout << "\n";
  }
  std::cout << "done after " << nterms << " terms\n";

  close(fd_dict);
  close(fd_terms);
  close(fd_postings);
  return 0;
}
