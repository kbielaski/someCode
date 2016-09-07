#include "stemmer.h"
#include "index_searcher.h"
#include "helpers.h"
#include "tfidf.h"
#include "bm25.h"
#include <iostream>
#include <set>
#include <cmath>
#include <inttypes.h>
#include <vector>
#include <queue>


typedef uint64_t docid_t;
typedef uint64_t termid_t;

//size_t rstore_chunk_size = 20;
//size_t tstore_chunk_size = 64;
//size_t xstore_chunk_size = 20;

#define MAX_DIST 10

void usage(const char *prog) {
  std::cout
    << "Usage: " << prog << " [ options ]\n"
    << "  --index <base path>  path to the index files (without the extension)\n";
}

void features_usage(bool with_score, bool with_features) {
  if (with_score) {
    std::cout << "  score:calculated ranking score\n";
  }
  if (with_features) {
    std::cout
    << "  0:body min counts\n"
    << "  1:body max counts\n"
    << "  2:body mean counts\n"
    << "  3:body gmean counts\n"
    << "  4:title min counts\n"
    << "  5:title max counts\n"
    << "  6:title mean counts\n"
    << "  7:title gmean counts\n"
    << "  8:body bigram min(1/min dist)\n"
    << "  9:body bigram max(1/min dist)\n"
    << "  10:body bigram mean(1/min dist)\n"
    << "  11:body bigram gmean(1/min dist)\n"
    << "  12:title bigram min(1/min dist)\n"
    << "  13:title bigram max(1/min dist)\n"
    << "  14:title bigram mean(1/min dist)\n"
    << "  15:title bigram gmean(1/min dist)\n"
    << "  16:body bigram min(1/min dist^2)\n"
    << "  17:body bigram max(1/min dist^2)\n"
    << "  18:body bigram mean(1/min dist^2)\n"
    << "  19:body bigram gmean(1/min dist^2)\n"
    << "  20:title bigram min(1/min dist^2)\n"
    << "  21:title bigram max(1/min dist^2)\n"
    << "  22:title bigram mean(1/min dist^2)\n"
    << "  23:title bigram gmean(1/min dist^2)\n"
    << "  24:body tfidf\n"
    << "  25:title tfidf\n"
    << "  26:body+title concat tfidf\n"
    << "  27:body bm25\n"
    << "  28:title bm25\n"
    << "  29:body+title concat bm25\n"
    << "  30:body bigram tfidf\n"
    << "  31:title bigram tfidf\n"
    << "  32:body bigram bm25\n"
    << "  33:title bigram bm25\n";
  }
}

typedef std::pair<uint64_t,uint64_t> range_t;
bool range_intersect(const range_t &lhs, const range_t &rhs) {
    return lhs.first <= rhs.second && rhs.first <= lhs.second;
}
range_t range_intersection(const range_t &lhs, const range_t &rhs) {
    return range_t(std::max(lhs.first, rhs.first), std::min(lhs.second,rhs.second));
}
bool range_lt(const range_t &lhs, const range_t &rhs) {
    return lhs.second < rhs.first;
}
#define rcount(term,chunk) ((feature_vectors[term].size()+chunk-1)/(chunk))
#define rmin(term,i,chunk) (feature_vectors[term][(chunk)*(i)].first)
#define rmax(term,i,chunk) (feature_vectors[term][(chunk)*((i)+1)-1].first)
#define rrange(term,i,chunk) (range_t(feature_vectors[term][(chunk)*(i)].first,feature_vectors[term][std::min((chunk)*((i)+1)-1,feature_vectors[term].size()-1)].first))
std::vector<std::vector<uint32_t> > range_intersection(std::map<std::string,std::vector<std::pair<docid_t,double> > > feature_vectors, std::vector<std::string> terms, size_t num_base_terms, size_t rstore_chunk_size);


stemmer *z;
inline void clean_text(char *buffer) {
  for(char *p = buffer;*p;++p) {
    char c = *p;
    if (!isalnum(c)) *p = ' '; //TODO: decide whether to include '-'
  }
  stem_str(z,buffer); //also does to-lower, but leaves symbols in-place
}
std::string make_pair(std::string term1, std::string term2) {
  if (term1.compare(term2) < 0) {
    return term1 + "|" + term2;
  } else {
    return term2 + "|" + term1;
  }
}

std::string make_title_pair(std::string term1, std::string term2) {
  if (term1.compare(term2) < 0) {
    return "TTL|" + term1 + "|" + term2;
  } else {
    return "TTL|" + term2 + "|" + term1;
  }
}

std::string make_title_term(std::string term) {
  return "TTL|" + term;
}

uint64_t compute_term_id(std::string term) {
  const char *buf = term.c_str();
  const size_t buflen = term.size();

  return serializer::get_SHA1(buf,buflen);
}

std::vector<std::string> generate_extra_terms(std::vector<std::string> base_terms) {
  std::vector<std::string> extra_terms;
  //for(size_t i = 0; i < base_terms.size(); ++i) {
  //    extra_terms.push_back(clean_term(base_terms[i]));
  //}
  for(size_t i = 0; i < base_terms.size(); ++i) {
    extra_terms.push_back(make_title_term(base_terms[i]));
  }
  for(size_t i = 0; i < base_terms.size(); ++i) {
    for(size_t j = i+1; j < base_terms.size(); ++j) {
      extra_terms.push_back(make_pair(base_terms[i],base_terms[j]));
    }
  }
  for(size_t i = 0; i < base_terms.size(); ++i) {
    for(size_t j = i+1; j < base_terms.size(); ++j) {
      extra_terms.push_back(make_title_pair(base_terms[i],base_terms[j]));
    }
  }
  return extra_terms;
}

bool lookup_features(std::string term, std::vector<std::pair<docid_t,double> > &features) {
  bool istitle = false;
  bool ispair = false;
  std::string term1 = term;
  std::string term2;
  if (0 == term.substr(0,4).compare("TTL|")) {
    istitle = true;
    term1 = term.substr(4);
  }
  std::size_t found = term.find("|");
  if (found != std::string::npos) {
    ispair = true;
    term2 = term1.substr(found+1);
    term1 = term1.substr(0,found);
  }

  if (istitle) {
    if (ispair) {
      term1 = "TTL|" + term1;
      term2 = "TTL|" + term2;
    } else {
      term1 = "TTL|" + term1;
    }
  }
  return false;
}

std::vector<std::pair<docid_t, double> > intersect_terms(std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting1,std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting2, uint32_t distcap) {
  std::vector<std::pair<docid_t,std::vector<uint32_t> > >::const_iterator it1 = posting1.begin();
  std::vector<std::pair<docid_t,std::vector<uint32_t> > >::const_iterator it2 = posting2.begin();
  std::vector<std::pair<docid_t, double> > ret;
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
        ret.push_back(std::pair<docid_t,double>(it1->first, 1.0 / (double)dist));
      }
      ++it1;
      ++it2;
    }
  }
  return ret;
}

double min_feature(std::vector<double>::const_iterator begin, std::vector<double>::const_iterator end) {
  if (begin == end) return 0;
  double min = *begin;
  for(++begin; begin != end; ++begin) {
    if (*begin < min) min = *begin;
  }
  return min;
}

