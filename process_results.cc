#include <cstring>
#include <cstdlib>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <fstream>
#include "helpers.h"


void usage(const char *prog) {
  std::cout 
    << "Usage: cat feature_results | " << prog << " [ options ]\n"
    << "  --docids <path>  path to the file containing a list of docids (for docnum->docid mapping)\n"
    << "  --qrels <path>  path to file containing list of qrels for each qid-docid pair\n"
    << "  --no-add-missing  instead of listing all missing docids with a list of zero-features, drop them\n"
    << "  --no-assume-zero  instead of assuming missing qid-docid qrels are zero, drop those lines\n";
}

//assumes sorted docnums
//size_t get_docnum(const std::vector<std::string> &docids, std::string docid) {
//  if (docids.empty()) return -1;
//
//  size_t l = 0, r = docids.size()-1;
//  while (l <= r) {
//    size_t m = (l+r)/2;
//    int cmp = docid.compare(docids[m]);
//    if (cmp == 0) {
//      return m;
//    } else if (cmp < 0) {
//      if (m == 0) return -1; //because we use unsigned m value
//      else r = m-1;
//    } else {
//      l = m+1;
//    }
//  }
//  return -1;
//}

size_t get_docnum(const std::map<std::string,size_t> &docnums, const std::string &docid) {
  std::map<std::string,size_t>::const_iterator it = docnums.find(docid);
  if (it == docnums.end()) {
    return docnums.size()+1;
  } else {
    return it->second;
  }
}

