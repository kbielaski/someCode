#include "index_scanner.h"
#include "helpers.h"
#include <map>
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

uint64_t compute_term_id(std::string term) {
  const char *buf = term.c_str();
  const size_t buflen = term.size();

  return serializer::get_SHA1(buf,buflen);
}

index_scanner::index_scanner(std::string prefix, int max_bigram_dist, bool cache_postings) :
  index(prefix), bigrams_ser(prefix + ".bgm", serializer::READ), finished_terms(false), finished_bigrams(false), cache_postings(cache_postings), max_bigram_dist(max_bigram_dist)
{
}

bool index_scanner::do_lookup(termid_t termid,std::vector<std::pair<docid_t,std::vector<uint32_t> > > &posting) {
  if (cache_postings) {
    std::map<termid_t,std::vector<std::pair<docid_t,std::vector<uint32_t> > > >::const_iterator it = posting_cache.find(termid);
    if (it == posting_cache.end()) {
      return index.lookup_term(termid, posting);
    } else {
      posting = it->second;
      return true;
    }
  } else {
    return index.lookup_term(termid, posting);
  }
}

bool index_scanner::next_posting(std::string &term, bool &primary_term, std::vector<std::pair<docid_t, std::vector<double> > > &posting) {
  if (finished_bigrams) return false;
  else if (!finished_terms) {
    termid_t termid;
    std::vector<std::pair<docid_t,std::vector<uint32_t> > > p;
    finished_terms = !index.next_posting(term, termid, p);
    if (!finished_terms) {
      posting.clear();
      posting.reserve(p.size());
      primary_term = true;
      for(std::vector<std::pair<docid_t,std::vector<uint32_t> > >::const_iterator it = p.begin(); it != p.end(); ++it) {
        std::vector<double> feature;
        feature.push_back(it->second.size());
        posting.push_back(std::pair<docid_t, std::vector<double> >(it->first,feature));
      }
      if (cache_postings) {
        posting_cache.insert(std::pair<termid_t,std::vector<std::pair<docid_t,std::vector<uint32_t> > > >(termid,p));
      }
      return true;
    } else {
      index.reset_scan();
    }
  }
  if (finished_terms) {
    primary_term = false;
    char tbuffer[1024];
    if (bigrams_ser.read_tok(tbuffer, '|', 1024) < 1) {
      return false;
    }
    bool title = false;
    if (0 == strcmp(tbuffer,"TTL")) {
      title = true;
      if (bigrams_ser.read_tok(tbuffer, '|', 1024) < 1) { std::cerr << "bad stuff\n"; return -1; }
    }
    std::string first(tbuffer);
    if (bigrams_ser.read_tok(tbuffer, '\n', 1024) < 1) { std::cerr << "bad stuff\n"; return -1; }
    std::string second(tbuffer);
    docid_t first_id;
    docid_t second_id;
    if (title) {
      first_id = compute_term_id("TTL|" + first);
      second_id = compute_term_id("TTL|" + second);
    } else {
      first_id = compute_term_id(first);
      second_id = compute_term_id(second);
    }
    term = ( title ? "TTL|" : "" ) + first + "|" + second;

    std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting1,posting2;
    if (!do_lookup(first_id, posting1)) {
      std::cerr << "failed to look up first term of " << (title ? "title " : "") << "bigram: " << first << "(" << first_id << ")\n";
      die("bigram lookup failure");
    } else if(!do_lookup(second_id, posting2)) {
      std::cerr << "failed to look up second term of " << (title ? "title " : "") << "bigram: " << second << "(" << second_id << ")\n";
      die("bigram lookup failure");
    }

    std::vector<std::pair<docid_t, uint32_t> > min_dists = intersect_terms(posting1,posting2,max_bigram_dist);
    //std::cout << ( title ? "TTL|" : "" ) << first << "|" << second;

    //for(std::vector<std::pair<docid_t, uint32_t> >::const_iterator it = min_dists.begin(); it != min_dists.end(); ++it) {
    //  std::cout << " " << it->first << ":" << it->second;
    //}
    //std::cout << "\n";
    posting.clear();
    posting.reserve(min_dists.size());
    for(std::vector<std::pair<docid_t, uint32_t> >::const_iterator it = min_dists.begin(); it != min_dists.end(); ++it) {
        std::vector<double> feature;
        feature.push_back( 1.0 / (double)it->second);
        posting.push_back(std::pair<docid_t, std::vector<double> >(it->first,feature));
    }
    return true;
  } else {
    die("invalid logic state");
  }
}

