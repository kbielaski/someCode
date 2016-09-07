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
  std::cout <<"hello" <<std::endl;
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
  /* int headgap = 3 ;
  int bodygap = 3 ;
  int tablegap = 6 ;
  int rowgap = 9 ;
  int rowgap2 = 12;
  tabletext << "<html>\n";
  makeGap( headgap , tabletext ) ;
  tabletext << "<head>";
  makeGap( tablegap, tabletext);
  tabletext << "<meta content=\"text/html; charset=ISO-8859-1\" http-equiv=\"content-type\">\n";
  makeGap(headgap,tabletext);
  tabletext << "</head>\n" ;
  makeGap( bodygap , tabletext ) ;
  tabletext << "<body>\n" ;
  makeGap(tablegap, tabletext);
  tabletext << "<center><h1>Results</h1></center>";
  makeGap(tablegap,tabletext);
  tabletext << "<Pre><h2>     Features</h2></Pre>\n";
  makeGap(tablegap,tabletext);
  tabletext << "<table width=\"100%\" border=\"1\" bgcolor=\"#FFFFFF\">\n";
  makeGap(rowgap,tabletext);
  tabletext << "<tr bgcolor=\"#FFFFFF\">\n";
  if(isPreset) {
    makeGap(rowgap2,tabletext);
    tabletext << "<td width=\"3.7%\" align=\"center\">relevant</td>\n";
  }
  makeGap(rowgap2,tabletext);
  tabletext << "<td width=\"3.7%\" align=\"center\">qid</td>\n";
  if(isPreset) {
    makeGap(rowgap2,tabletext);
    tabletext << "<td width=\"3.7%\" align=\"center\">Score</td>\n";
    }*/
  std::vector<std::string> firstLine;
  getline(std::cin,strline);
  firstLine = parser(strline,isPreset);
  int numFeatures;
  if(isPreset) {
    numFeatures = firstLine.size() - 5;
  }
  else {
    numFeatures = firstLine.size() - 2;
  }
  for (int i = 0; i<numFeatures; i++) {
    /*makeGap(rowgap2,tabletext);
      tabletext << "<td width=\"3.7%\" align=\"center\">";*/
    tabletext << std::to_string(i);
    /*tabletext << "</td>\n";*/
  }
  /*makeGap(rowgap2,tabletext);
    tabletext << "<td width=\"3.7%\" align=\"center\">docid:</td>\n";
  if(isPreset) {
    makeGap(rowgap2,tabletext);
    tabletext << "<td width=\"3.7%\" align=\"center\">doc label</td>\n";
  }
  makeGap(rowgap,tabletext);
  tabletext << "</tr>\n";
  makeGap(rowgap,tabletext);*/
  for(int i = 0; i<firstLine.size()-1; i++) {
    /*makeGap(rowgap2,tabletext);
      tabletext << "<td>";*/
    tabletext << firstLine[i]; //here is where I am getting each of the values ****
    //tabletext << "</td>\n";
  }
  /* makeGap(rowgap2,tabletext);
     tabletext << "<td>";*/
  if(!isPreset) {
    tabletext << "<a href=\"/cgi-bin/doc_lookup.sh?query=";
    tabletext << allTerms.at(0);
    for(int i = 1; i < allTerms.size(); i++){
      tabletext << "+" << allTerms.at(i);
    }
    tabletext << "&" << "dataset=" << dataset;
    tabletext << "&";
    tabletext << "docid=" << firstLine[firstLine.size()-1] << "\">" << firstLine[firstLine.size()-1]; //here is where I am getting the docid number ******
    //tabletext << "</a>";
  }
  if(isPreset) {
    tabletext << firstLine[firstLine.size()-1];
  }
  /*tabletext << "</td>\n";
  makeGap(rowgap,tabletext);
  tabletext << "</tr>\n";*/


  //the rest is for precalculated searches
  if(isPreset) {
    //tabletext << "<td colspan=\"39\">";
      std::string command = "/silo/davidwang/SecureSearch/indexer/doc_lookup --index /silo/datasets/indices/" + dataset + "/index --corpus /silo/datasets/lines-" + dataset + ".txt " + firstLine[firstLine.size()-2] + " | /silo/davidwang/SecureSearch/indexer/highlight " + terms;
    std::string body = GetStdoutFromCommand(command);
    tabletext << body;
    /*tabletext << "</td>";
    makeGap(rowgap,tabletext);
    tabletext << "</tr>\n";*/
  }
  int printBody = 0;
  while(getline(std::cin,strline)) {
    if(strline.size() < 1) continue;
    std::vector<std::string> result = parser(strline,isPreset);
    for (int i = 0; i<result.size()-1; i++) {
      /*makeGap(rowgap2,tabletext);
	tabletext << "<td>";*/
      tabletext << result[i];
      //tabletext << "</td>\n";
    }
    /*makeGap(rowgap2,tabletext);
      tabletext << "<td>";*/
    if(!isPreset) {
      tabletext << "<a href=\"/cgi-bin/doc_lookup.sh?query=";
      tabletext << allTerms.at(0);
      for(int i = 1; i < allTerms.size(); i++){
	tabletext << "+" << allTerms.at(i);
      }
      tabletext << "&" << "dataset=" << dataset;
      tabletext << "&";
      tabletext << "docid=" << result[result.size()-1] << "\">" << result[result.size()-1];
      //tabletext << "</a>";
    }
    if(isPreset) {
      tabletext << result[result.size()-1];
    }
    /*tabletext << "</td>\n";
    makeGap(rowgap,tabletext);
    tabletext << "</tr>\n";*/
    if(isPreset == 1 && printBody < 9) {
      //tabletext << "<td colspan=\"39\">";
      std::string command = "/silo/davidwang/SecureSearch/indexer/doc_lookup --index /silo/datasets/indices/" + dataset + "/index --corpus /silo/datasets/lines-" + dataset + ".txt " + result[result.size()-2] + " | /silo/davidwang/SecureSearch/indexer/highlight \"" + terms + "\"";
      char * cstrcommand = new char[command.length()];
      strcpy(cstrcommand,command.c_str());
      //std::string body = GetStdoutFromCommand(command);
      std::string body = exec(cstrcommand);
      tabletext << body;
      /*tabletext << "</td>";
      makeGap(rowgap,tabletext);
      tabletext << "</tr>\n";*/
      printBody++;
    }
  }
  /*makeGap(tablegap,tabletext);
  tabletext << "<table>\n" ;
  makeGap(bodygap,tabletext);
  tabletext << "</body>\n";
  makeGap(bodygap,tabletext);
  tabletext << "</html>";*/
  std:: string s = tabletext.str();
  std::ofstream htmltable( "testtable.html" , std::ios::out | std::ios::trunc ) ;
  std::cout << s;
  htmltable << s ;
  htmltable.close( ) ;
  return 0 ;
}
  
