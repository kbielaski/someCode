//index_gen - takes in trec formatted xml (actually sgml), and outputs an index for each document
//output format: <feature> <docno> <value>
//there is a feature for each word with the frequency, and also for each pair of words with a dist <= 10 there is a minimum distance
#include "binary_index_gen.h"
#include "serializer.h"
#include <set>

//#define DOPAIRS 1

static std::map<termid_t, std::vector<index_pair> > postings;
static std::map<termid_t, std::string> dictionary;
static std::set<std::string> bigrams;

bool verbose = true;
bool do_docs = false;

int generate_index(int input_fd, std::string fname, bool debug) {
  LineCorpusScanner *corpus = new LineCorpusScanner(input_fd);
  DocumentScanner *scanner = corpus->getScanner();
  uint64_t fpos;
  docid_t docid;
  std::string doc_name;
  std::vector<std::string> title_terms;
  std::vector<std::string> body_terms;
  std::set<termid_t> doc_title_terms,doc_body_terms;
  std::map<std::string,uint64_t> stats;
#ifdef DOPAIRS
  std::string wordbuffer[MAX_DIST];
#endif

  uint64_t ntitle_words = 0;
  uint64_t nbody_words = 0;
  uint64_t ntitle_terms = 0;
  uint64_t nbody_terms = 0;
  uint64_t ndocs = 0;

  serializer * docs_ser;
  if (do_docs) {
    docs_ser = new serializer(fname + ".docs", serializer::WRITE);
  } else {
    docs_ser = NULL;
  }

  while (scanner->nextDocument(fpos,docid,doc_name,title_terms,body_terms)) {
    doc_title_terms.clear();
    doc_body_terms.clear();
    ++ndocs;
    uint32_t pos = 0;
    for(std::vector<std::string>::const_iterator it = title_terms.begin(); it != title_terms.end(); ++it) {
      ++ntitle_words;
      std::string term = make_title_term(*it);
      termid_t tid = compute_term_id(term);
      postings[tid].push_back(index_pair(docid,pos));

      doc_title_terms.insert(tid);
      std::pair<std::map<termid_t, std::string>::iterator,bool> i = dictionary.insert(std::pair<termid_t,std::string>(tid,term));
      if (!i.second) { //element already there
        if (i.first->second != term) {
          std::cerr << "hash collision between '" << term << "' and '" << i.first->second << "'\n";
        }
      } else {
        ++ntitle_terms;
      }

#ifdef DOPAIRS
      wordbuffer[pos % MAX_DIST] = *it;
      for(uint32_t pairdist = MAX_DIST-1; pairdist > 0; pairdist--) {
        if (pos > pairdist) {
          std::string pair = make_title_pair(wordbuffer[(pos+MAX_DIST-pairdist)%MAX_DIST],wordbuffer[pos%MAX_DIST]);
          bigrams.insert(pair);
        }
      }
#endif
      pos++;
    }
    pos = 0;
    for(std::vector<std::string>::const_iterator it = body_terms.begin(); it != body_terms.end(); ++it) {
      ++nbody_words;
      std::string term = *it;
      termid_t tid = compute_term_id(term);
      postings[tid].push_back(index_pair(docid,pos));

      doc_body_terms.insert(tid);
      std::pair<std::map<termid_t, std::string>::iterator,bool> i = dictionary.insert(std::pair<termid_t,std::string>(tid,term));
      if (!i.second) { //element already there
        if (i.first->second != term) {
          std::cerr << "hash collision between '" << term << "' and '" << i.first->second << "'\n";
        }
      } else {
        ++nbody_terms;
      }
      
#ifdef DOPAIRS
      wordbuffer[pos % MAX_DIST] = term;
      for(uint32_t pairdist = MAX_DIST-1; pairdist > 0; pairdist--) {
        if (pos > pairdist) {
          std::string pair = make_pair(wordbuffer[(pos+MAX_DIST-pairdist)%MAX_DIST],wordbuffer[pos%MAX_DIST]);
          bigrams.insert(pair);
        }
      }
#endif
      pos++;
    }
    if (docs_ser) {
      docs_ser->write_uint64(docid);
      docs_ser->write_uint64(fpos);
      docs_ser->write_uint32(title_terms.size());
      docs_ser->write_uint32(body_terms.size());
      docs_ser->write_uint32(doc_title_terms.size());
      docs_ser->write_uint32(doc_body_terms.size());
    }
  }
  stats["ntitle_terms"] = ntitle_terms;
  stats["ntitle_words"] = ntitle_words;
  stats["nbody_terms"] = nbody_terms;
  stats["nbody_words"] = nbody_words;
  stats["nterms"] = ntitle_terms + nbody_terms;
  stats["nwords"] = ntitle_words + nbody_words;
  stats["ndocs"] = ndocs;

  std::cerr << "\nfinished scan, dumping index...\n";
  //std::map<termid_t, std::vector<index_pair> >(postings).swap(postings); //resize posting capacity to actual size
  if (docs_ser) {
    docs_ser->flush();
    delete docs_ser;
  }
  return dump_index(fname,postings,dictionary,bigrams,stats);
}