double max_feature(std::vector<double>::const_iterator begin, std::vector<double>::const_iterator end) {
  if (begin == end) return 0;
  double max = *begin;
  for(++begin; begin != end; ++begin) {
    if (*begin > max) max = *begin;
  }
  return max;
}

double mean_feature(std::vector<double>::const_iterator begin, std::vector<double>::const_iterator end) {
  //given f1,f2,f3,...,fn, produces mean f value
  if (begin == end) return 0;
  double sum = *begin;
  int count = 1;
  for(++begin; begin != end; ++begin) {
    sum += *begin;
    ++count;
  }
  return sum / count;
}

double weighted_sum_features(std::vector<double>::const_iterator fbegin, std::vector<double>::const_iterator fend, std::vector<double>::const_iterator wbegin, std::vector<double>::const_iterator wend) {
  //given f1,f2,f3,...,fn,w1,w2,w3,..,wn  produces f1*w1 + f2*w2 + f3*w3 + ... + fn*wn
  if (fbegin == fend) return 0;
  if (wbegin == wend) return -1;
  double sum = (*fbegin) * (*wbegin);
  for(++fbegin; fbegin != fend; ++fbegin) {
    if (++wbegin == wend) return -1;
    sum += (*fbegin) * (*wbegin);
  }
  if (++wbegin != wend) return -1;
  return sum;
}

double gmean_feature(std::vector<double>::const_iterator begin, std::vector<double>::const_iterator end) {
  //given f1,f2,f3,...,fn, produces nth root of (f1*f2*f3*...*fn)
  if (begin == end) return 0;
  double prod = *begin;
  int count = 1;
  for(++begin; begin != end; ++begin) {
    prod *= *begin;
    ++count;
  }
  return std::pow(prod, 1.0 / count);
}

void square_features(std::vector<double> &ret, std::vector<double>::const_iterator begin, std::vector<double>::const_iterator end) {
  //given list of f (or 1/f), produces list of f^2 (or 1/(f^2))
  ret.clear();
  if (begin == end) return;
  for(++begin; begin != end; ++begin) {
    ret.push_back(*begin * *begin);
  }
}

void denom_root_features(std::vector<double> &ret, std::vector<double>::const_iterator begin, std::vector<double>::const_iterator end) {
  //given list of 1/f, produces list of 1/sqrt(f)
  ret.clear();
  if (begin == end) return; 
  for(++begin; begin != end; ++begin) {
    ret.push_back(1.0 / sqrt(1.0 / *begin));
  }
}

double score_document(const std::vector<double> &features, const std::vector<double> &weights) {
  return weighted_sum_features(features.begin(),features.end(),weights.begin(),weights.end());
}

std::map<docid_t,double> rank_documents(const std::map<docid_t,std::vector<double> > &features, const std::vector<double> &weights) {
  std::map<docid_t,double> scores;
  for(std::map<docid_t,std::vector<double> >::const_iterator it = features.begin(); it != features.end(); ++it) {
    if (weights.size() != it->second.size()) {
      std::cerr << "weight-feature size mismatch: weights( " << weights.size() << " ), features( " << it->second.size() << " )\n";
      exit(-1);
    }
    std::pair<docid_t,double> score(it->first, score_document(it->second, weights));
    scores.insert(score);
  }

  return scores;
}

double norm_feature(double f, bool zero_floor) {
  if (zero_floor) {
    return std::max(f,0.0);
  } else {
    return f;
  }
}

int parse_double_list(char *arg, std::vector<double> &vec) {
  const char *delim = " ,;";
  int r=0;

  for(char *saveptr, *tok=strtok_r(arg, delim, &saveptr); tok; tok=strtok_r(NULL, delim, &saveptr)) {
    char *endptr;
    vec.push_back(strtod(tok,&endptr));
    if (*endptr != '\0' || endptr == tok) {
      if (*tok=='\0') {
        continue; //ignore empty token
      }
      return -1;
    }
    r++;
  }
  return r;
}

struct sort_column_class {
  int sort_column;
  std::map<uint64_t, double> scores;
  bool reverse;
  bool operator() (std::pair<docid_t,std::vector<double> > a, std::pair<docid_t,std::vector<double> > b) {
    if (sort_column == -1) {
      if(reverse == false)
        return(scores[a.first] < scores[b.first]);
      else
        return(scores[a.first] > scores[b.first]);
    } else {
      if(reverse == false)
        return(a.second.at(sort_column) < b.second.at(sort_column));
      else
        return(a.second.at(sort_column) > b.second.at(sort_column));
    }
  }
};

/*bool sort_column_function(std::pair<docid_t,std::vector<double> > a, std::pair<docid_t,std::vector<double> > b, int sort_column) {
  return(a.second.at(sort_column) < b.second.at(sort_column));
  }*/

