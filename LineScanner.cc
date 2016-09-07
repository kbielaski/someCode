#include "LineScanner.h"
#include <cstring>
#include <iostream>


LineCorpusScanner::LineCorpusScanner(int fd) : init(false) {
  //infb_ = (struct file_buffer*)malloc(sizeof(struct file_buffer));
  //fbuffer_init(infb_, fd, FBUFFER_READ);
  ser = new serializer(fd, serializer::READ,BUFLEN);
  docnum_ = 0;
}

DocumentScanner *LineCorpusScanner::getScanner() {
  if(init) return NULL;
  else {
    init = true;
    return new LineDocumentScanner(this);
  }
}
void LineCorpusScanner::freeScanner(DocumentScanner *scanner) {
  delete scanner;
}

LineCorpusScanner::LineDocumentScanner::LineDocumentScanner(LineCorpusScanner *parent) : parent_(parent) {
  linebuf_ = (char*)malloc(BUFLEN);
  z = create_stemmer();
}

//bool LineCorpusScanner::LineDocumentScanner::nextDocument(docid_t &docid) {
//  int len;
//  while(0 == (len = fbuffer_getline(parent_->infb_,linebuf_,BUFLEN-1))) {
//    std::cerr << "empty line\n";
//  }
//  if (len < 0) return false;
//
//  curdoc_ = parent_->docnum_++;
//  state = TITLE_INIT;
//  docid_ = linebuf_;
//  //docid = std::stoul(docid_);
//  docid = curdoc_;
//
//  title_ = strchr(linebuf_,'\t');
//  if (NULL == title_) {
//    std::cerr << "empty doc '" << linebuf_ << "'\n";
//    body_ = NULL;
//    return true;
//  }
//  *title_ = '\0';
//  ++title_;
//  body_ = strchr(title_,'\t');
//  if (NULL == body_) {
//    std::cerr << "empty doc '" << linebuf_ << "'\n";
//    return true;
//  }
//  *body_ = '\0';
//  ++body_;
//
//  stem_str(z,title_);
//  stem_str(z,body_);
//
//  //std::string curDID_name = std::string(docid_);
//  //std::string title_string = std::string(title);
//  //line = line.substr(secondtab+1);
//  return true;
//}
//
//bool LineCorpusScanner::LineDocumentScanner::nextTerm(std::string &term, DocumentScanner::term_type &type) {
//  char *tok;
//  switch(state) {
//    case TITLE_INIT:
//      if (title_ != NULL) {
//        tok = strtok_r(title_," ",&saveptr_);
//        if (tok != NULL) {
//          term = std::string(tok);
//          type = DocumentScanner::TERM_TITLE;
//          state = TITLE;
//          return true;
//        }
//      }
//    case TITLE:
//      if (title_ != NULL) {
//        tok = strtok_r(NULL," ",&saveptr_);
//        if (tok != NULL) {
//          term = std::string(tok);
//          type = DocumentScanner::TERM_TITLE;
//          return true;
//        }
//      }
//      state = BODY_INIT;
//    case BODY_INIT:
//      if (body_ != NULL) {
//        tok = strtok_r(body_," ",&saveptr_);
//        if (tok != NULL) {
//          term = std::string(tok);
//          type = DocumentScanner::TERM_BODY;
//          state = BODY;
//          return true;
//        }
//      }
//      state = END;
//      return false;
//    case BODY:
//      if (body_ != NULL) {
//        tok = strtok_r(NULL," ",&saveptr_);
//        if (tok != NULL) {
//          term = std::string(tok);
//          type = DocumentScanner::TERM_BODY;
//          return true;
//        }
//      }
//      state = END;
//    case END:
//      return false;
//      break;
//  }
//  return false;
//}


uint64_t LineCorpusScanner::LineDocumentScanner::get_file_pos() {
  return parent_->ser->pos();
}

bool LineCorpusScanner::LineDocumentScanner::nextDocument(uint64_t &fpos, docid_t &docid, std::string doc_name, std::vector<std::string> &title_terms, std::vector<std::string> &body_terms) {
  char *tok;
  int len;
  fpos = parent_->ser->pos();
  //while(0 == (len = fbuffer_getline(parent_->infb_,linebuf_,BUFLEN-1))) {
  if(0 == (len = parent_->ser->read_line(linebuf_,BUFLEN-1))) {
    //std::cerr << "empty line\n";
    return false;
  }
  if (len < 0) return false;

  curdoc_ = parent_->docnum_++;
  docid_ = linebuf_;
  //docid = std::stoul(docid_);
  docid = curdoc_;

  title_ = strchr(linebuf_,'\t');
  if (NULL == title_) {
    std::cerr << "empty doc '" << linebuf_ << "'\n";
    body_ = NULL;
    return true;
  }
  *title_ = '\0';
  ++title_;
  body_ = strchr(title_,'\t');
  if (NULL == body_) {
    std::cerr << "empty doc '" << linebuf_ << "'\n";
    return true;
  }
  *body_ = '\0';
  ++body_;

  clean_text(title_);
  clean_text(body_);

  //return true;

  title_terms.clear();
  body_terms.clear();

  if (docid_ != NULL) {
    doc_name = std::string(docid_);
  }

  if (title_ != NULL) {
    tok = strtok_r(title_," ",&saveptr_);
    while(tok) {
      title_terms.push_back(std::string(tok));
      tok = strtok_r(NULL," ",&saveptr_);
    }
  }
  if (body_ != NULL) {
    tok = strtok_r(body_," ",&saveptr_);
    while(tok) {
      body_terms.push_back(std::string(tok));
      tok = strtok_r(NULL," ",&saveptr_);
    }
  }
  return true;
}

LineCorpusScanner::LineDocumentScanner::~LineDocumentScanner() {
  free(linebuf_);
  free_stemmer(z);
}
