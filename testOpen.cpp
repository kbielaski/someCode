#include <iostream>
#include <cstdlib>
#include <fcntl.h>
#include <cstring>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include "helpers.h"
#include <errno.h>

int main(){
  //I think that I don't have the correct permissions because I can cat, but not actually open the file, currently 644
  std::string filename = "/silo/datasets/indices/aquaint/index.txt";
  int flags = 0;
  int m = 432;
  printf(filename.c_str());
  printf("\n");
  printf("%d",flags);
  printf(" ");
  printf("%d",m);
  printf("\n");

  int fd = open(filename.c_str(),flags,m);
  printf("error: %s\n", strerror(errno));
  printf("%d", fd);
  printf("\n");

  return 0;
}
