#include "stemmer.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <set>
#include <cmath>
#include <inttypes.h>
#include <cstdio>
#include <memory>
#include <stdexcept>

std::vector<std::string> parser(std::string oneLine, int predet) {
  std::vector<std::string> parsed;
  std::string element = "";
  int state;
  if(predet == 1) {
    state = 1;
  }
  else {
    state = 0;
  }
  int docidHelper = 0;
  int doclabelHelper = 0;
  for(int i = 0; i<oneLine.length(); ++i) {
    if(doclabelHelper == 2) {
      state = 1;
    }
    if (oneLine[i] == ' ') {
	state = 0;
	if(doclabelHelper == 1) {
	  doclabelHelper = 2;
	}
      }
      if(state == 1) {
	element += oneLine[i];
      }
      if(oneLine[i] == ':') {
	state = 1;
      }
      if (state == 0) {
	if (element.length()>0) {
	  parsed.push_back(element);
	  element.clear();
	}
      }
      if (i  == oneLine.length()-1) {
	if(element.length()>0) {
	  parsed.push_back(element);
	  element.clear();
	}
      }
      if(i < oneLine.length()-2 && oneLine[i] == 'o' && oneLine[i+1] == 'c') {
	  docidHelper = 6;
	  doclabelHelper = 1;
	}
      
      if(docidHelper == 1) {
	state = 1;
      }
      if(docidHelper !=0) {
	docidHelper--;
	}
    }

  return parsed;
}

void makeGap( int gap , std::stringstream& ss ) {
	for ( int i = 0 ; i < gap ; i++ ) 
	  ss << " ";
}

/*std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL)
            result += buffer;
    }
    return result;
}*/

std::string GetStdoutFromCommand(std::string cmd) {

  std::string data;
  FILE * stream;
  const int max_buffer = 256;
  char buffer[max_buffer];
  cmd.append(" 2>&1");

  stream = popen(cmd.c_str(), "r");
  if (stream) {
    while (!feof(stream))
      if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
    pclose(stream);
  }
  return data;
}

std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL)
            result += buffer;
    }
    return result;
}

int main(int argc, char* argv[]) {

  std::string strline;
  std::string terms = argv[1];
  std::string dataset = argv[2];
  std::string preset = argv[3];
 int isPreset;
  if(preset == "1") {
    isPreset = 1;
  }
  else {
    isPreset = 0;
    }
  std::vector<std::string> allTerms;
  char cterms[terms.length()+1];
strcpy(cterms,terms.c_str());
char* tok;
for(tok = strtok(cterms, "+"); tok; tok = strtok(NULL, "+")) {
  std::string temp = "";
  temp += tok;
  allTerms.push_back(temp);
 }
 std::stringstream tabletext;
//from this I get each result from the 
while(getline(std::cin,strline)) {
  if(strline.size() < 1) continue;
  std::vector<std::string> nthLine = parser(strline,isPreset);
  for(int i=0; i<nthLine.size(); i++){
    tabletext<< nthLine[i]<< " ";
    }
 }
//from here, I want to return the numbers to the handleButtons, from there I will format them into vectors
 std:: string s = tabletext.str();


 //std::ofstream htmltable( "testtable.html" , std::ios::out | std::ios::trunc ) ;
 std::cout << s;
 //htmltable << s ;
 //htmltable.close( ) ;

 /*
 std::ofstream myfile("output.txt");
 if(myfile.is_open()){
   myfile<< s;
   myfile.close();

   }*/
	return 0 ;
}
  
