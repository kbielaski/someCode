#include "stemmer.h"
#include <iostream>
#include <set>
#include <cmath>
#include <inttypes.h>
#include <string>
#include <vector>


stemmer *z;

std::string remove_spaces(std::string word) {
  int prespaces = 0;
  while(word.at(prespaces) == 32) {
    prespaces++;
  }
  word.erase(word.begin(),word.begin()+prespaces);
  int afterspaces = 0;
  int endCount = word.size()-1;
  while(word.at(endCount) == 32){
    afterspaces++;
    endCount--;
  }
  word.erase(word.end()-afterspaces,word.end());
  return word;
}

inline void clean_text(char *buffer) {
  for(char *p = buffer;*p;++p) {
    char c = *p;
    if (!isalnum(c)) *p = ' '; //TODO: decide whether to include '-'
  }
  stem_str(z,buffer); //also does to-lower, but leaves symbols in-place
}

std::vector<int> getHighlight(std::string input, std::vector<std::string> terms){
  std::vector<int> result;
  char* tok;
  char * sp;
  char cinput[input.length()+1];
  strcpy(cinput, input.c_str());
  std::string tmp = "";
  char * tmp2;
  for(tok = strtok_r(cinput, " ", &sp); tok; tok = strtok_r(NULL, " ", &sp)) {
    tmp = "";
    clean_text(tok);
    tmp += tok;
    tmp = remove_spaces(tmp);
    for(int i = 0; i<terms.size(); i++) {
      if(tmp.compare(terms.at(i)) == 0){
	 int place = tok-cinput;
	 result.push_back(place);
      }
    }
  }
  /*if(result.size() == 0) {
    std::cout << "there are no results\n";
  }
  else{
    for(int i = 0; i<result.size(); i++) {
      std::cout << result.at(i) << std::endl;
    }
    }*/
  return result;
}

std::string doHighlight(std::string input, std::vector<int> indexes) {
  bool isHighlight = false;
  bool hlStart = false;
  int vectorindex = 0;
  std::string result = "";
  if(indexes.size() == 0) {
    return input;
  }
  else{
    for(int i = 0; i<input.length(); i++){
      if (i == indexes.at(vectorindex)) {
	isHighlight = true;
	hlStart = true;
	if(vectorindex < indexes.size() - 1) {
	  vectorindex++;
	}
      }
      if(hlStart) {
	result += "<span style=\"background-color: #FFFF00\">";
	hlStart = false;
      }
      if (isHighlight) {
	if (!isalnum(input.at(i))) {
	  result += "</span>";
	  isHighlight = false;
	}
      }
      result += input.at(i);
      if(i == input.length()-1 && isHighlight) {
	result += "</span>";
	isHighlight = false;
      }
    }
    return result;
  }
}

int main(int argc, char* argv[]){
  std::string result = "";
  std::string strline;
  std::string temp = "";
  std::string temp1 = "";
  std::vector<int> TitleHlIndexes;
  std::vector<int> BodyHlIndexes;
  std::string TitleAdd = "";
  std::string BodyAdd = "";
  std::string terms = argv[1];
  z = create_stemmer();
  char cterms[terms.length()+1];
  strcpy(cterms, terms.c_str());
  clean_text(cterms);
  std::vector<std::string> allTerms;
  char* tok;
  for(tok = strtok(cterms, " "); tok; tok = strtok(NULL, " ")) {
    clean_text(tok);
    std::string temp = "";
    temp += tok;
    temp = remove_spaces(temp);
    allTerms.push_back(temp);
  }
  result += "Metadata: ";
  if(!getline(std::cin,strline)){
    std::cerr << "no line\n";
    return 0;
  }
  if(strline == "" || strline ==  "\n"){
    std::cerr << "empty line\n";
    return 0;
  }
  result += strline;
  result += "<br/>Doc id: ";
  if(!getline(std::cin,strline)){
    std::cerr << "no line\n";
    return 0;
  }
  if(strline == "" || strline ==  "\n"){
    std::cerr << "empty line\n";
    return 0;
  }
  char cstrline[strline.length()+1];
  strcpy(cstrline, strline.c_str());
  tok = strtok(cstrline, "\t");
  result += tok;
  result += "<br/>Title: ";
  tok = strtok(NULL, "\t\n");
  temp = "";
  temp += tok;
  TitleHlIndexes = getHighlight(temp, allTerms);
  TitleAdd = doHighlight(temp, TitleHlIndexes);
  result += TitleAdd;
  result += "<br/>Document body: ";
  tok = strtok(NULL, "\n\t");
  if(tok == NULL){
    result += "there is no body\n";
  }
  else {
    temp1 = "";
    temp1 += tok;
    BodyHlIndexes = getHighlight(temp1, allTerms);
    BodyAdd = doHighlight(temp1, BodyHlIndexes);
    result += BodyAdd;
  }
  std::cout << result << std::flush;
  return 0;
}