int main(int argc, char *argv[]) {
  z = create_stemmer();

  const char *index_path = "\0";
  bool do_rset = false;
  bool do_rset_savings = false;
  bool quiet = true;
  bool do_features = true;
  bool do_sort = false;
  bool reverse = false;
  int sort_column = -1;
  bool show_queries = false;
  bool term_count = false;
  bool parse_qid = false;
  bool show_postings = false;
  bool read_stdin = true;
  bool zero_floor = false;
  bool do_intersect_terms = true;
  bool do_count = false;
  size_t rstore_chunk_size = 20;
  std::queue<const char *> arg_queries;
  std::vector<double> weights;

  int argi = 1;
  while (argi < argc) {
    const char *arg = argv[argi++];
    if (!strcmp(arg,"--index")) {
      if (argi >= argc) { std::cerr << "missing option to --index\n"; return 1; }
      index_path = argv[argi++];
    } else if (!strcmp(arg,"--list-features")) {
      features_usage(!weights.empty(), do_features);
      return 0;
    } else if (!strcmp(arg,"--rset")) {
      do_rset = true;
    } else if (!strcmp(arg,"--rset-chunk")) {
      if (argi >= argc) { std::cerr << "missing option to " << arg << "\n"; return 1; }
      char *endptr;
      rstore_chunk_size = strtol(argv[argi++],&endptr,10);
      if (*endptr != '\0' || endptr == argv[argi-1]) {
        std::cerr << "invalid chunk size for " << arg << "\n"; return 1; 
        return -1;
      }
      do_rset = true;
    } else if (!strcmp(arg,"--no-rset")) {
      do_rset = false;
    } else if (!strcmp(arg,"--rset-compare")) {
      do_rset_savings = true;
    } else if (!strcmp(arg,"--weights") || !strcmp(arg,"-w")) {
      if (argi >= argc) { std::cerr << "missing option to " << arg << "\n"; return 1; }
      weights.clear();
      if (0 > parse_double_list(argv[argi++], weights)) {
        std::cerr << "invalid weight list for ranking " << arg << "\n"; return 1;
      }
    } else if (!strcmp(arg,"--features")) {
      do_features = true;
    } else if (!strcmp(arg,"--count")) {
      do_count = true;
    } else if (!strcmp(arg,"--postings")) {
      show_postings = true;
    } else if (!strcmp(arg,"--no-features")) {
      do_features = false;
    } else if (!strcmp(arg,"--sort") || !strcmp(arg,"--rsort")) {
      if (argi >= argc) { std::cerr << "missing option to " << arg << "\n"; return 1; }
      char *endptr;
      sort_column = (int)strtol(argv[argi++],&endptr,10);
      if (*endptr != '\0' || endptr == argv[argi-1]) {
        std::cerr << "invalid column index for " << arg << "\n"; return 1; 
        return -1;
      }
      if(!strcmp(arg,"--rsort")) {
	  reverse = true;
	}
      do_sort = true;
    } else if (!strcmp(arg,"--ranking")) {
      do_sort = true;
      reverse = true;
      sort_column = -1;
    } else if (!strcmp(arg,"--zero-floor")) {
      zero_floor = true;
    } else if (!strcmp(arg,"--query") || !strcmp(arg,"-q")) {
      if (argi >= argc) { std::cerr << "missing option to " << arg << "\n"; return 1; }
      arg_queries.push(argv[argi++]);
      read_stdin = false;
    } else if (!strcmp(arg,"--show-query")) {
      show_queries = true;
    } else if (!strcmp(arg,"--no-show-query")) {
      show_queries = false;
    } else if (!strcmp(arg,"--or-mode")) {
      do_intersect_terms = false;
    } else if (!strcmp(arg,"--and-mode")) {
      do_intersect_terms = true;
    } else if (!strcmp(arg,"--quiet") || !strcmp(arg,"-q")) {
      quiet = true;
    } else if (!strcmp(arg,"--verbose") || !strcmp(arg,"-v")) {
      quiet = false;
    } else if (!strcmp(arg,"--term-count")) {
      term_count = true;
    } else if (!strcmp(arg,"--qid")) {
      parse_qid = true;
    } else if (!strcmp(arg,"--")) {
      break;
    } else if (arg[0] == '-') {
      std::cerr << "unknown option '" << arg << "'\n"; return 1;
    } else {
      argi--;
      break;
    }
  }

  if (do_sort && sort_column < 0 && weights.empty()) {
    std::cerr << "score sort specified, but no scoring weights given\n";
    return 1;
  }

  if (index_path[0] == '\0' && argi < argc) index_path = argv[argi++];
  
  if (index_path[0] == '\0') {
    std::cerr << "must specify index path (with any extension, just the base filename of the index)\n";
    return 1;
  }

  if (argi < argc) {
    std::cerr << "unkown extra program arguments\n";
    return 1;
  }


  std::string fname(index_path);
  index_searcher searcher(fname);
  if (!searcher.is_open()) {
    std::cerr << "failed to open index '" << index_path << "'\n";
  }


    char *tok;
    const char *qid_str = NULL;
    int qidi = 0;
    size_t linelen = 1024;
    char *line = new char[linelen];
    std::string strline;
    while(!arg_queries.empty() || read_stdin) {
      // std::cout<< "got here" <<std::endl;
      if (!arg_queries.empty()) {
        size_t qlen = strlen(arg_queries.front());
        if ( qlen >= linelen ) {
          delete[] line;
          while (qlen >= linelen) {
            linelen *= 1.5;
          }
          line = new char[linelen];
        }
        strcpy(line,arg_queries.front());
        arg_queries.pop();
      } else if (read_stdin) {
        if (!getline(std::cin, strline)) break;
        if (strline.size() < 1) continue;
        if (strline.size() >= linelen) {
          delete[] line;
          while (strline.size() >= linelen) {
            linelen *= 1.5;
          }
          line = new char[linelen];
        }
        strcpy(line,strline.c_str());
      } else {
        std::cerr << "error, incorrect search state";
      }

      if (parse_qid) {
        tok = strpbrk(line, " \t\n");
        if (tok == NULL) {
          continue;
        } else {
          qid_str = line;
          *tok = '\0';
          line=tok+1;
        }
      }

      strline = std::string(line);

      clean_text(line); //stem and remove non-alphanumerics
      if (show_queries) {
        if (parse_qid) {
          //std::cout << "query(qid:" << qid_str << ") raw( " << strline << " ) clean( "; << line << " )\n";
          std::cout << "query(qid:" << qid_str << ") raw( " << strline << " ) clean(";
        } else {
          //std::cout << "query(qid:" << qidi << ") raw( " << strline << " ) clean( "; << line << " )\n";
          std::cout << "query(qid:" << qidi << ") raw( " << strline << " ) clean(";
        }
      }

      std::vector<std::string> base_terms;
      std::vector<std::string> title_terms;
      std::vector<std::string> body_pair_terms;
      std::vector<std::string> title_pair_terms;

      for(tok = strtok(line, " \t\n"); tok; tok = strtok(NULL, " \t\n")) {
        base_terms.push_back(std::string(tok));
        if (show_queries) std::cout << " " << tok;
      }
      if (show_queries) std::cout << " )\n";


      //1. lookup base terms and base title terms
      //2. do range searches
      //  a) calculate range ids
      //  b) convert ranges to Xstore/Tstore chunks
      //  c) report statistics
      //3. do intersection
      //  a) get raw features for each term
      //  b) calculate composite features
      //  c) report composite features

      std::map<std::string,std::vector<std::pair<docid_t,std::vector<uint32_t> > > > postings;
      std::map<std::string,std::vector<std::pair<docid_t,double> > > feature_vectors;

      //first lookup base terms
      for(std::vector<std::string>::const_iterator it = base_terms.begin(); it != base_terms.end(); ++it) {
        std::string term = *it;
        std::vector<std::pair<uint64_t,std::vector<uint32_t> > > posting;
        std::vector<std::pair<uint64_t,double> > features;
        if (!searcher.lookup_term(compute_term_id(term), posting)) {
          //std::cerr << "couldn't find base term: " << *it << "\n";
          //exit(-1);
          //search_fail = true;
          //break;
          continue;
        }
        if (show_postings) {
          std::cout << term;
          for(std::vector<std::pair<uint64_t,std::vector<uint32_t> > >::const_iterator it2 = posting.begin(); it2 != posting.end(); ++it2) {
            std::cout << " " << it2->first << ":";
            for(std::vector<uint32_t>::const_iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++it3) {
              if (it3 != it2->second.begin()) {
                std::cout << ",";
              }
              std::cout << *it3;
            }
            features.push_back(std::pair<uint64_t,double>(it2->first,it2->second.size()));
          }
          std::cout << "\n";
        }
        for(std::vector<std::pair<uint64_t,std::vector<uint32_t> > >::const_iterator it2 = posting.begin(); it2 != posting.end(); ++it2) {
          features.push_back(std::pair<uint64_t,double>(it2->first,it2->second.size()));
        }
        postings.insert(std::pair<std::string,std::vector<std::pair<docid_t,std::vector<uint32_t> > > >(term, posting));
        feature_vectors.insert(std::pair<std::string, std::vector<std::pair<docid_t,double> > >(term,features));

        //std::cout << term << "( " << features.size() << " )";
        //for(std::vector<std::pair<uint64_t,double> >::const_iterator it2 = features.begin(); it2 != features.end(); ++it2) {
        //  std::cout << " " << it2->first << ":" << it2->second;
        //}
        //std::cout << "\n";
      }
      //if (search_fail) continue;

      //now add feature vectors for base title terms
      for(std::vector<std::string>::const_iterator it = base_terms.begin(); it != base_terms.end(); ++it) {
        std::string term = make_title_term(*it);
        title_terms.push_back(term);
        std::vector<std::pair<uint64_t,std::vector<uint32_t> > > posting;
        std::vector<std::pair<uint64_t,double> > features;
        if (!searcher.lookup_term(compute_term_id(term), posting)) {
          //std::cerr << "couldn't find base term: " << *it << "\n";
          //exit(-1);
        }
        if (show_postings) {
          std::cout << term;
          for(std::vector<std::pair<uint64_t,std::vector<uint32_t> > >::const_iterator it2 = posting.begin(); it2 != posting.end(); ++it2) {
            std::cout << " " << it2->first << ":";
            for(std::vector<uint32_t>::const_iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++it3) {
              if (it3 != it2->second.begin()) {
                std::cout << ",";
              }
              std::cout << *it3;
            }
          }
          std::cout << "\n";
        }
        for(std::vector<std::pair<uint64_t,std::vector<uint32_t> > >::const_iterator it2 = posting.begin(); it2 != posting.end(); ++it2) {
          features.push_back(std::pair<uint64_t,double>(it2->first,it2->second.size()));
        }
        postings.insert(std::pair<std::string,std::vector<std::pair<docid_t,std::vector<uint32_t> > > >(term, posting));
        feature_vectors.insert(std::pair<std::string, std::vector<std::pair<docid_t,double> > >(term,features));

        //std::cout << term << "( " << features.size() << " )";
        //for(std::vector<std::pair<uint64_t,double> >::const_iterator it2 = features.begin(); it2 != features.end(); ++it2) {
        //  std::cout << " " << it2->first << ":" << it2->second;
        //}
        //std::cout << "\n";

        //this code is bad, but it ensures that any term that appears in the title also appears in the body feature vector (with a frequency of zero)
        std::vector<std::pair<uint64_t,double> > new_base_fvec;
        std::vector<std::pair<uint64_t,double> >::const_iterator body_it = feature_vectors[*it].begin();
        std::vector<std::pair<uint64_t,double> >::const_iterator title_it = features.begin();
        while(body_it != feature_vectors[*it].end() || title_it != features.end()) {
          if (title_it == features.end()) {
            new_base_fvec.push_back(*body_it);
            ++body_it;
          } else if (body_it == feature_vectors[*it].end()) {
            new_base_fvec.push_back(std::pair<uint64_t,double>(title_it->first,0.0));
            std::vector<uint32_t> tv;
            ++title_it;
          } else if (body_it->first < title_it->first) {
            new_base_fvec.push_back(*body_it);
            ++body_it;
          } else if (body_it->first > title_it->first) {
            new_base_fvec.push_back(std::pair<uint64_t,double>(title_it->first,0.0));
            ++title_it;
          } else { //equal
            new_base_fvec.push_back(*body_it);
            ++body_it;
            ++title_it;
          }
        }
        feature_vectors[*it] = new_base_fvec;
        //std::cout << term << "( " << new_base_fvec.size() << " )";
        //for(std::vector<std::pair<uint64_t,double> >::const_iterator it2 = new_base_fvec.begin(); it2 != new_base_fvec.end(); ++it2) {
        //  std::cout << " " << it2->first << ":" << it2->second;
        //}
        //std::cout << "\n";
      }

      bool search_fail = false;
      for(std::vector<std::string>::const_iterator it = base_terms.begin(); it != base_terms.end(); ++it) {
        if (feature_vectors[*it].size() < 1) {
          if (!quiet) std::cout << "couldn't find base term: " << *it << "\n";
          search_fail = true;
          break;
        }
      }
      if (search_fail) { qidi++; continue; }

      //at this point we have everything we need for the rest of the search

      //body pairs
      for(size_t i = 0; i < base_terms.size(); ++i) {
        std::string t1 = base_terms[i];
        for(size_t j = i+1; j < base_terms.size(); ++j) {
          std::string t2 = base_terms[j];
          std::string term = make_pair(t1,t2);
          body_pair_terms.push_back(term);
          std::vector<std::pair<uint64_t,double> > features = intersect_terms(postings[t1],postings[t2],MAX_DIST);
          feature_vectors.insert(std::pair<std::string, std::vector<std::pair<docid_t,double> > >(term,features));

          if (term_count) {
            std::cout << term << "( " << features.size() << " )";
            //for(std::vector<std::pair<uint64_t,double> >::const_iterator it2 = features.begin(); it2 != features.end(); ++it2) {
            //  std::cout << " " << it2->first << ":" << it2->second;
            //}
            std::cout << "\n";
          }
        }
      }

      //title pairs
      for(size_t i = 0; i < base_terms.size(); ++i) {
        std::string t1 = base_terms[i];
        for(size_t j = i+1; j < base_terms.size(); ++j) {
          std::string t2 = base_terms[j];
          std::string term = make_title_pair(t1,t2);
          title_pair_terms.push_back(term);
          std::vector<std::pair<uint64_t,double> > features = intersect_terms(postings[make_title_term(t1)],postings[make_title_term(t2)],MAX_DIST);
          feature_vectors.insert(std::pair<std::string, std::vector<std::pair<docid_t,double> > >(term,features));

          if (term_count) {
            std::cout << term << "( " << features.size() << " )";
            //for(std::vector<std::pair<uint64_t,double> >::const_iterator it2 = features.begin(); it2 != features.end(); ++it2) {
            //std::cout << " " << it2->first << ":" << it2->second;
            //}
            std::cout << "\n";
          }
        }
      }

      //body pair tf
      std::map<docid_t,std::vector<double>> bigram_results_body;
      for(std::vector<std::string>::iterator it = body_pair_terms.begin(); it != body_pair_terms.end(); it++) {
	std::size_t found = it->find("|");
	std::string term1;
	std::string term2;
	if(found != std::string::npos) {
	  term1 = it->substr(0,found);
	  term2 = it->substr(found+1);
	}
	int term1_it = 0;
	int term2_it = 0;
	//double numBigrams = 0;

	while(term1_it < postings[term1].size() && term2_it < postings[term2].size()) {
	  if(postings[term1].at(term1_it).first == postings[term2].at(term2_it).first) {
	    double numBigrams = (double)compute_bigram_count(MAX_DIST,postings[term1].at(term1_it).second,postings[term2].at(term2_it).second);
	    //std::cout << "The number of body Bigrams for doc id " << postings[term1].at(term1_it).first << "for terms " << term1 << " and " << term2 << " is: " << numBigrams << std::endl;
	    bigram_results_body[postings[term1].at(term1_it).first].push_back(numBigrams);
	    term1_it++;
	    term2_it++;
	    }
	  else if(postings[term1].at(term1_it).first < postings[term2].at(term2_it).first) {
	    bigram_results_body[postings[term1].at(term1_it).first].push_back(0);
	    term1_it++;
	  }
	  else if(postings[term2].at(term2_it).first < postings[term1].at(term1_it).first) {
	    bigram_results_body[postings[term2].at(term2_it).first].push_back(0);
	    term2_it++;
	  }
	}
      }

      //title pair tf
      std::map<docid_t,std::vector<double>> bigram_results_title;
      for(std::vector<std::string>::iterator it = body_pair_terms.begin(); it != body_pair_terms.end(); it++) {
	std::size_t found = it->find("|");
	std::string term1;
	std::string term2;
	if(found != std::string::npos) {
	  term1 = it->substr(0,found);
	  term2 = it->substr(found+1);
	  term1 = "TTL|" + term1;
	  term2 = "TTL|" + term2;
	}
	int term1_it = 0;
	int term2_it = 0;

	while(term1_it < postings[term1].size() && term2_it < postings[term2].size()) {
	  if(postings[term1].at(term1_it).first == postings[term2].at(term2_it).first) {
	    double numBigrams = (double)compute_bigram_count(MAX_DIST,postings[term1].at(term1_it).second,postings[term2].at(term2_it).second);
	    bigram_results_title[postings[term1].at(term1_it).first].push_back(numBigrams);
	    term1_it++;
	    term2_it++;
	    }
	  else if(postings[term1].at(term1_it).first < postings[term2].at(term2_it).first) {
	    bigram_results_title[postings[term1].at(term1_it).first].push_back(0);
	    term1_it++;
	  }
	  else if(postings[term2].at(term2_it).first < postings[term1].at(term1_it).first) {
	    bigram_results_title[postings[term2].at(term2_it).first].push_back(0);
	    term2_it++;
	  }
	}
      }

      //Now we have all of our raw features

      std::map<docid_t,std::vector<double> > raw_doc_features;

      if (do_intersect_terms) {
        //first lets do the basic intersection, this tells us which documents we care about
        std::vector<std::vector<std::pair<docid_t,double> >::const_iterator> base_its;
        std::vector<std::vector<std::pair<docid_t,double> >::const_iterator> end_its;
        for(std::vector<std::string>::const_iterator it = base_terms.begin(); it != base_terms.end(); ++it) {
          base_its.push_back(feature_vectors[*it].begin());
          end_its.push_back(feature_vectors[*it].end());
        }
        std::string sterm = base_terms[0];
        //std::cout << "docs of interest: ";
        while(base_its[0] != end_its[0]) {
          bool match = true;
          bool fin = false;
          docid_t doc = base_its[0]->first;
          for(size_t i = 1; i < base_its.size(); ++i) {
            if (base_its[i] == end_its[i]) {fin=true;break;}
            while(base_its[i]->first < doc) {
              ++(base_its[i]);
              if (base_its[i] == end_its[i]) {fin=true;break;}
            }
            if (fin) break;
            if (base_its[i]->first == doc) ++(base_its[i]);
            else { match = false; break; }
          }
          if (fin) break;
          if (match) {
            std::vector<double> temp;
            raw_doc_features.insert(std::pair<docid_t,std::vector<double> >(doc,temp));
            //std::cout << " " << doc;
          }
          base_its[0]++;
        }
        //std::cout << "\n";
      } else {
        for(std::vector<std::string>::const_iterator it = base_terms.begin(); it != base_terms.end(); ++it) {
          for(std::vector<std::pair<docid_t,double> >::const_iterator it2 = feature_vectors[*it].begin(); it2 != feature_vectors[*it].end(); ++it2) {
            std::vector<double> temp;
            raw_doc_features.insert(std::pair<docid_t,std::vector<double> >(it2->first,temp));
          }
        }
      }

      int base_i = 0;
      int title_i = base_terms.size();
      int pair_i = title_i + title_terms.size();
      int title_pair_i = pair_i + body_pair_terms.size();
      int end_i = title_pair_i + title_pair_terms.size();

      std::vector<std::string> all_terms(end_i);
      std::copy(base_terms.begin(), base_terms.end(),all_terms.begin() + base_i);
      std::copy(title_terms.begin(), title_terms.end(),all_terms.begin() + title_i);
      std::copy(body_pair_terms.begin(), body_pair_terms.end(),all_terms.begin() + pair_i);
      std::copy(title_pair_terms.begin(), title_pair_terms.end(),all_terms.begin() + title_pair_i);

      //std::cout << "all terms:";
      //for(std::vector<std::string>::const_iterator it = all_terms.begin(); it != all_terms.end(); ++it) {
      //  std::cout << " " << *it;
      //}
      //std::cout << "\n";

      //now lets scan all feature vectors to build the document based feature vectors (rotation)
      for(std::vector<std::string>::const_iterator it = all_terms.begin(); it != all_terms.end(); ++it) {
        std::vector<std::pair<uint64_t,double> >::iterator it2 = feature_vectors[*it].begin();
        std::map<docid_t,std::vector<double> >::iterator it_f = raw_doc_features.begin();
        while (it2 != feature_vectors[*it].end() && it_f != raw_doc_features.end()) {
          if (it2->first < it_f->first) { //unimportant doc
            ++it2;
          } else if (it_f->first < it2->first) { //missing doc
            it_f->second.push_back(0.0);
            ++it_f;
          } else { //lined up
            it_f->second.push_back(it2->second);
            ++it2;
            ++it_f;
          }
        }
        while(it_f != raw_doc_features.end()) {
          it_f->second.push_back(0.0);
          ++it_f;
        }
      }

      if (do_count) {
        std::cout << "Search Result Count: " << raw_doc_features.size() << "\n";
      }

      if (do_rset || do_rset_savings) {
        std::vector<std::vector<uint32_t> > rangeids = range_intersection(feature_vectors, all_terms, base_terms.size(), rstore_chunk_size);
        if (!quiet) std::cout << "============BEGIN RSET RESULTS=============\n";
        if (do_rset) {
          //finally lets go back and calculate chunking (Rstore effectiveness)
          for(size_t i = 0; i < rangeids.size(); ++i) {
            std::cout << all_terms[i] << ":" << rangeids[i].size() << "/" << rcount(all_terms[i],rstore_chunk_size) << "\n";
          }
        }


        if (do_rset_savings) {
          int sterm_i = -1;
          size_t sterm_hits = 0;
          for(size_t i = 0; i < base_terms.size(); ++i) {
            size_t term_hits = rangeids[i].size();
            if (sterm_i == -1 || term_hits < sterm_hits) {
              sterm_i = i;
              sterm_hits = term_hits;
            }
          }


          size_t tstore_hits = 0;
          size_t tstore_chunks = 0;
          size_t xstore_hits = 0;
          size_t xstore_chunks = 0;
          for(int i = 0; i < (int)rangeids.size(); ++i) {
            if (i == sterm_i) {
              tstore_hits = rangeids[i].size();
              tstore_chunks = rcount(all_terms[i],rstore_chunk_size) + 1; //+1 because we have to do an extra check to find end of posting (if we weren't using rset)
            } else {
              xstore_hits += rangeids[i].size();
              xstore_chunks += rcount(all_terms[i],rstore_chunk_size);
            }
          }

          //size_t tstore_lookups = tstore_hits * rstore_chunk_size;
          //size_t xstore_lookups = xstore_hits * rstore_chunk_size;
          //size_t tstore_possible_lookups = tstore_chunks * rstore_chunk_size;
          //size_t xstore_possible_lookups = tstore_chunks * rstore_chunk_size * (rangeids.size() - 1);
          //size_t total_lookups = tstore_lookups + xstore_lookups;
          //size_t total_possible_lookups = tstore_possible_lookups + xstore_possible_lookups;

          size_t total_hits = tstore_hits + xstore_hits;
          size_t total_chunks = tstore_chunks + xstore_chunks;

          double tstore_save_chunk_percent = ( 100.0 * (double)((tstore_chunks) - tstore_hits) / (double)(tstore_chunks) );
          double xstore_save_chunk_percent = ( 100.0 * (double)((xstore_chunks) - xstore_hits) / (double)(xstore_chunks) );
          double total_save_chunk_percent = ( 100.0 * (double)((total_chunks) - total_hits) / (double)(total_chunks) );
          //double tstore_save_lookup_percent = ( 100.0 * (double)((tstore_possible_lookups) - tstore_lookups) / (double)(tstore_possible_lookups) );
          //double xstore_save_lookup_percent = ( 100.0 * (double)((xstore_possible_lookups) - xstore_lookups) / (double)(xstore_possible_lookups) );
          //double total_save_lookup_percent = ( 100.0 * (double)((total_possible_lookups) - total_lookups) / (double)(total_possible_lookups) );

          //if (tstore_chunks == 0) tstore_save_chunk_percent = 100.0;
          //if (xstore_chunks == 0) xstore_save_chunk_percent = 100.0;
          //if (total_chunks == 0) total_save_chunk_percent = 100.0;
          //if (tstore_possible_lookups == 0) tstore_save_lookup_percent = 100.0;
          //if (xstore_possible_lookups == 0) xstore_save_lookup_percent = 100.0;
          //if (total_possible_lookups == 0) total_save_lookup_percent = 100.0;


          std::cout << "R-store T-store saved chunks: " << ((tstore_chunks) - tstore_hits) << " / " << (tstore_chunks) << " ( " << tstore_save_chunk_percent << "% )\n";
          std::cout << "R-store X-store saved chunks: " << ((xstore_chunks) - xstore_hits) << " / " << (xstore_chunks) << " ( " << xstore_save_chunk_percent << "% )\n";
          std::cout << "R-store total saved chunks: " << ((total_chunks) - total_hits) << " / " << (total_chunks) << " ( " << total_save_chunk_percent << "% )\n";
          //std::cout << "R-store T-store saved lookups: " << ((tstore_possible_lookups) - tstore_lookups) << " / " << (tstore_possible_lookups) << " ( " << tstore_save_lookup_percent << "% )\n";
          //std::cout << "R-store X-store saved lookups: " << ((xstore_possible_lookups) - xstore_lookups) << " / " << (xstore_possible_lookups) << " ( " << xstore_save_lookup_percent << "% )\n";
          //std::cout << "R-store total saved lookups: " << ((total_possible_lookups) - total_lookups) << " / " << (total_possible_lookups) << " ( " << total_save_lookup_percent << "% )\n";
        }
        if (!quiet) std::cout << "============END RSET RESULTS=============\n";
      }

      std::map<docid_t,std::vector<double> > ranking_features;
      if (do_features || do_sort || !weights.empty()) {

        uint64_t ndocs = searcher.get_ndocs();
        std::vector<double> bm25_body_idf_ = bm25_idf(postings, base_terms, ndocs);
        std::vector<double> bm25_title_idf_ = bm25_idf(postings, title_terms, ndocs);
        std::vector<double> bm25_concat_idf_ = bm25_concat_idf(postings, base_terms, title_terms, ndocs);
	std::vector<double> bm25_body_bigram_idfs = bm25_bigram_idf(postings,body_pair_terms,ndocs,MAX_DIST);
	std::vector<double> bm25_title_bigram_idfs = bm25_bigram_idf(postings,title_pair_terms,ndocs,MAX_DIST);

        std::vector<double>::const_iterator bm25_wbbegin = bm25_body_idf_.begin();
        std::vector<double>::const_iterator bm25_wbend = bm25_body_idf_.end();
        std::vector<double>::const_iterator bm25_wtbegin = bm25_title_idf_.begin();
        std::vector<double>::const_iterator bm25_wtend = bm25_title_idf_.end();
        std::vector<double>::const_iterator bm25_wcbegin = bm25_concat_idf_.begin();
        std::vector<double>::const_iterator bm25_wcend = bm25_concat_idf_.end();

        std::vector<double> tfidf_body_idf_ = tfidf_idf(postings, base_terms, ndocs);
        std::vector<double> tfidf_title_idf_ = tfidf_idf(postings, title_terms, ndocs);
        std::vector<double> tfidf_concat_idf_ = tfidf_concat_idf(postings, base_terms, title_terms, ndocs);
	std::vector<double> tfidf_body_bigram_idfs = tfidf_bigram_idf(postings,body_pair_terms,ndocs,MAX_DIST);
	std::vector<double> tfidf_title_bigram_idfs = tfidf_bigram_idf(postings,title_pair_terms,ndocs,MAX_DIST);

        std::vector<double>::const_iterator tfidf_wbbegin = tfidf_body_idf_.begin();
        std::vector<double>::const_iterator tfidf_wbend = tfidf_body_idf_.end();
        std::vector<double>::const_iterator tfidf_wtbegin = tfidf_title_idf_.begin();
        std::vector<double>::const_iterator tfidf_wtend = tfidf_title_idf_.end();
        std::vector<double>::const_iterator tfidf_wcbegin = tfidf_concat_idf_.begin();
        std::vector<double>::const_iterator tfidf_wcend = tfidf_concat_idf_.end();




        //features to calculate: max, min, mean, gmean

        if (do_features || !weights.empty()) {
          if (!quiet) std::cout << "============BEGIN RANKING FEATURES=============\n";
        }
        
        std::map<uint64_t, double> scores;

        for(std::map<docid_t,std::vector<double> >::const_iterator it = raw_doc_features.begin(); it != raw_doc_features.end(); ++it) {
          std::vector<double> rf;
          rf.push_back(norm_feature(min_feature(it->second.begin() + base_i, it->second.begin() + title_i),zero_floor));
          rf.push_back(norm_feature(max_feature(it->second.begin() + base_i, it->second.begin() + title_i),zero_floor));
          rf.push_back(norm_feature(mean_feature(it->second.begin() + base_i, it->second.begin() + title_i),zero_floor));
          rf.push_back(norm_feature(gmean_feature(it->second.begin() + base_i, it->second.begin() + title_i),zero_floor));

          rf.push_back(norm_feature(min_feature(it->second.begin() + title_i, it->second.begin() + pair_i),zero_floor));
          rf.push_back(norm_feature(max_feature(it->second.begin() + title_i, it->second.begin() + pair_i),zero_floor));
          rf.push_back(norm_feature(mean_feature(it->second.begin() + title_i, it->second.begin() + pair_i),zero_floor));
          rf.push_back(norm_feature(gmean_feature(it->second.begin() + title_i, it->second.begin() + pair_i),zero_floor));

          rf.push_back(norm_feature(min_feature(it->second.begin() + pair_i, it->second.begin() + title_pair_i),zero_floor));
          rf.push_back(norm_feature(max_feature(it->second.begin() + pair_i, it->second.begin() + title_pair_i),zero_floor));
          rf.push_back(norm_feature(mean_feature(it->second.begin() + pair_i, it->second.begin() + title_pair_i),zero_floor));
          rf.push_back(norm_feature(gmean_feature(it->second.begin() + pair_i, it->second.begin() + title_pair_i),zero_floor));

          rf.push_back(norm_feature(min_feature(it->second.begin() + title_pair_i, it->second.begin() + end_i),zero_floor));
          rf.push_back(norm_feature(max_feature(it->second.begin() + title_pair_i, it->second.begin() + end_i),zero_floor));
          rf.push_back(norm_feature(mean_feature(it->second.begin() + title_pair_i, it->second.begin() + end_i),zero_floor));
          rf.push_back(norm_feature(gmean_feature(it->second.begin() + title_pair_i, it->second.begin() + end_i),zero_floor));

          std::vector<double> extra_features;
          square_features(extra_features, it->second.begin() + pair_i, it->second.begin() + title_pair_i); 
          rf.push_back(norm_feature(min_feature(extra_features.begin(), extra_features.end()),zero_floor));
          rf.push_back(norm_feature(max_feature(extra_features.begin(), extra_features.end()),zero_floor));
          rf.push_back(norm_feature(mean_feature(extra_features.begin(), extra_features.end()),zero_floor));
          rf.push_back(norm_feature(gmean_feature(extra_features.begin(), extra_features.end()),zero_floor));

          square_features(extra_features, it->second.begin() + title_pair_i, it->second.begin() + end_i); 
          rf.push_back(norm_feature(min_feature(extra_features.begin(), extra_features.end()),zero_floor));
          rf.push_back(norm_feature(max_feature(extra_features.begin(), extra_features.end()),zero_floor));
          rf.push_back(norm_feature(mean_feature(extra_features.begin(), extra_features.end()),zero_floor));
          rf.push_back(norm_feature(gmean_feature(extra_features.begin(), extra_features.end()),zero_floor));


          double aBl = (double)searcher.get_nbody_words() / (double)ndocs;
          double aTl = (double)searcher.get_ntitle_words() / (double)ndocs;
          int nTerms = base_terms.size();


          rf.push_back(norm_feature(calc_tfidf(
                it->second.begin() + base_i, it->second.begin() + title_i,
                it->second.begin() + title_i, it->second.begin() + pair_i,
                tfidf_wbbegin, tfidf_wbend, tfidf_wtbegin, tfidf_wtend, tfidf_wcbegin, tfidf_wcend,
                (double)searcher.get_doc_body_length(it->first) / aBl, (double)searcher.get_doc_title_length(it->first) / aTl, nTerms, 
                0
          ),zero_floor));
          rf.push_back(norm_feature(calc_tfidf(
                it->second.begin() + base_i, it->second.begin() + title_i,
                it->second.begin() + title_i, it->second.begin() + pair_i,
                tfidf_wbbegin, tfidf_wbend, tfidf_wtbegin, tfidf_wtend, tfidf_wcbegin, tfidf_wcend,
                (double)searcher.get_doc_body_length(it->first) / aBl, (double)searcher.get_doc_title_length(it->first) / aTl, nTerms, 
                1
          ),zero_floor));
          rf.push_back(norm_feature(calc_tfidf(
                it->second.begin() + base_i, it->second.begin() + title_i,
                it->second.begin() + title_i, it->second.begin() + pair_i,
                tfidf_wbbegin, tfidf_wbend, tfidf_wtbegin, tfidf_wtend, tfidf_wcbegin, tfidf_wcend,
                (double)searcher.get_doc_body_length(it->first) / aBl, (double)searcher.get_doc_title_length(it->first) / aTl, nTerms, 
                2
          ),zero_floor));

          rf.push_back(norm_feature(calc_bm25(
                it->second.begin() + base_i, it->second.begin() + title_i,
                it->second.begin() + title_i, it->second.begin() + pair_i,
                bm25_wbbegin, bm25_wbend, bm25_wtbegin, bm25_wtend, bm25_wcbegin, bm25_wcend,
                (double)searcher.get_doc_body_length(it->first) / aBl, (double)searcher.get_doc_title_length(it->first) / aTl, nTerms, 
                0
          ),zero_floor));
          rf.push_back(norm_feature(calc_bm25(
                it->second.begin() + base_i, it->second.begin() + title_i,
                it->second.begin() + title_i, it->second.begin() + pair_i,
                bm25_wbbegin, bm25_wbend, bm25_wtbegin, bm25_wtend, bm25_wcbegin, bm25_wcend,
                (double)searcher.get_doc_body_length(it->first) / aBl, (double)searcher.get_doc_title_length(it->first) / aTl, nTerms, 
                1
          ),zero_floor));
          rf.push_back(norm_feature(calc_bm25(
                it->second.begin() + base_i, it->second.begin() + title_i,
                it->second.begin() + title_i, it->second.begin() + pair_i,
                bm25_wbbegin, bm25_wbend, bm25_wtbegin, bm25_wtend, bm25_wcbegin, bm25_wcend,
                (double)searcher.get_doc_body_length(it->first) / aBl, (double)searcher.get_doc_title_length(it->first) / aTl, nTerms, 
                2
          ),zero_floor));

	  rf.push_back(norm_feature(calc_tfidf(
	        bigram_results_body[it->first].begin(), bigram_results_body[it->first].end(),
		bigram_results_title[it->first].begin(), bigram_results_title[it->first].end(),
		tfidf_body_bigram_idfs.begin(), tfidf_body_bigram_idfs.end(), tfidf_title_bigram_idfs.begin(), tfidf_title_bigram_idfs.end(), tfidf_wcbegin, tfidf_wcend,
		(double)searcher.get_doc_body_length(it->first) / aBl, (double)searcher.get_doc_title_length(it->first) / aTl, nTerms, 
		0
	  ),zero_floor));

	  rf.push_back(norm_feature(calc_tfidf(
		bigram_results_body[it->first].begin(), bigram_results_body[it->first].end(),
		bigram_results_title[it->first].begin(), bigram_results_title[it->first].end(),
		tfidf_body_bigram_idfs.begin(), tfidf_body_bigram_idfs.end(), tfidf_title_bigram_idfs.begin(), tfidf_title_bigram_idfs.end(), tfidf_wcbegin, tfidf_wcend,
		(double)searcher.get_doc_body_length(it->first) / aBl, (double)searcher.get_doc_title_length(it->first) / aTl, nTerms, 
		1
	  ),zero_floor));

	  rf.push_back(norm_feature(calc_bm25(
		bigram_results_body[it->first].begin(), bigram_results_body[it->first].end(),
		bigram_results_title[it->first].begin(), bigram_results_title[it->first].end(),
		bm25_body_bigram_idfs.begin(), bm25_body_bigram_idfs.end(), bm25_title_bigram_idfs.begin(), bm25_title_bigram_idfs.end(), bm25_wcbegin, bm25_wcend,
		(double)searcher.get_doc_body_length(it->first) / aBl, (double)searcher.get_doc_title_length(it->first) / aTl, nTerms, 
		0
	  ),zero_floor));

	  rf.push_back(norm_feature(calc_bm25(
		bigram_results_body[it->first].begin(), bigram_results_body[it->first].end(),
		bigram_results_title[it->first].begin(), bigram_results_title[it->first].end(),
		bm25_body_bigram_idfs.begin(), bm25_body_bigram_idfs.end(), bm25_title_bigram_idfs.begin(), bm25_title_bigram_idfs.end(), bm25_wcbegin, bm25_wcend,
		(double)searcher.get_doc_body_length(it->first) / aBl, (double)searcher.get_doc_title_length(it->first) / aTl, nTerms, 
		1
	  ),zero_floor));


          ranking_features[it->first] = rf;

          double score;
          if (!weights.empty()) {
            if (weights.size() != rf.size()) {
              std::cerr << "weight-feature size mismatch: weights( " << weights.size() << " ), features( " << rf.size() << " )\n";
              exit(-1);
            }
            score = score_document(rf, weights);
            scores.insert(std::pair<docid_t,double>(it->first, score));
          }

          if (do_features && !do_sort) {
            if (parse_qid) std::cout << "qid:" << qid_str;
            else std::cout << "qid:" << qidi;

            if (!weights.empty()) {
              std::cout << " score:" << score;
            }

            for(size_t i = 0; i < rf.size(); ++i) {
              std::cout << " " << i << ":" << rf[i];
            }
            std::cout << " # docid: " << it->first << "\n";
          }

        }

        if (do_sort) {

          std::vector<std::pair<docid_t,std::vector<double> > > ranking_vectors;

          for(std::map<docid_t,std::vector<double> >::const_iterator it = ranking_features.begin(); it != ranking_features.end(); ++it) {
            ranking_vectors.push_back(*it);
          }
	  sort_column_class sortobj;
          sortobj.scores = scores;
	  sortobj.sort_column = sort_column;
	  sortobj.reverse = reverse;
	  std::sort(ranking_vectors.begin(), ranking_vectors.end(), sortobj);
          for(std::vector<std::pair<docid_t,std::vector<double> > >::const_iterator it = ranking_vectors.begin(); it != ranking_vectors.end(); ++it) {
	    if(parse_qid) std::cout << "qid:" << qid_str;
	    else std::cout << "qid:" << qidi;

            if (!weights.empty()) {
              std::cout << " score:" << scores[it->first];
            }

            if (do_features) {
              for(size_t j = 0; j < it->second.size(); ++j) {
                std::cout << " " << j << ":" << it->second.at(j);
              }
            }
            std::cout << " # docid: " << it->first << "\n";
	  }
	}

        if (do_features || !weights.empty()) {
          if (!quiet) std::cout << "============END RANKING FEATURES=============\n";
        }

      }

      qidi++;
    }

    free_stemmer(z);
    return 0;
}

