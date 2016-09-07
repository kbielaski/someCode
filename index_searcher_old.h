#include <inttypes.h>
#include <map>
#include <vector>
#include <string>
#include "coding.h"
class index_searcher {
  public:
    typedef uint32_t offset_t;
    typedef uint64_t termid_t;
    typedef uint64_t docid_t;
    index_searcher(std::string fname);
    ~index_searcher();
    bool lookup_term(termid_t termid, std::vector<std::pair<docid_t,std::vector<offset_t> > > &posting);
    bool is_open();
  protected:
    std::map<termid_t, uint64_t> term_offsets;
    int fd_postings;
    struct file_buffer *fb_postings;
};