int dump_index(std::string fname, std::map<termid_t, std::vector<index_pair> > &postings, std::map<termid_t, std::string> &dictionary, std::set<std::string> &bigrams,std::map<std::string,uint64_t> stats) {
  serializer * dict_ser = new serializer(fname + ".dct", serializer::WRITE);
  serializer * terms_ser = new serializer(fname + ".trm", serializer::WRITE);
  serializer * postings_ser = new serializer(fname + ".idx", serializer::WRITE);
  serializer * stats_ser = new serializer(fname + ".stats", serializer::WRITE);
#ifdef DOPAIRS
  serializer bigrams_ser = new serializer(fname + ".bgm", serializer::WRITE);
#endif

#ifdef DOPAIRS
  for(std::set<std::string>::const_iterator it = bigrams.begin(); it != bigrams.end(); ++it) {
    std::string b = *it + "\n";
    bigrams_ser.write_bytes(b.c_str(), b.size());
  }
#endif
  

  termid_t prev_tid = 0;

  std::map<docid_t, std::vector<uint32_t> > posting;
  for(std::map<termid_t, std::vector<index_pair> >::const_iterator it = postings.begin(); it != postings.end(); ++it) {
  //while(!postings.empty()) {
    //std::map<termid_t, std::vector<index_pair> >::iterator it = postings.begin();
    termid_t tid = it->first;
    std::string term = dictionary[tid];
    if (verbose) std::cout << term;
    //std::cout << " " << std::hex << tid << std::dec;

    posting.clear();
    //construct posting sorted by document
    for(std::vector<index_pair>::const_iterator pit = it->second.begin(); pit != it->second.end(); ++pit) {
      docid_t docid = pit->first;
      uint32_t pos = pit->second;
      posting[docid].push_back(pos);
    }
    stats["npostings"] += posting.size();
    //postings.erase(it); //free up memory

    //std::map<docid_t, std::vector<uint32_t> >(posting).swap(posting); //resize posting capacity to actual size
    //for(std::map<docid_t, std::vector<uint32_t> >::iterator it = posting.begin(); it != posting.end(); ++it) {
    //  std::vector<uint32_t>(it->second).swap(it->second); //resize each doclist to actual size
    //}


    //write to dictionary
    dict_ser->write_vbyte64(tid-prev_tid); //write delta termid
    dict_ser->write_bytes(term.c_str(),strlen(term.c_str())+1); //write term string with null terminator

    //write to term index
    terms_ser->write_uint64(tid); //write termid
    uint64_t pos = postings_ser->pos();
    terms_ser->write_uint64(pos); //write term position in postings file

    //now we write: 0) #docs 1) doc ids, 2) posting lengths, 3) posting positions
    postings_ser->write_vbyte32(posting.size()); //write #docs in posting

    docid_t prev_docid = 0;
    for(std::map<docid_t, std::vector<uint32_t> >::iterator it = posting.begin(); it != posting.end(); ++it) {
      docid_t docid = it->first;
      postings_ser->write_vbyte64(docid - prev_docid); //write delta docid
      prev_docid = docid + 1; //because docids are always 1+ apart
    }
    for(std::map<docid_t, std::vector<uint32_t> >::iterator it = posting.begin(); it != posting.end(); ++it) {
      uint32_t posting_length = it->second.size();
      postings_ser->write_s9(posting_length-1); //write posting length-1, because it can't be zero
    }

    //now iterate through posting by document
    if (verbose) {
      std::cout << " ndocs:" << posting.size();

      std::cout << " docs:";
      for(std::map<docid_t, std::vector<uint32_t> >::const_iterator it = posting.begin(); it != posting.end(); ++it) {
        std::cout << it->first << ",";
      }
      std::cout << " plens:";
      for(std::map<docid_t, std::vector<uint32_t> >::const_iterator it = posting.begin(); it != posting.end(); ++it) {
        std::cout << it->second.size() << ",";
      }
    }
    for(std::map<docid_t, std::vector<uint32_t> >::iterator it = posting.begin(); it != posting.end(); ++it) {
      std::sort(it->second.begin(), it->second.end()); //sort posting by pos
      //docid_t docid = it->first;
      if (verbose) {
        //std::cout << " " << docid << ":";
        std::cout << " p:";
      }
      uint32_t prev_pos = 0;
      for(std::vector<uint32_t>::const_iterator pit = it->second.begin(); pit != it->second.end(); ++pit) {
        if (verbose) {
          std::cout << *pit << ",";
        }
        postings_ser->write_s9(*pit-prev_pos); //write posting postion
        prev_pos = *pit + 1; //because positions are always 1+ apart
      }
    }
    postings_ser->flush_s9();
    if (verbose) std::cout << "\n";
    prev_tid = tid + 1; //because tids are always 1+ apart
  }

  for(std::map<std::string,uint64_t>::const_iterator it = stats.begin(); it != stats.end(); ++it) {
    stats_ser->write_bytes(it->first.c_str(),strlen(it->first.c_str())+1); //write key string with null terminator
    stats_ser->write_vbyte64(it->second); //write delta termid
  }

  dict_ser->flush();
  terms_ser->flush();
  postings_ser->flush();
  stats_ser->flush();
#ifdef DOPAIRS
  bigrams_ser.flush();
  delete bigrams_ser;
#endif
  delete dict_ser;
  delete terms_ser;
  delete postings_ser;
  delete stats_ser;
  return 0;
}

