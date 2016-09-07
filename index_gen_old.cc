//index_gen - takes in trec formatted xml (actually sgml), and outputs an index for each document
//output format: <feature> <docno> <value>
//there is a feature for each word with the frequency, and also for each pair of words with a dist <= 10 there is a minimum distance
#include "index_gen.h"
#include "coding.h"

//#define DOPAIRS 1

int main(int argc, char *argv[]) { 
    int input_fd;
    if (argc > 1) {
      int fd = open(argv[1], O_RDONLY);
      if (fd < 0) {
        std::cout << "failed to open corpus" << std::endl;
        exit(-1);
      }
      input_fd = fd;
    } else {
      input_fd = STDIN_FILENO;
    }

    return generate_index(input_fd, true);
}

static std::map<termid_t, std::vector<index_pair> > postings;
static std::map<termid_t, std::string> dictionary;
static std::set<std::string> bigrams;

int generate_index(int input_fd, bool debug) {
  LineCorpusScanner *corpus = new LineCorpusScanner(input_fd);
  DocumentScanner *scanner = corpus->getScanner();
  docid_t docid;
  std::vector<std::string> title_terms;
  std::vector<std::string> body_terms;
#ifdef DOPAIRS
  std::string wordbuffer[MAX_DIST];
#endif


  while (scanner->nextDocument(docid,title_terms,body_terms)) {
    uint32_t pos = 0;
    for(std::vector<std::string>::const_iterator it = title_terms.begin(); it != title_terms.end(); ++it) {
      std::string term = make_title_term(*it);
      termid_t tid = compute_term_id(term);
      postings[tid].push_back(index_pair(docid,pos));

      std::pair<std::map<termid_t, std::string>::iterator,bool> i = dictionary.insert(std::pair<termid_t,std::string>(tid,term));
      if (!i.second) { //element already there
        if (i.first->second != term) {
          std::cerr << "hash collision between '" << term << "' and '" << i.first->second << "'\n";
        }
      }

#ifdef DOPAIRS
      wordbuffer[pos % MAX_DIST] = *it;
      for(uint32_t pairdist = MAX_DIST-1; pairdist > 0; pairdist--) {
        if (pos > pairdist) {
          std::string pair = make_title_pair(wordbuffer[(pos+MAX_DIST-pairdist)%MAX_DIST],wordbuffer[pos%MAX_DIST]);
          bigrams.insert(pair);
        }
      }
#endif
      pos++;
    }
    pos = 0;
    for(std::vector<std::string>::const_iterator it = body_terms.begin(); it != body_terms.end(); ++it) {
      std::string term = *it;
      termid_t tid = compute_term_id(term);
      postings[tid].push_back(index_pair(docid,pos));

      std::pair<std::map<termid_t, std::string>::iterator,bool> i = dictionary.insert(std::pair<termid_t,std::string>(tid,term));
      if (!i.second) { //element already there
        if (i.first->second != term) {
          std::cerr << "hash collision between '" << term << "' and '" << i.first->second << "'\n";
        }
      }
      
#ifdef DOPAIRS
      wordbuffer[pos % MAX_DIST] = term;
      for(uint32_t pairdist = MAX_DIST-1; pairdist > 0; pairdist--) {
        if (pos > pairdist) {
          std::string pair = make_pair(wordbuffer[(pos+MAX_DIST-pairdist)%MAX_DIST],wordbuffer[pos%MAX_DIST]);
          bigrams.insert(pair);
        }
      }
#endif
      pos++;
    }
  }
  std::cerr << "\nfinished scan, dumping index...\n";
  //std::map<termid_t, std::vector<index_pair> >(postings).swap(postings); //resize posting capacity to actual size
  return dump_index("index",postings,dictionary,bigrams);
}

