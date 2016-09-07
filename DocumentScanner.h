#ifndef _DOCUMENT_SCANNER_H_
#define _DOCUMENT_SCANNER_H_
#include <string>
#include <vector>
#include <stdint.h>

typedef uint64_t docid_t;
typedef uint64_t termid_t;

class DocumentScanner
{
public:
    //typedef enum { TERM_TITLE, TERM_BODY } term_type;
    //virtual bool nextDocument(docid_t &docid) = 0;
    //virtual bool nextTerm(std::string &term, term_type &type) = 0;
    virtual bool nextDocument(uint64_t &fpos, docid_t &docid, std::string doc_name, std::vector<std::string> &title_terms, std::vector<std::string> &body_terms) = 0;
    virtual ~DocumentScanner() {};
};

#endif