int main(int argc, char *argv[]) { 
    int input_fd;
  const char *index_path = "\0";

    int argi = 1;
    while(argi < argc) {
      const char *arg = argv[argi];
      argi++;
      if (0 == strcmp(arg,"--help")) {
      } else if (!strcmp(arg,"--index")) {
        if (argi >= argc) { std::cerr << "missing option to --index\n"; return 1; }
        index_path = argv[argi++];
      } else if (0 == strcmp(arg,"--verbose") || 0 == strcmp(arg,"-v")) {
        verbose = true;
      } else if (0 == strcmp(arg,"--docs")) {
        do_docs = true;
      } else if (0 == strcmp(arg,"--quiet") || 0 == strcmp(arg,"-q")) {
        verbose = false;
      } else if (arg[0] == '-') {
        std::cerr << "unknown option \"" << arg << "\"\n";
        return -1;
      } else {
        argi--;
        break;
      }
    }

    if (argi < argc) {
      int fd = open(argv[argi++], O_RDONLY);
      if (fd < 0) {
        std::cerr << "failed to open corpus" << std::endl;
        exit(-1);
      }
      input_fd = fd;
    } else {
      input_fd = STDIN_FILENO;
    }

    if (index_path[0] == '\0' && argi < argc) index_path = argv[argi++];

    if (index_path[0] == '\0') {
      index_path = "index";
    }

    std::string fname(index_path);

    if (argi < argc) {
      std::cerr << "unknown extra options\n";
      return -1;
    }

    return generate_index(input_fd, fname, true);
}
