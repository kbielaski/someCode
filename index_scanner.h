#ifndef _INDEX_SCANNER_H_
#define _INDEX_SCANNER_H_
#include "serializer.h"
#include "DocumentScanner.h" //just for termid_t/docid_t
#include "index_searcher.h"
#include <map>
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

class index_scanner {
  public:
    index_scanner(std::string prefix, int max_bigram_dist, bool cache_postings = false);
    bool next_posting(std::string &term, bool &primary_term, std::vector<std::pair<docid_t, std::vector<double> > > &posting);
    bool count_bigram_postings(bool progress, uint64_t &body_bigram_count, uint64_t &title_bigram_count, uint64_t &body_bigram_entry_count, uint64_t &title_bigram_entry_count);
    index_searcher index;
  protected:
    serializer bigrams_ser;
    bool finished_terms;
    bool finished_bigrams;
    bool cache_postings;
    int max_bigram_dist;
    std::map<termid_t,std::vector<std::pair<docid_t,std::vector<uint32_t> > > > posting_cache;
    bool do_lookup(termid_t termid,std::vector<std::pair<docid_t,std::vector<uint32_t> > > &posting);
    std::vector<std::pair<docid_t, uint32_t> > intersect_terms(std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting1,std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting2, uint32_t distcap);
};
#endif