std::vector<std::vector<uint32_t> > range_intersection(std::map<std::string,std::vector<std::pair<docid_t,double> > > feature_vectors, std::vector<std::string> terms, size_t num_base_terms, size_t rstore_chunk_size) {

  std::vector<size_t> ris;
  std::vector<size_t> rcs;
  std::vector<std::vector<uint32_t> > rangeids;
  for(size_t i = 0; i < terms.size(); ++i) {
    ris.push_back(0);
    rcs.push_back(rcount(terms[i],rstore_chunk_size));
    std::vector<uint32_t> t;
    rangeids.push_back(t);
  }

  while (ris[0] < rcs[0]) {
    range_t intersection = rrange(terms[0],ris[0],rstore_chunk_size);
    bool match = true;
    size_t i = 0;
    //TODO: improve the intersection loop
    while (i < num_base_terms) {
      if (ris[i] >= rcs[i]) {
        //std::cerr << "term " << i << " finished. returning\n";
        return rangeids;
      }

      range_t rangei = rrange(terms[i],ris[i],rstore_chunk_size);

      if (range_intersect(intersection,rangei)) {
        //reduce the intersection space and go to the next term
        //std::cerr << "range[" << i << "] (" << rangei.first << "," << rangei.second << ") matches intersection (" << intersection.first << "," << intersection.second << ")\n";
        intersection = range_intersection(intersection,rangei);
        ++i;
      } else if (range_lt(rangei,intersection)) {
        //this term doesn't match, so go to the next range in this term
        //std::cerr << "range[" << i << "] (" << rangei.first << "," << rangei.second << ") goes before intersection (" << intersection.first << "," << intersection.second << ")\n";
        ++(ris[i]);
      } else if (range_lt(intersection,rangei)) {
        //this term has already passed the intersection space. increment any previously scanned terms that don't intersect with i, then reset the scan

        //std::cerr << "range[" << i << "] (" << rangei.first << "," << rangei.second << ") goes after intersection (" << intersection.first << "," << intersection.second << ")\n";
        for(size_t j = 0; j < i; ++j) {
          if (range_lt(rrange(terms[j],ris[j],rstore_chunk_size),rangei)) {
            ++(ris[j]);
          }
        }
        intersection = rrange(terms[0],ris[0],rstore_chunk_size);
        i = 1;
      } else {
        die("range comparison failed");
      }
    }

    if (match) {
      range_t min_range = rrange(terms[0],ris[0],rstore_chunk_size);
      size_t min_i = 0;
      for(size_t i = 0; i < num_base_terms; ++i) {
        if (rangeids[i].empty() || (*rangeids[i].rbegin()) != ris[i]) {
          rangeids[i].push_back(ris[i]);
        }
        if (i > 0 && rrange(terms[i],ris[i],rstore_chunk_size).second < min_range.second) {
          min_i = i;
          min_range = rrange(terms[i],ris[i],rstore_chunk_size);
        }
      }
      ++(ris[min_i]);

      //now go through all the extra terms, and add any that intersect with the intersection area
      for(size_t i = num_base_terms; i < ris.size(); ++i) {
        if (ris[i] < rcs[i]) {
          range_t ri = rrange(terms[i],ris[i],rstore_chunk_size);
          while(range_lt(ri,intersection)) {
            ++(ris[i]);
            if (ris[i] >= rcs[i]) break;
            ri = rrange(terms[i],ris[i],rstore_chunk_size);
          }
          if (ris[i] < rcs[i] && range_intersect(ri,intersection) && (rangeids[i].empty() || (*rangeids[i].rbegin()) != ris[i])) {
            rangeids[i].push_back(ris[i]);
          }
        }
      }
    }
  }

  return rangeids;
}


