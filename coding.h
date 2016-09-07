#ifndef CODING_H
#define CODING_H 1
//various helper functions for writing binary data
#include <openssl/md5.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

//#define MAX_DOCID 32767
//#define MAX_VERSIONS 64
#define BUFLEN ((1 << 20) * 16)
//READSIZE MUST be a power of 2 and <= BUFLEN
//  - reads will always be some multiple of READSIZE
#define READSIZE (1<<20)

#define FBUFFER_WRITE 0
#define FBUFFER_READ 1

struct file_buffer {
    int fd;
    char buffer[BUFLEN];
    int i;
    int s9_i;
    uint32_t s9_buf[29];
    uint64_t offset;
    int rw;
};

namespace coding {
    struct bitset {
        uint32_t *b;
        int nbits;
    };
}


void fbuffer_init(struct file_buffer *fb, const int fd, const int rw);
uint64_t fbuffer_get_file_offset(struct file_buffer *fb);
int fbuffer_seek_file_offset(struct file_buffer *fb, uint64_t offset);
inline void fbuffer_flush(struct file_buffer *fb);
inline void fbuffer_reserve(struct file_buffer *fb, const int size);
inline int fbuffer_refill(struct file_buffer *fb);
inline int fbuffer_ensure(struct file_buffer *fb, const int size);
void fbuffer_write(struct file_buffer *fb, const char *v, const int size);
//int fbuffer_read(struct file_buffer *fb, char *v, int size);
int fbuffer_getline(struct file_buffer *fb, char *v, const int size);
int fbuffer_read_string(struct file_buffer *fb, char *v, const int size);
void fbuffer_write_uint8(struct file_buffer *fb, const uint8_t v);
void fbuffer_write_uint16(struct file_buffer *fb, const uint16_t v);
void fbuffer_write_uint32(struct file_buffer *fb, const uint32_t v);
void fbuffer_write_uint64(struct file_buffer *fb, const uint64_t v);
int fbuffer_read_uint8(struct file_buffer *fb, uint8_t *v);
int fbuffer_read_uint16(struct file_buffer *fb, uint16_t *v);
int fbuffer_read_uint24(struct file_buffer *fb, uint32_t *v);
int fbuffer_read_uint32(struct file_buffer *fb, uint32_t *v);
int fbuffer_read_uint64(struct file_buffer *fb, uint64_t *v);
void fbuffer_write_vbyte(struct file_buffer *fb, uint32_t v);
void fbuffer_write_vbyte64(struct file_buffer *fb, uint64_t v);
int fbuffer_read_vbyte(struct file_buffer *fb, uint32_t *v);
int fbuffer_read_vbyte64(struct file_buffer *fb, uint64_t *v);
void fbuffer_write_s9(struct file_buffer *fb, int v);
void fbuffer_flush_s9(struct file_buffer *fb);
int fbuffer_read_s9list(struct file_buffer *fb, uint32_t *v);
int fbuffer_fill_s9list(struct file_buffer *fb, uint32_t *v);
int fbuffer_read_s9(struct file_buffer *fb, uint32_t *v);

void bitset_clearall(const struct coding::bitset *b);
void bitset_setall(const struct coding::bitset *b);
void bitset_free(struct coding::bitset *b);
void bitset_setbit(const struct coding::bitset *b, int n);
void bitset_clearbit(const struct coding::bitset *b, int n);
void bitset_and(const struct coding::bitset *lhs,const struct coding::bitset *rhs);
void bitset_or(const struct coding::bitset *lhs,const struct coding::bitset *rhs);
void write_bitset_raw(struct file_buffer *fb,struct coding::bitset *b);

uint64_t get_SHA1(const void *buffer, int len);
uint64_t get_MD5(const void *buffer, int len);

//flush everything in the buffer to disk
void fbuffer_flush(struct file_buffer *fb) {
  //fprintf(stderr, "dumping %d chars\n",fb->i);
  //fbuffer_flush_s9(fb); //FIXME: need to handle recursive deadlock
  if (fb->i > 0) {
    if (fb->fd < 0) {
      fprintf(stderr, "Buffer overflow\n");
      exit(-1);
    }
    int len = fb->i;
    int c = 0;
    int w;
    while (c < len) {
      w = write(fb->fd, &fb->buffer[c], len - c);
      if (w < 0) {
        fprintf(stderr, "Failed to flush buffer\n");
        exit(-1);
      }
      c += w;
      if (fb->offset != (uint64_t)-1) fb->offset += w;
    }
    fb->i = 0;
  }
}

//ensure we have space for at least size bytes in the write buffer
void fbuffer_reserve(struct file_buffer *fb, const int size) {
  if (fb->i + size > BUFLEN) {
    return fbuffer_flush(fb);
  }
}

//read as much data as we can (must be a multiple of READSIZE though)
int fbuffer_refill(struct file_buffer *fb) {
  if (fb->i > 0) { //if the buffer isn't completely full
    if (fb->fd < 0) {
      fprintf(stderr, "Filebuffer out of data\n");
      exit(-1);
    }
    int to_read = fb->i & ~(READSIZE-1); //read some multiple of the READSIZE

    memmove(&fb->buffer[fb->i-to_read], &fb->buffer[fb->i], BUFLEN-fb->i); //shift the buffer back far enough for the new data
    fb->i -= to_read;

    int r;
    while (to_read > 0) {
      r = read(fb->fd, &fb->buffer[BUFLEN-to_read], to_read);
      if (r < 0) {
        fprintf(stderr, "Failed to read to buffer\n");
        exit(-1);
      } else if (r == 0) { //we hit the end of the file
        //we ran out of data, so fix the buffer and index
        //  - the end of the read buffer should always be at BUFLEN
        memmove(&fb->buffer[fb->i+to_read], &fb->buffer[fb->i], BUFLEN-fb->i-to_read);
        fb->i += to_read;
        return -1;
      }
      to_read -= r;
      if (fb->offset != (uint64_t)-1) fb->offset += r;
    }
  }
  return 0;
}

//ensure that there are at least size bytes in the read buffer
int fbuffer_ensure(struct file_buffer *fb, const int size) {
  if (BUFLEN - size < fb->i) {
    if (fbuffer_refill(fb)) {
      if (BUFLEN - size < fb->i) {
        //fprintf(stderr, "Failed to ensure enough buffer\n");
        return -1;
      }
    }
  }
  return 0;
}


#endif
