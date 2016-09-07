#ifndef _CORPUS_SCANNER_H_
#define _CORPUS_SCANNER_H_
#include "DocumentScanner.h"

class CorpusScanner
{
public:
    virtual DocumentScanner *getScanner() = 0;
    virtual void freeScanner(DocumentScanner *scanner) = 0;
};

#endif
