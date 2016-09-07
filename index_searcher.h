#ifndef _INDEX_SEARCHER_H_
#define _INDEX_SEARCHER_H_
#include <inttypes.h>
#include <map>
#include <vector>
#include <string>
#include "serializer.h"
class index_searcher {
  public:
    typedef uint32_t offset_t;
    typedef uint64_t termid_t;
    typedef uint64_t docid_t;
    index_searcher(std::string fname,std::string corpus_path = std::string(""));
    ~index_searcher();
    bool lookup_term(termid_t termid, std::vector<std::pair<docid_t,std::vector<offset_t> > > &posting);
    bool is_open();
    bool reset_scan();
    bool next_posting(std::string &term, termid_t &termid, std::vector<std::pair<docid_t,std::vector<offset_t> > > &posting);
    uint64_t get_ntitle_terms();
    uint64_t get_ntitle_words();
    uint64_t get_nbody_terms();
    uint64_t get_nbody_words();
    uint64_t get_npostings();
    uint64_t get_nterms();
    uint64_t get_nwords();
    uint64_t get_ndocs();

    std::vector<docid_t> get_docids();
    uint64_t get_doc_fpos(docid_t docid);
    uint32_t get_doc_title_length(docid_t docid);
    uint32_t get_doc_body_length(docid_t docid);
    uint32_t get_doc_title_terms(docid_t docid);
    uint32_t get_doc_body_terms(docid_t docid);
    std::pair<uint32_t,uint32_t> get_doc_length(docid_t docid);
    std::pair<uint32_t,uint32_t> get_doc_terms(docid_t docid);

    bool get_raw_document(docid_t docid, std::string &doc);
    //uint64_t compute_term_id(std::string term) {
    //  const char *buf = term.c_str();
    //  const size_t buflen = term.size();

    //  return serializer::get_SHA1(buf,buflen);
    //}
    std::map<docid_t, std::tuple<uint64_t, uint32_t,uint32_t,uint32_t,uint32_t> > docs;
  protected:
    std::map<termid_t, uint64_t> term_offsets;
    std::map<std::string, uint64_t> stats;
    serializer *dict_ser;
    serializer *postings_ser;
    serializer *corpus_ser;
    int64_t scan_index;
    termid_t prev_tid;
};
#endif