int main(int argc, char *argv[]) {
  int argi = 1;
  const char *docid_path = NULL;
  const char *qrel_path = NULL;
  char *endptr;
  double offset = 0;
  long nfeatures = 0;
  std::string qid_prefix;
  bool numeric_qid = false;
  bool add_missing = true; //add documents that feature list missed but qrels contains
  bool assume_zero = true; //whether to assume missing qrels have score zero, or to remove lines that have no qrel
  long missing_score=0;
  long missing_qrel=0;
  long matched_qrel=0;
  bool with_score=false;

  while(argi < argc) {
    const char *arg = argv[argi++];
    if (0 == strcmp(arg,"--help")) {
      usage(argv[0]);
      return 0;
    } else if (0 == strcmp(arg,"--docids")) {
      docid_path = argv[argi++];
    } else if (0 == strcmp(arg,"--qrels")) {
      qrel_path = argv[argi++];
    } else if (0 == strcmp(arg,"--with-score")) {
      with_score=true;
    } else if (0 == strcmp(arg,"--numeric-qid")) {
      numeric_qid = true;
    } else if (0 == strcmp(arg,"--add-missing")) {
      add_missing = true;
    } else if (0 == strcmp(arg,"--assume-zero")) {
      assume_zero = true;
    } else if (0 == strcmp(arg,"--no-add-missing")) {
      add_missing = false;
    } else if (0 == strcmp(arg,"--no-assume-zero")) {
      assume_zero = false;
    } else if (0 == strcmp(arg,"--offset")) {
      offset = strtod(argv[argi],&endptr);
      if (!(*arg != '\0' && *endptr == '\0')) {
        std::cerr << "bad offset \"" << argv[argi] << "\". must be numeric\n";
        return 1;
      }
      argi++;
    } else if (0 == strcmp(arg,"--nfeatures")) {
      nfeatures = strtol(argv[argi],&endptr,10);
      if (!(*arg != '\0' && *endptr == '\0')) {
        std::cerr << "bad number of features \"" << argv[argi] << "\". must be numeric\n";
        return 1;
      }
      argi++;
    } else if (0 == strcmp(arg,"--")) {
      break;
    } else if (arg[0] == '-') {
      std::cerr << "unkown option \"" << arg << "\"\n";
      return 1;
    } else {
      argi--;
      break;
    }
  }

  std::string line;
  int clen = 1024;
  char *cline = (char*)malloc(clen);

  //first go through the list of docids so we can map docnum -> docid
  std::vector<std::string> docids;
  if (docid_path != NULL) {
    for (std::ifstream docid_file(docid_path); std::getline(docid_file,line); ) { //go through lines in docid_path
      docids.push_back(line);
    }
  }

  std::map<std::string,size_t> docnums;
  for(size_t i=0; i<docids.size(); ++i) {
    docnums.insert(std::pair<std::string,size_t>(docids[i],i));
  }

  //now we go through the list of qrels to get the correct evaluation value for each doc per query
  std::map<std::string, std::map<std::string,double> > qrels;
  if (qrel_path != NULL) {
    for (std::ifstream qrel_file(qrel_path); std::getline(qrel_file,line); ) { //go through lines in qrel_path
      if (clen <= (int)line.size()) {
        clen = line.size()+1;
        char *t = (char*)realloc(cline,clen);
        if (!t) { std::cerr << "couldn't realloc\n"; return -1; }
        cline = t;
      }
      strcpy(cline,line.c_str());
      std::vector<const char*> toks;
      for(char *tok = strtok(cline," \t\n"); tok; tok = strtok(NULL, " \t\n")) {
        toks.push_back(tok);
      }
      if (toks.size() == 0) { continue; } //empty line
      if (toks.size() > 4 || toks.size() > 5) { //empty line
        std::cerr << "bad input line \"" << line << "\"\n";
        return 1;
        //continue;
      }

      std::string qid;

      if (numeric_qid) {
        long nqid = strtol(toks[0],&endptr,10);
        if (!(*toks[0] != '\0' && *endptr == '\0')) {
          std::cerr << "bad numeric qid on line \"" << line << "\"\n";
          return 1;
          //continue;
        }
        std::stringstream ss;
        ss << nqid;
        qid = ss.str();
      } else {
        qid = std::string(toks[0]);
      }

      std::string docid(toks[2]);
      double qrel(strtod(toks.back(),&endptr));
      if (!(*toks.back() != '\0' && *endptr == '\0')) {
        std::cerr << "bad qrel on line \"" << line << "\"\n";
        return 1;
        //continue;
      }
      qrels[qid][docid] = qrel + offset;
    }
  }

  std::string prev_qid;
  //now we can start processing queries
  while(std::getline(std::cin,line)) {
    if (clen <= (int)line.size()) {
      clen = line.size()+1;
      char *t = (char*)realloc(cline,clen);
      if (!t) { std::cerr << "couldn't realloc\n"; return -1; }
      cline = t;
    }
    strcpy(cline,line.c_str());
    std::vector<const char*> toks;
    for(char *tok = strtok(cline," \t\n"); tok; tok = strtok(NULL, " \t\n")) {
      toks.push_back(tok);
    }

    std::string qid;
    if (!toks.empty()) {
      const char *colon = strchr(toks[0],':');
      if (colon != NULL) {
        qid_prefix = std::string(toks[0], colon - toks[0] + 1);
        toks[0] = colon+1;
      } else {
        qid_prefix.clear();
      }
      if (numeric_qid) {
        long nqid = strtol(toks[0],&endptr,10);
        if (!(*toks[0] != '\0' && *endptr == '\0')) {
          std::cerr << "bad numeric qid on line \"" << line << "\"\n";
          return 1;
          //continue;
        }
        std::stringstream ss;
        ss << nqid;
        qid = ss.str();
      } else {
        qid = std::string(toks[0]);
      }
    }

    if (!qrels.empty() && !prev_qid.empty() && (qid.empty() || prev_qid != qid)) { //hit end of query
      std::map<std::string, std::map<std::string,double> >::iterator qid_it = qrels.find(prev_qid);
      if (qid_it == qrels.end()) { //couldn't find previous qid in qrels, so just move on
        //std::cerr << "fatal error: qid_it invalid on followup\n";
        //return -1;
      } else {
        std::stringstream ss;
        ss << " " << qid_prefix << prev_qid;
        if (with_score) ss << " score:0";
        for(long i = 0; i < nfeatures; ++i) {
          ss << " " << i << ":0";
        }
        std::string zero_features(ss.str());
        //now go through the rest of the missing documents and give them features of all zeros
        for(std::map<std::string,double>::iterator it = qid_it->second.begin(); it != qid_it->second.end(); ++it) {
          if (add_missing) {
            size_t docnum = get_docnum(docnums,it->first);
            if (docnum < docids.size()) {
              std::cout << it->second << zero_features << " # docid: " << docnum << " " << it->first << "\n";
            } else {
              std::cout << it->second << zero_features << " # docid: NaN " << it->first << "\n";
            }
          }
          missing_score++;
        }
        qrels.erase(qid_it);
      }
      prev_qid = qid;
    }
    if (qid.empty()) {  //empty line
      prev_qid.clear();
      //std::cout << "\n";
      continue;
    }
    if (prev_qid.empty()) prev_qid = qid;


    //now parse docnum and convert to docid
    std::string docid;
    if (docid_path != NULL) {
      //get docnum and convert to docid
      long docnum = strtol(toks.back(),&endptr,10);
      if (!(*toks.back() != '\0' && *endptr == '\0')) {
        std::cerr << "bad docnum on line \"" << line << "\"\n";
        return 1;
        //continue;
      }

      if (docnum < 0 || docnum >= (long)docids.size()) {
        std::cerr << "out of bounds docnum on line \"" << line << "\"\n";
        return 1;
        //continue;
      } else {
        docid = docids[docnum];
      }
    } else {
      docid = std::string(toks.back());
    }

    std::stringstream ss;

    //now we need to figure out the qrel assigned evaluation for this document
    if (qrel_path != NULL) {
      //determine qrel
      double qrel;
      std::map<std::string, std::map<std::string,double> >::iterator qid_it = qrels.find(qid);
      if (qid_it == qrels.end()) { //if we couldn't even find the qid in qrels
        //std::cerr << "unknown (or out of order) qid " << qid << " on line \"" << line << "\"\n";
        //return 1;
        //continue;
        qrel = 0; //TODO: maybe print an error, but only once per query
        missing_qrel++;
        if (!assume_zero) continue; //skip this line (no qrel available)
      } else { //now we look for the docid in the qrels for this qid
        std::map<std::string,double>::iterator qrel_it = qid_it->second.find(docid);
        if (qrel_it == qid_it->second.end()) { //if qrels doesn't include this document
          qrel = 0;
          missing_qrel++;
          if (!assume_zero) continue; //skip this line (no qrel available)
        } else { //else qrels includes this document, use the assigned score
          qrel = qrel_it->second;
          qid_it->second.erase(qrel_it);
          matched_qrel++;
        }
      }
      ss << qrel << " ";
    }
    ss << line;
    if (docid_path != NULL) ss << " " << docid;
    std::cout << ss.str() << "\n";
  }

  //finally, wrap up the final qid we worked on (by adding missing entries)
  if (!prev_qid.empty() && qrel_path != NULL) {
    //TODO add missing entries
    std::map<std::string, std::map<std::string,double> >::iterator qid_it = qrels.find(prev_qid);
    if (qid_it == qrels.end()) {
      //std::cerr << "fatal error: qid_it invalid on followup\n";
      //return -1;
    } else {
      std::stringstream ss;
      ss << " " << qid_prefix << prev_qid;
      if (with_score) ss << " score:0";
      for(long i = 0; i < nfeatures; ++i) {
        ss << " " << i << ":0";
      }
      std::string zero_features(ss.str());
      for(std::map<std::string,double>::iterator it = qid_it->second.begin(); it != qid_it->second.end(); ++it) {
        if (add_missing) {
          size_t docnum = get_docnum(docnums,it->first);
          if (docnum < docids.size()) {
            std::cout << it->second << zero_features << " # docid: " << docnum << " " << it->first << "\n";
          } else {
            std::cout << it->second << zero_features << " # docid: NaN " << it->first << "\n";
          }
        }
        missing_score++;
      }
      qrels.erase(qid_it);
    }
  }
  std::cerr << "matched qrels: " << matched_qrel << "\n"
    << "missing qrels: " << missing_qrel << "\n"
    << "missing scores: " << missing_score << "\n";
}
