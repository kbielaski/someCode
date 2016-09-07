#include "index_searcher.h"
#include "helpers.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
index_searcher::index_searcher(std::string fname) {
  fb_postings = (struct file_buffer*)malloc(sizeof(struct file_buffer));
  struct file_buffer *fb_terms = (struct file_buffer*)malloc(sizeof(struct file_buffer));

  std::string terms_path = fname + ".trm";
  std::string postings_path = fname + ".idx";

  int fd_terms = open(terms_path.c_str(), O_RDONLY, 0);
  fd_postings = open(postings_path.c_str(), O_RDONLY, 0);

  if (fd_terms == -1 || fd_postings == -1) {
    fd_postings = -1;
    return;
  }
  fbuffer_init(fb_terms, fd_terms, FBUFFER_READ);
  fbuffer_init(fb_postings, fd_postings, FBUFFER_READ);

  int nterms = 0;
  termid_t tid;
  while(!fbuffer_read_uint64(fb_terms,&tid)) { //read termid
    nterms++;

    termid_t term_offset;
    if (fbuffer_read_uint64(fb_terms,&term_offset)) die("couln't read from index");
    term_offsets[tid] = term_offset;
  }

  free(fb_terms);
  close(fd_terms);
}

index_searcher::~index_searcher() {
  free(fb_postings);
  close(fd_postings);
  fd_postings = -1;
}

bool index_searcher::is_open() {
  return fd_postings != -1;
}

bool index_searcher::lookup_term(termid_t termid, std::vector<std::pair<docid_t,std::vector<offset_t> > > &posting) {
  if (!is_open()) {
    die("index not loaded");
  }
  std::map<termid_t, uint64_t>::const_iterator it = term_offsets.find(termid);
  if (it == term_offsets.end()) return false;

  fbuffer_seek_file_offset(fb_postings,it->second);

  uint32_t ndocs;
  if (fbuffer_read_vbyte(fb_postings,&ndocs)) { fprintf(stderr,"fail 3\n"); return false; }

  posting.clear();
  posting.reserve(ndocs);

  docid_t prev_docid = 0;
  for(uint32_t i=0;i<ndocs;++i) {
    docid_t docid;
    if (fbuffer_read_vbyte64(fb_postings,&docid)) { fprintf(stderr,"fail 4\n"); return false; } //read docid
    docid += prev_docid;
    prev_docid = docid+1;
    posting.push_back(std::pair<docid_t,std::vector<uint32_t> >(docid,std::vector<uint32_t>()));
  }

  for(uint32_t i=0;i<ndocs;++i) {
    uint32_t posting_length;
    if (fbuffer_read_s9(fb_postings,&posting_length)) { fprintf(stderr,"fail 5\n"); return false; } //read posting length
    posting_length += 1;
    posting[i].second.resize(posting_length); //allocate and initialize all the entries
  }

  for(uint32_t i=0;i<ndocs;++i) {
    uint32_t posting_length = posting[i].second.size();
    uint32_t prev_pos = 0;
    for(uint32_t j = 0; j < posting_length; ++j) {
      uint32_t pos;
      int sr = fbuffer_read_s9(fb_postings,&pos);
      if (sr) { fprintf(stderr,"fail 6. flag %d\n",sr); return false; } //read posting pos
      pos += prev_pos;
      prev_pos = pos + 1;
      posting[i].second[j] = pos;
    }
  }
  //if (fb_postings->s9_i != 28) { fprintf(stderr,"fail 7. leftovers %d\n",fb_postings->s9_i); return false; }

  return true;
}
