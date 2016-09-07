#ifndef _TFIDF_H_
#define _TFIDF_H_
#include <cmath>
#include <string>
#include <cassert>
#include <set>
#include <vector>
#include <tuple>
#include <stdint.h>
#include <map>
#include <algorithm>
#include <iostream>
#include "bigram_count.h"

typedef std::vector<std::pair<uint64_t,std::vector<uint32_t> > > posting_t;
typedef std::map<std::string,posting_t > posting_map_t;

//calculates the idf scores for each query term as used in tfidf
//  IDF(qi) = log( ndocs / nqi )
std::vector<double> tfidf_idf(std::map<std::string,std::vector<std::pair<uint64_t,std::vector<uint32_t> > > > &postings,
    std::vector<std::string> terms, uint64_t ndocs) {
  std::vector<double> idf;
  for(std::vector<std::string>::const_iterator it = terms.cbegin(); it != terms.cend(); ++it) {
    double nqi = postings[*it].size(); //num docs containing term
    if (nqi == 0) idf.push_back(0);
    else idf.push_back(log(ndocs/nqi));
  }
  return idf;
}

std::vector<double>tfidf_bigram_idf(std::map<std::string,std::vector<std::pair<uint64_t,std::vector<uint32_t> > > > &postings,
				    std::vector<std::string> bigrams, uint64_t ndocs, int maxdist) {
  std::vector<double> idf;
  for(std::vector<std::string>::const_iterator it = bigrams.cbegin(); it != bigrams.cend(); ++it) {
    double nqi = 0;
    std::string term1 = *it;
    std::string term2;
    if (0 == term1.substr(0,4).compare("TTL|")) {
      term1 = term1.substr(4);
    }
    std::size_t found = term1.find("|");
    if (found != std::string::npos) {
      term2 = term1.substr(found+1);
      term1 = term1.substr(0,found);
    }
    int term1_it = 0;
    int term2_it = 0;
    while(term1_it < postings[term1].size() && term2_it < postings[term2].size()) {
      if(postings[term1].at(term1_it).first == postings[term2].at(term2_it).first) {
	if(compute_bigram_bool(maxdist,postings[term1].at(term1_it).second,postings[term2].at(term2_it).second)) {
	    nqi++;
	  }
	term1_it++;
	term2_it++;
      }
      else if(postings[term1].at(term1_it).first < postings[term2].at(term2_it).first) {
	  term1_it++;
      }
      else if(postings[term2].at(term2_it).first < postings[term1].at(term1_it).first) {
        term2_it++;
      }
    }
    if (nqi == 0) idf.push_back(0);
    else idf.push_back(log(ndocs/nqi));
  }
  return idf;
}

//calculates the concat idf scores for each query term as used in tfidf
//  - need to union the postings to get the number of distinct docs that point to a term in the inverted index
//  IDF(qi) = log( ndocs / nqi )
std::vector<double> tfidf_concat_idf(std::map<std::string,std::vector<std::pair<uint64_t,std::vector<uint32_t> > > > &postings,
    std::vector<std::string> terms, std::vector<std::string> terms2, uint64_t ndocs) {
  std::vector<double> idf;
  for(std::vector<std::string>::const_iterator it = terms.cbegin(), it2 = terms2.cbegin(); it != terms.cend(); ++it, ++it2) {
    // -- need union of it list and it2 list
    std::set<uint64_t> docs;
    for(std::vector<std::pair<uint64_t,std::vector<uint32_t> > >::const_iterator it3 = postings[*it].begin(); it3 != postings[*it].end(); ++it3) {
      docs.insert(it3->first);
    }
    for(std::vector<std::pair<uint64_t,std::vector<uint32_t> > >::const_iterator it3 = postings[*it2].begin(); it3 != postings[*it2].end(); ++it3) {
      docs.insert(it3->first);
    }
    double nqi = docs.size(); //num docs containing term
    if (nqi == 0) idf.push_back(0);
    else idf.push_back(log(ndocs/nqi));
  }
  return idf;
}

// tf-idf
//  ndocs - total num docs
//  qi - query term
//  nqi - num docs containing qi
//  fqD - freq of qi in doc D
//  k1 - parameter, typical value between 1.2 and 2.0, we use 1.5
//  b - parameter, typical value is 0.75
//
//  Score(D,Q) = sum for all qi ( IDF(qi) * (1 + log(fqD)) )
//mode: 0 - body
//      1 - title
//      2 - combined (simulate concat of title and body)
double calc_tfidf(
    std::vector<double>::const_iterator bbegin, std::vector<double>::const_iterator bend,
    std::vector<double>::const_iterator tbegin, std::vector<double>::const_iterator tend,
    std::vector<double>::const_iterator wbbegin, std::vector<double>::const_iterator wbend,
    std::vector<double>::const_iterator wtbegin, std::vector<double>::const_iterator wtend,
    std::vector<double>::const_iterator wcbegin, std::vector<double>::const_iterator wcend,
    double nBl, double nTl, int nTerms, int mode) {
  std::vector<double>::const_iterator body_it = bbegin;
  std::vector<double>::const_iterator title_it = tbegin;
  std::vector<double>::const_iterator body_weight_it = wbbegin;
  std::vector<double>::const_iterator title_weight_it = wtbegin;
  std::vector<double>::const_iterator concat_weight_it = wcbegin;

  double score = 0;
  double nDl;
  switch(mode) { //set normalized doc length depending on data we are using
    case 0: nDl = nBl; break;
    case 1: nDl = nTl; break;
    case 2: nDl = nBl + nTl; break;
    default: return -1;
  }

  switch(mode) {
    case 0: //body
      while (body_it != bend) {
        if (body_weight_it == wbend) { return -1; }
        const double idf = *body_weight_it;
        const double fqD = *body_it;
        if (fqD != 0) score += idf * (1 + log(fqD) );

        ++body_it; ++body_weight_it;
      }
      break;
    case 1: //title
      while (title_it != tend) {
        if (title_weight_it == wtend) { return -1; }
        const double idf = *title_weight_it;
        const double fqD = *title_it;
        if (fqD != 0) score += idf * (1 + log(fqD) );

        ++title_it; ++title_weight_it;
      }
      break;
    case 2: //title + body concat
      while (body_it != bend) {
        if (title_it == tend || concat_weight_it == wcend) { return -1; }
        const double idf = *concat_weight_it;
        const double fqD = *body_it + *title_it;
        if (fqD != 0) score += idf * (1 + log(fqD) );

        ++body_it; ++title_it; ++concat_weight_it;
      }
      break;
    default: return -1;
  }
  return score;
  //return std::max(score,0.0);
  //return score/(double)nTerms;
}
#endif
