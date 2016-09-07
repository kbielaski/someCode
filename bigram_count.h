#ifndef _BIGRAM_COUNT_H_
#define _BIGRAM_COUNT_H_
#include <vector>
#include <string>
#include <iostream>
#include <cstdint>
#include <cmath>

struct term_and_pos {
  uint32_t position;
  int term; //is either one or two for term 1 or term 2
};

std::vector<term_and_pos> createCombinedVector(std::vector<uint32_t> term1_pos, std::vector<uint32_t> term2_pos) {
  std::vector<term_and_pos> term1;
  std::vector<term_and_pos> term2;
  std::vector<term_and_pos> result;
  int t1pos = 0;
  int t2pos = 0;
  for(int i = 0; i<term1_pos.size(); i++) {
    term_and_pos temp;
    temp.position = term1_pos.at(i);
    temp.term = 1;
    term1.push_back(temp);
  }
  for(int i = 0; i<term2_pos.size(); i++) {
    term_and_pos temp;
    temp.position = term2_pos.at(i);
    temp.term = 2;
    term2.push_back(temp);
  }
  while(true) {
    if(t1pos == term1.size()) {
      for(int i = t2pos; i<term2.size(); i++) {
	result.push_back(term2.at(i));
      }
      return result;
    }
    else if(t2pos == term2.size()) {
      for(int i = t1pos; i<term1.size(); i++) {
	result.push_back(term1.at(i));
      }
      return result;
    }
    else{
      if(term1.at(t1pos).position < term2.at(t2pos).position) {
	result.push_back(term1.at(t1pos));
	t1pos++;
      }
      else if(term2.at(t2pos).position < term1.at(t1pos).position) {
	result.push_back(term2.at(t2pos));
	t2pos++;
      }
    }
  }
}

int compute_bigram_count(int maxdist, std::vector<uint32_t> term1_pos, std::vector <uint32_t> term2_pos) {
  std::vector<term_and_pos> combinedVector = createCombinedVector(term1_pos,term2_pos);
  int numBigrams = 0;
  for(int i = 1; i < combinedVector.size(); i++) {
    if(combinedVector.at(i-1).term != combinedVector.at(i).term) {
      if(maxdist != -1) {
	if(abs(combinedVector.at(i).position - combinedVector.at(i-1).position) <= maxdist) {
	  numBigrams++;
	}
      }
      else {
	numBigrams++;
      }
    }
  }
  //std::cout << "Number of bigrams: " <<  numBigrams << std::endl;
  return numBigrams;
}

bool compute_bigram_bool(int maxdist, std::vector<uint32_t> term1_pos, std::vector <uint32_t> term2_pos) {
  std::vector<term_and_pos> combinedVector = createCombinedVector(term1_pos,term2_pos);
  for(int i = 1; i < combinedVector.size(); i++) {
    if(combinedVector.at(i-1).term != combinedVector.at(i).term) {
      if(maxdist != -1) {
	if(abs(combinedVector.at(i).position - combinedVector.at(i-1).position) <= maxdist) {
	  return true;
	}
      }
      else {
	return true;
      }
    }
  }
  return false;
}

/* int main() {
   std::vector<uint32_t> t1;
   std::vector<uint32_t> t2;
   t1.push_back(1);
   t1.push_back(11);
   t1.push_back(21);
   t1.push_back(31);
   t2.push_back(6);
   t2.push_back(16);
   t2.push_back(26);
   t2.push_back(36);

   std::vector<term_and_pos> result = createCombinedVector(t1,t2);
   for(int i = 0; i<result.size(); i++) {
     std::cout << result.at(i).position << ": term " << result.at(i).term << std::endl;
   }
   int result1 = compute_bigram_count(5,t1,t2);
   std::cout << result1 << std::endl;
   return 0;
 }*/

#endif