int dump_index(std::string fname, std::map<termid_t, std::vector<index_pair> > &postings, std::map<termid_t, std::string> &dictionary, std::set<std::string> &bigrams) {
  struct file_buffer *fb_dict = (struct file_buffer*)malloc(sizeof(struct file_buffer));
  struct file_buffer *fb_terms = (struct file_buffer*)malloc(sizeof(struct file_buffer));
  struct file_buffer *fb_postings = (struct file_buffer*)malloc(sizeof(struct file_buffer));
#ifdef DOPAIRS
  struct file_buffer *fb_bigrams = (struct file_buffer*)malloc(sizeof(struct file_buffer));
#endif

  std::string dict_path = fname + ".dct";
  std::string terms_path = fname + ".trm";
  std::string postings_path = fname + ".idx";
#ifdef DOPAIRS
  std::string bigrams_path = fname + ".bgm";
#endif

  int fd_dict = open(dict_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  int fd_terms = open(terms_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  int fd_postings = open(postings_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
#ifdef DOPAIRS
  int fd_bigrams = open(bigrams_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
#endif
  if (fd_dict == -1 || fd_terms == -1 || fd_postings == -1) {
    std::cerr << "error opening index files\n";
    return -1;
  }
#ifdef DOPAIRS
  if (fd_bigrams == -1) {
    std::cerr << "error opening index files\n";
    return -1;
  }
#endif
  fbuffer_init(fb_dict, fd_dict, FBUFFER_WRITE);
  fbuffer_init(fb_terms, fd_terms, FBUFFER_WRITE);
  fbuffer_init(fb_postings, fd_postings, FBUFFER_WRITE);
#ifdef DOPAIRS
  fbuffer_init(fb_bigrams, fd_bigrams, FBUFFER_WRITE);

  for(std::set<std::string>::const_iterator it = bigrams.begin(); it != bigrams.end(); ++it) {
    std::string b = *it + "\n";
    fbuffer_write(fb_bigrams, b.c_str(), b.size());
  }
#endif

  termid_t prev_tid = 0;

  std::map<docid_t, std::vector<uint32_t> > posting;
  for(std::map<termid_t, std::vector<index_pair> >::const_iterator it = postings.begin(); it != postings.end(); ++it) {
  //while(!postings.empty()) {
    //std::map<termid_t, std::vector<index_pair> >::iterator it = postings.begin();
    termid_t tid = it->first;
    std::string term = dictionary[tid];
    std::cout << term;
    //std::cout << " " << std::hex << tid << std::dec;

    posting.clear();
    //construct posting sorted by document
    for(std::vector<index_pair>::const_iterator pit = it->second.begin(); pit != it->second.end(); ++pit) {
      docid_t docid = pit->first;
      uint32_t pos = pit->second;
      posting[docid].push_back(pos);
    }
    //postings.erase(it); //free up memory

    //std::map<docid_t, std::vector<uint32_t> >(posting).swap(posting); //resize posting capacity to actual size
    //for(std::map<docid_t, std::vector<uint32_t> >::iterator it = posting.begin(); it != posting.end(); ++it) {
    //  std::vector<uint32_t>(it->second).swap(it->second); //resize each doclist to actual size
    //}


    //write to dictionary
    fbuffer_write_vbyte64(fb_dict,tid-prev_tid); //write delta termid
    fbuffer_write(fb_dict,term.c_str(),strlen(term.c_str())+1); //write term string with null terminator

    //write to term index
    fbuffer_write_uint64(fb_terms,tid); //write termid
    uint64_t pos = fbuffer_get_file_offset(fb_postings);
    fbuffer_write_uint64(fb_terms,pos); //write term position in postings file

    //now we write: 0) #docs 1) doc ids, 2) posting lengths, 3) posting positions
    fbuffer_write_vbyte(fb_postings,posting.size()); //write #docs in posting

    docid_t prev_docid = 0;
    for(std::map<docid_t, std::vector<uint32_t> >::iterator it = posting.begin(); it != posting.end(); ++it) {
      docid_t docid = it->first;
      fbuffer_write_vbyte64(fb_postings,docid - prev_docid); //write delta docid
      prev_docid = docid + 1; //because docids are always 1+ apart
    }
    for(std::map<docid_t, std::vector<uint32_t> >::iterator it = posting.begin(); it != posting.end(); ++it) {
      uint32_t posting_length = it->second.size();
      fbuffer_write_s9(fb_postings,posting_length-1); //write posting length-1, because it can't be zero
    }

    //now iterate through posting by document
    std::cout << " ndocs:" << posting.size();

    std::cout << " docs:";
    for(std::map<docid_t, std::vector<uint32_t> >::const_iterator it = posting.begin(); it != posting.end(); ++it) {
      std::cout << it->first << ",";
    }
    std::cout << " plens:";
    for(std::map<docid_t, std::vector<uint32_t> >::const_iterator it = posting.begin(); it != posting.end(); ++it) {
      std::cout << it->second.size() << ",";
    }
    for(std::map<docid_t, std::vector<uint32_t> >::iterator it = posting.begin(); it != posting.end(); ++it) {
      std::sort(it->second.begin(), it->second.end()); //sort posting by pos
      //docid_t docid = it->first;
      //std::cout << " " << docid << ":";
      std::cout << " p:";
      uint32_t prev_pos = 0;
      for(std::vector<uint32_t>::const_iterator pit = it->second.begin(); pit != it->second.end(); ++pit) {
        std::cout << *pit << ",";
        fbuffer_write_s9(fb_postings,*pit-prev_pos); //write posting postion
        prev_pos = *pit + 1; //because positions are always 1+ apart
      }
    }
    fbuffer_flush_s9(fb_postings);
    std::cout << "\n";
    prev_tid = tid + 1; //because tids are always 1+ apart
  }

  fbuffer_flush(fb_dict);
  fbuffer_flush(fb_terms);
  fbuffer_flush(fb_postings);
#ifdef DOPAIRS
  fbuffer_flush(fb_bigrams);
#endif
  close(fd_dict);
  close(fd_terms);
  close(fd_postings);
#ifdef DOPAIRS
  close(fd_bigrams);
#endif
  return 0;
}
