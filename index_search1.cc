#include "stemmer.h"
#include "index_searcher.h"
#include "tfidf.h"
#include "bm25.h"
#include <iostream>
#include <set>
#include <cmath>
#include <inttypes.h>

typedef uint64_t docid_t;
typedef uint64_t termid_t;

size_t rstore_chunk_size = 20;
size_t tstore_chunk_size = 64;
size_t xstore_chunk_size = 20;

#define MAX_DIST 10

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
std::vector<std::vector<uint32_t> > range_intersection(std::map<std::string,std::vector<std::pair<docid_t,double> > > feature_vectors, std::vector<std::string> terms, size_t num_base_terms);


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
  if (begin == end) return 0;
  double sum = *begin;
  int count = 1;
  for(++begin; begin != end; ++begin) {
    sum += *begin;
    ++count;
  }
  return sum / count;
}

double gmean_feature(std::vector<double>::const_iterator begin, std::vector<double>::const_iterator end) {
  if (begin == end) return 0;
  double prod = *begin;
  int count = 1;
  for(++begin; begin != end; ++begin) {
    prod *= *begin;
    ++count;
  }
  return std::pow(prod, 1.0 / count);
}

int main(int argc, char *argv[]) {
  z = create_stemmer();

  const char *index_path = "\0";
  bool do_rset = false;
  bool do_ranking = true;
  bool quiet = true;
  bool do_features = true;
  bool show_queries = false;
  bool term_count = false;
  bool parse_qid = false;

  int argi = 1;
  const char *docid_path = NULL;
  const char *qrel_path = NULL;
  char *endptr;
  while (argi < argc) {
    const char *arg = argv[argi++];
    if (!strcmp(arg,"--index")) {
      if (argi >= argc) { std::cerr << "missing option to --index\n"; return 1; }
      index_path = argv[argi++];
    } else if (!strcmp(arg,"--rset")) {
      do_rset = true;
    } else if (!strcmp(arg,"--no-rset")) {
      do_rset = false;
    } else if (!strcmp(arg,"--ranking")) {
      do_ranking = true;
    } else if (!strcmp(arg,"--no-ranking")) {
      do_ranking = false;
    } else if (!strcmp(arg,"--features")) {
      do_features = true;
    } else if (!strcmp(arg,"--no-features")) {
      do_features = false;
    } else if (!strcmp(arg,"--query")) {
      show_queries = true;
    } else if (!strcmp(arg,"--no-query")) {
      show_queries = false;
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
    }  /*else if (!strcmp(arg,"--nDocs")) {
	nDocs = strtol(argv[argi],&endptr,10);
	if (!(*arg != '\0' && *endptr == '\0')) {
	  std::cerr << "bad offset \"" << argv[argi] << "\". must be numeric\n";
	  return 1;
	}
	argi++;
	} */else if (arg[0] == '-') {
      std::cerr << "unknown option '" << arg << "'\n"; return 1;
    } else {
      argi--;
      break;
    }
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
    std::cerr << "failed to open index '" << argv[1] << "'\n";
  }


    size_t linelen = 1024;
    char *line = new char[linelen];
    char *tok;
    std::string strline;
    int qid = 0;
    const char *qid_str = NULL;
    while(getline(std::cin, strline)) {
      if (strline.size() < 1) return 0;
      if (strline.size() >= linelen) {
        delete[] line;
        while (strline.size() >= linelen) {
          linelen *= 1.5;
        }
        line = new char[linelen];
      }
      strcpy(line,strline.c_str());

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

      clean_text(line); //stem and remove non-alphanumerics
      if (show_queries) {
        if (parse_qid) {
          std::cout << "query(qid:" << qid_str << ") raw( " << strline << " ) clean(";
        } else {
          std::cout << "query(qid:" << qid << ") raw( " << strline << " ) clean(";
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
      if (search_fail) { qid++; continue; }

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

      //Now we have all of our raw features

      //first lets do the basic intersection, this tells us which documents we care about
      std::vector<std::vector<std::pair<docid_t,double> >::const_iterator> base_its;
      for(std::vector<std::string>::const_iterator it = base_terms.begin(); it != base_terms.end(); ++it) {
        base_its.push_back(feature_vectors[*it].begin());
      }
      std::string sterm = base_terms[0];
      std::map<docid_t,std::vector<double> > raw_doc_features;
      //std::cout << "docs of interest: ";
      while(base_its[0] != feature_vectors[sterm].end()) {
        bool match = true;
        docid_t doc = base_its[0]->first;
        for(size_t i = 1; i < base_its.size(); ++i) {
          while(base_its[i]->first < doc) ++(base_its[i]);
          if (base_its[i]->first == doc) ++(base_its[i]);
          else { match = false; break; }
        }
        if (match) {
          std::vector<double> temp;
          raw_doc_features.insert(std::pair<docid_t,std::vector<double> >(doc,temp));
          //std::cout << " " << doc;
        }
        base_its[0]++;
      }
      //std::cout << "\n";

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

      if (do_rset) {
        if (!quiet) std::cout << "============BEGIN RSET RESULTS=============\n";
        //finally lets go back and calculate chunking (Rstore effectiveness)
        std::vector<std::vector<uint32_t> > rangeids = range_intersection(feature_vectors, all_terms, base_terms.size());
        for(size_t i = 0; i < rangeids.size(); ++i) {
          std::cout << all_terms[i] << ":" << rangeids[i].size() << "/" << rcount(all_terms[i],rstore_chunk_size) << "\n";
        }
        if (!quiet) std::cout << "============END RSET RESULTS=============\n";
      }

      if (do_features) {
        //features to calculate: max, min, mean, gmean
        std::map<docid_t,std::vector<double> > final_features;
	uint64_t nDocs = searcher.get_ndocs();
	std::vector<uint64_t> relevant;
	for(std::map<docid_t,std::vector<double> >::const_iterator it = raw_doc_features.begin(); it != raw_doc_features.end(); ++it) {
	  relevant.push_back(it->first);
	}
        std::map<uint64_t,double> tfIdfTotal = calculateTfIdf(postings,all_terms,nDocs, relevant);

        for(std::map<docid_t,std::vector<double> >::const_iterator it = raw_doc_features.begin(); it != raw_doc_features.end(); ++it) {
          final_features[it->first].push_back(min_feature(it->second.begin() + base_i, it->second.begin() + title_i));
          final_features[it->first].push_back(max_feature(it->second.begin() + base_i, it->second.begin() + title_i));
          final_features[it->first].push_back(mean_feature(it->second.begin() + base_i, it->second.begin() + title_i));
          final_features[it->first].push_back(gmean_feature(it->second.begin() + base_i, it->second.begin() + title_i));


          final_features[it->first].push_back(min_feature(it->second.begin() + title_i, it->second.begin() + pair_i));
          final_features[it->first].push_back(max_feature(it->second.begin() + title_i, it->second.begin() + pair_i));
          final_features[it->first].push_back(mean_feature(it->second.begin() + title_i, it->second.begin() + pair_i));
          final_features[it->first].push_back(gmean_feature(it->second.begin() + title_i, it->second.begin() + pair_i));

          final_features[it->first].push_back(min_feature(it->second.begin() + pair_i, it->second.begin() + title_pair_i));
          final_features[it->first].push_back(max_feature(it->second.begin() + pair_i, it->second.begin() + title_pair_i));
          final_features[it->first].push_back(mean_feature(it->second.begin() + pair_i, it->second.begin() + title_pair_i));
          final_features[it->first].push_back(gmean_feature(it->second.begin() + pair_i, it->second.begin() + title_pair_i));

          final_features[it->first].push_back(min_feature(it->second.begin() + title_pair_i, it->second.begin() + end_i));
          final_features[it->first].push_back(max_feature(it->second.begin() + title_pair_i, it->second.begin() + end_i));
          final_features[it->first].push_back(mean_feature(it->second.begin() + title_pair_i, it->second.begin() + end_i));
          final_features[it->first].push_back(gmean_feature(it->second.begin() + title_pair_i, it->second.begin() + end_i));

          final_features[it->first].push_back(tfIdfTotal[it->first]);

          //std::cout << it->first;
          //for(std::vector<double>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
          //  std::cout << " " << *it2;
          //}
          //std::cout << "\n";
        }

        if (!quiet) std::cout << "============BEGIN BASIC RANKING FEATURES=============\n";

        for(std::map<docid_t,std::vector<double> >::const_iterator it = final_features.begin(); it != final_features.end(); ++it) {
          //std::cout << it->first;
          //for(size_t i = 0; i < it->second.size(); ++i) {
          //  std::cout << " " << i << ":" << it->second[i];
          //}
          //std::cout << "\n";
          
          if (parse_qid) std::cout << "qid:" << qid_str;
          else std::cout << "qid:" << qid;

          for(size_t i = 0; i < it->second.size(); ++i) {
            std::cout << " " << i << ":" << it->second[i];
          }
          std::cout << " # docid: " << it->first << "\n";
        }
        if (!quiet) std::cout << "============END BASIC RANKING FEATURES=============\n";
      }

      if (do_ranking) {
        //TODO: rank the documents, and print each document with it's score in rank order (from good->bad).
        //TODO: scoring function should look something like this:
        //std::vector<std::pair<uint64_t, double> > calculate_scores(std::map<std::string,std::vector<std::pair<uint64_t,std::vector<uint32_t> > > > postings, std::vector<std::string> terms);
      
        if (!quiet) std::cout << "============BEGIN ADDITIONAL RANKING RESULTS=============\n";
	std::map<uint64_t,double> bodyLengths;
	std::map<uint64_t,double> titleLengths;
	uint64_t nDocs = searcher.get_ndocs();
	std::vector<uint64_t> relevant;
	for(std::map<docid_t,std::vector<double> >::const_iterator it = raw_doc_features.begin(); it != raw_doc_features.end(); ++it) {
	  relevant.push_back(it->first);
	}
	for (std::vector<uint64_t>::iterator it = relevant.begin();it!=relevant.end();it++) {
	  bodyLengths[*it] =(double) searcher.get_doc_body_length(*it);
	  titleLengths[*it] =(double) searcher.get_doc_title_length(*it);
	}
	double avgBodyLength = (double)searcher.get_nbody_words()/nDocs;
	double avgTitleLength = (double)searcher.get_ntitle_words()/nDocs;
	std::vector<std::pair<uint64_t, double> > scores = calculateBM25(postings,title_terms,nDocs,titleLengths, avgTitleLength, relevant);
        for(std::vector<std::pair<uint64_t, double> >::const_iterator it = scores.begin(); it != scores.end(); ++it) {
	  std::cout << it->first << ":" << it->second << "\n";
        }	
        if (!quiet) std::cout << "============END ADDITIONAL RANKING RESULTS=============\n";
      }

      qid++;
    }

    free_stemmer(z);
    return 0;
}

std::vector<std::vector<uint32_t> > range_intersection(std::map<std::string,std::vector<std::pair<docid_t,double> > > feature_vectors, std::vector<std::string> terms, size_t num_base_terms) {

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
        //throw std::out_of_range("range comparison");
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