std::vector<std::pair<docid_t, uint32_t> > index_scanner::intersect_terms(std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting1,std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting2, uint32_t distcap) {
  std::vector<std::pair<docid_t,std::vector<uint32_t> > >::const_iterator it1 = posting1.begin();
  std::vector<std::pair<docid_t,std::vector<uint32_t> > >::const_iterator it2 = posting2.begin();
  std::vector<std::pair<docid_t, uint32_t> > ret;
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
        ret.push_back(std::pair<docid_t,uint32_t>(it1->first, dist));
      }
      ++it1;
      ++it2;
    }
  }
  return ret;
}

void init_progress(uint64_t &state) {
  state=1;
  std::cout
    << "         0    10   20   30   40   50   60   70   80   90  100\n"
  //<< "         |----*----|----*----|----*----|----*----|----*----||\n"
    << "Progress:|" << std::flush;
}

void update_progress(uint64_t progress, uint64_t max, uint64_t &state) {
  uint64_t goal = (state*max)/50;
  while(progress >= goal) {
    if (state>=50) {
      std::cout << "||\n" << std::flush;
    } else if (state%10 == 0) {
      std::cout << "|" << std::flush;
    } else if (state%5 == 0) {
      std::cout << "*" << std::flush;
    } else {
      std::cout << "-" << std::flush;
    }
    state++;
    goal = (state*max)/50;
  }
}

bool index_scanner::count_bigram_postings(bool progress, uint64_t &body_bigram_count, uint64_t &title_bigram_count, uint64_t &body_bigram_entry_count, uint64_t &title_bigram_entry_count) {

  uint64_t total_bigrams = 0;
  uint64_t total_terms = index.get_nterms();
  int ret = 0;
  char tbuffer[1024];
  uint64_t count = 0;
  if (!bigrams_ser.seek(0)) {
    die("couldn't seek to start of bigrams\n");
  }

  while((ret = bigrams_ser.read_tok(tbuffer, '\n', 1024)) > 0) {
    total_bigrams++;
  }

  if (!bigrams_ser.seek(0)) {
    die("couldn't reset bigrams\n");
  }

  uint64_t progstate;

  if (cache_postings) {
    index.reset_scan();
    finished_terms = false;
    finished_bigrams = false;
    std::string term;
    bool primary_term;
    std::vector<std::pair<docid_t, std::vector<double> > > posting;
    std::cout << "preloading:\n";
    if (progress) init_progress(progstate);
    while (!finished_terms && next_posting(term,primary_term,posting)) {//just preloading the cache
      ++count;
      if (progress) update_progress(count, total_terms, progstate);
    }
  }

  count = 0;
  std::cout << "counting:\n";
  if (progress) init_progress(progstate);
  while((ret = bigrams_ser.read_tok(tbuffer, '|', 1024)) > 0) {
    ++count;
    if (progress) update_progress(count, total_bigrams, progstate);
    bool title = false;
    if (0 == strcmp(tbuffer,"TTL")) {
      title = true;
      if (bigrams_ser.read_tok(tbuffer, '|', 1024) < 1) { std::cerr << "bad stuff\n"; return false; }
    }
    std::string first(tbuffer);
    if (bigrams_ser.read_tok(tbuffer, '\n', 1024) < 1) { std::cerr << "bad stuff\n"; return false; }
    std::string second(tbuffer);
    docid_t first_id;
    docid_t second_id;
    if (title) {
      first_id = compute_term_id("TTL|" + first);
      second_id = compute_term_id("TTL|" + second);
    } else {
      first_id = compute_term_id(first);
      second_id = compute_term_id(second);
    }
    //std::string term = ( title ? "TTL|" : "" ) + first + "|" + second;

    std::vector<std::pair<docid_t,std::vector<uint32_t> > > posting1,posting2;
    if (!do_lookup(first_id, posting1)) {
      std::cerr << "failed to look up first term of " << (title ? "title " : "") << "bigram: " << first << "(" << first_id << ")\n";
      die("bigram lookup failure");
    } else if(!do_lookup(second_id, posting2)) {
      std::cerr << "failed to look up second term of " << (title ? "title " : "") << "bigram: " << second << "(" << second_id << ")\n";
      die("bigram lookup failure");
    }

    std::vector<std::pair<docid_t, uint32_t> > min_dists = intersect_terms(posting1,posting2,max_bigram_dist);
    //std::cout << ( title ? "TTL|" : "" ) << first << "|" << second;

    //for(std::vector<std::pair<docid_t, uint32_t> >::const_iterator it = min_dists.begin(); it != min_dists.end(); ++it) {
    //  std::cout << " " << it->first << ":" << it->second;
    //}
    //std::cout << "\n";
    if (title) {
      title_bigram_count++;
      title_bigram_entry_count += min_dists.size();
    } else {
      body_bigram_count++;
      body_bigram_entry_count += min_dists.size();
    }
  }
  return ret >= 0;
}
