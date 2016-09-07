//index_gen - takes in trec formatted xml (actually sgml), and outputs an index for each document
//output format: <feature> <docno> <value>
//there is a feature for each word with the frequency, and also for each pair of words with a dist <= 10 there is a minimum distance
#include "bigram_gen.h"
//#include "coding.h"


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

static std::set<termid_t> bigrams;

int generate_index(int input_fd, bool debug) {
  LineCorpusScanner *corpus = new LineCorpusScanner(input_fd);
  DocumentScanner *scanner = corpus->getScanner();
  uint64_t fpos;
  docid_t docid;
  std::string doc_name;
  std::vector<std::string> title_terms;
  std::vector<std::string> body_terms;
  std::string wordbuffer[MAX_DIST];
  std::string fname = "index";

  //struct file_buffer *fb_bigrams = (struct file_buffer*)malloc(sizeof(struct file_buffer));
  std::string bigrams_path = fname + ".bgm";
  //int fd_bigrams = open(bigrams_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  //if (fd_bigrams == -1) {
  //  std::cerr << "error opening index files\n";
  //  return -1;
  //}
  //fbuffer_init(fb_bigrams, fd_bigrams, FBUFFER_WRITE);
  serializer ser_bigrams(fname + ".bgm",serializer::WRITE);

  while (scanner->nextDocument(fpos,docid,doc_name, title_terms,body_terms)) {
    uint32_t pos = 0;
    for(std::vector<std::string>::const_iterator it = title_terms.begin(); it != title_terms.end(); ++it) {
      wordbuffer[pos % MAX_DIST] = *it;
      for(uint32_t pairdist = MAX_DIST-1; pairdist > 0; pairdist--) {
        if (pos > pairdist) {
          std::string pair = make_title_pair(wordbuffer[(pos+MAX_DIST-pairdist)%MAX_DIST],wordbuffer[pos%MAX_DIST]) + "\n";
          if (bigrams.insert(compute_term_id(pair)).second) {
            //fbuffer_write(fb_bigrams, pair.c_str(), pair.size());
            ser_bigrams.write_bytes(pair.c_str(),pair.size());
          }
        }
      }
      pos++;
    }
    pos = 0;
    for(std::vector<std::string>::const_iterator it = body_terms.begin(); it != body_terms.end(); ++it) {
      wordbuffer[pos % MAX_DIST] = *it;
      for(uint32_t pairdist = MAX_DIST-1; pairdist > 0; pairdist--) {
        if (pos > pairdist) {
          std::string pair = make_pair(wordbuffer[(pos+MAX_DIST-pairdist)%MAX_DIST],wordbuffer[pos%MAX_DIST]) + "\n";
          if (bigrams.insert(compute_term_id(pair)).second) {
            //fbuffer_write(fb_bigrams, pair.c_str(), pair.size());
            ser_bigrams.write_bytes(pair.c_str(),pair.size());
          }
        }
      }
      pos++;
    }
  }
  //std::map<termid_t, std::vector<index_pair> >(postings).swap(postings); //resize posting capacity to actual size
  //fbuffer_flush(fb_bigrams);
  //close(fd_bigrams);
  ser_bigrams.flush();
  return 0;
}
