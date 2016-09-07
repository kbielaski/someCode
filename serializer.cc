#define _FILE_OFFSET_BITS 64 //+
#include <sys/types.h>
#include <unistd.h>
#include "serializer.h"
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include "helpers.h"

//#if __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
//#error fast serialization doesn't support pdp endian
//#endif
//
//#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN
//then use __builtin_bswap16(x),__builtin_bswap32(x),__builtin_bswap64(x)
//OR #include <byteswap.h>, then bswap_16(x),bswap_32(x),bswap_64(x)
//#else if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
// use x
//#else
// use mask and shift to get little endian output
//#endif

//write least sig byte of v to buf, then shift v right 1 byte
#define SHIFTWRITEBYTE(v) { buf[bufi++] = (uint8_t)((v) & 0xff); v >>= 8; }
//write least sig byte of v to buf
#define WRITEBYTE(v) { buf[bufi++] = (uint8_t)((v) & 0xff); }

#define CODING_DEBUG 1

const int serializer::s9_field_length[9] = { 1,  2, 3, 4, 5, 7, 9, 14, 28 };
const int serializer::s9_field_count[9] = { 28, 14, 9, 7, 5, 4, 3,  2,  1 };
const uint32_t serializer::s9_field_max[9] = { 1, (1<<2)-1, (1<<3)-1, (1<<4)-1, (1<<5)-1, (1<<7)-1, (1<<9)-1, (1<<14)-1, (1<<28)-1 };

serializer::serializer(int fd, enum ser_mode mode, size_t bufcap, size_t watermark, size_t alignment, bool canresize)
  : fd(fd), mode(mode), bufcap(bufcap), bufi(0), buflen(0), alignment(alignment), watermark(watermark), canresize(canresize), s9_i(0), s9_len(0)
{
  if (mode != READ && mode != WRITE && mode != READWRITE) {
    die("bad mode");
  }
  if (fd != -1) {
    off_t off = lseek( fd, 0, SEEK_CUR );
    if (off == (off_t)-1) {
      offset = (size_t)-1; //just in case bit widths aren't the same
    } else {
      offset = (size_t)off;
    }
  } else {
    offset = (off_t)-1;
  }
  if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
  buf = (char *)malloc(bufcap);
  if (!buf) {
    die("no alloc");
  }
}

serializer::serializer(std::string filename, enum ser_mode mode, size_t bufcap, size_t watermark, size_t alignment, bool canresize)
  : mode(mode),bufcap(bufcap), bufi(0), buflen(0), offset(0), alignment(alignment), watermark(watermark), canresize(canresize), s9_i(0), s9_len(0)
{
  int flags = 0;
  int m = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
  if (mode == READ) {
    flags |= O_RDONLY;
  } else if (mode == WRITE) {
    flags |= O_WRONLY | O_TRUNC | O_CREAT;
  } else {
    flags |= O_RDWR | O_CREAT;
  }

  //printf(filename.c_str()); //+
  //printf("\n"); //+
  //printf("%d",flags); //+
  //printf(" "); //+
  //printf("%d",m); //+
  //printf("\n"); //+
 
  fd = open(filename.c_str(),flags,m);  //http://pubs.opengroup.org/onlinepubs/7908799/xsh/open.html
  //printf("%d", fd); //+
  //printf("\n");
  //printf("Error: %s\n", strerror(errno)); //+

  if (fd == -1) {
    dief("couldn't open file: %s\n",filename.c_str());
  }
  if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
  buf = (char *)malloc(bufcap);
  if (!buf) {
    die("no alloc");
  }
}

serializer::serializer(char *buf, size_t buflen, size_t bufcap, bool canresize)
  : mode(READWRITE), buf(buf), bufcap(bufcap), bufi(0), buflen(buflen), offset(-1), alignment(0), watermark(0), canresize(canresize), s9_i(0), s9_len(0)
{
  if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
}

serializer::~serializer() {
  free(buf);
  if (fd != -1) close(fd);
}

bool serializer::flush() {
  if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
  if (!can_write()) return false;

  if (bufi == 0) return false; //buffer completely empty

  int w;
  int i = 0;
  while (bufi > 0) {
    w = write(fd, &buf[i], bufi);
    if (w <= 0) {
      die("Failed to write to fd\n");
    }
    bufi -= w;
    i += w;
    if (offset != (size_t)-1) offset += w;
  }
  buflen = 0;
  return true;
}

bool serializer::aligned_flush() {
  if (!can_write()) return false;

  if (bufi == 0) return false; //buffer completely full
  size_t to_write = bufi;
  size_t to_keep = 0;

  if (alignment) {
    to_keep = to_write % alignment;
    to_write -= to_keep;
  }

  if (to_write == 0) {
      die("empty write, when space required\n");
  }

  int w;
  int i = 0;
  while (to_write > 0) {
    w = write(fd, &buf[i], to_write);
    if (w < 0) {
      die("Failed to write to fd\n");
    }
    to_write -= w;
    i += w;
    if (offset != (size_t)-1) offset += w;
  }
  if (to_keep > 0) {
    memmove(&buf[0], &buf[bufi-to_keep], to_keep); //slide any remaining data back to the start of the buffer
    buflen = bufi = to_keep;
  } else {
    buflen = bufi = 0;
  }
  return true;
}

//refill buffer, but only using aligned reads (if alignment enabled)
bool serializer::aligned_refill() {
  if (!can_read()) return false;

  if (bufi == 0 && buflen == bufcap) return false; //buffer completely full
  size_t data_left = buflen - bufi;
  //size_t to_read = (bufcap - data_left) & ~(alignment-1); //only works if alignment is power of 2
  size_t to_read = bufcap - data_left;

  if (alignment) to_read -= to_read % alignment;

  memmove(&buf[0], &buf[bufi], data_left); //slide any remaining data back to the start of the buffer
  bufi = 0;
  buflen = data_left;
  if (to_read == 0) {
      die("no space in buffer to fulfill read (read requested, but not enough space available)\n");
  }

  int r;
  while (to_read > 0) {
    r = read(fd, &buf[buflen], to_read);
    if (r < 0) {
      die("Failed to read to buffer\n");
    } else if (r == 0) { //we hit the end of the file
      return buflen > data_left; //return true if we read at least one new byte
    }
    to_read -= r;
    buflen += r;
    if (offset != (size_t)-1) offset += r;
  }
  return true;
}

bool serializer::put_back(size_t size) {
  if (bufi >= size) { //TODO: check this
    bufi -= size;
    return true;
  } else {
    return false;
  }
}


//TODO: still needs testing
bool serializer::seek(size_t offset) {
  if (fd == -1) {
    if (offset > bufcap) return false;
    bufi = offset;
    s9_i = s9_len = 0;
    return true;
  } else {
    if (can_write()) return false;
    else {
      if (offset < this->offset && offset > (this->offset - buflen)) {
        bufi = buflen - (this->offset - offset);
        s9_i = s9_len = 0;
        return true;
      } else {
        buflen = bufi = 0;
        s9_i = s9_len = 0;
        this->offset = offset;
        off_t off = lseek( fd, offset, SEEK_SET );
        if (off == (off_t)-1) {
          offset = (size_t)-1; //just in case bit widths aren't the same
          return false;
        } else {
          return true;
        }
      }
    }
    return false;
  }
}

size_t serializer::pos() {
  if (fd == -1) {
    return bufi;
  } else {
    if (can_write()) { //FIXME: this isn't quite correct, but it should work for testing
      return offset + bufi;
    } else if (can_read()) {
      return offset - (buflen - bufi);
    } else {
      return (size_t)-1;
    }
  }
}

bool serializer::reserve_read(size_t size) {
  if (!can_read()) return false;

  if (size > bufcap) { //request too large
    if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
    if (canresize) {
      while (size > bufcap) {
        bufcap = (bufcap * 3) / 2;
      }
      char *t = (char*)realloc(buf,bufcap);
      if (t == NULL) {
        die("bad realloc");
      }
      buf = t;
    } else {
      return false;
    }
  }

  //if we have passed the watermark, refill before doing anything else
  if (watermark != (size_t)-1 && (buflen - bufi) < watermark) {
    aligned_refill();
  }

  if (bufi + size <= buflen) return true; //already have the bytes
  else if (aligned_refill()) { //need to refill first
    return (bufi + size <= buflen);
  } else {
    return false;
  }
}

bool serializer::reserve_write(size_t size) {
  if (!can_write()) return false; //can't write at all
  
  if (size > bufcap) { //request too large
    if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
    if (canresize) {
      while (size > bufcap) {
        bufcap = (bufcap * 3) / 2;
      }
      char *t = (char*)realloc(buf,bufcap);
      if (t == NULL) {
        die("bad realloc");
      }
    } else {
      return false;
    }
  }

  //if we have passed the watermark, flush before doing anything else
  if (buflen > watermark) {
    aligned_flush();
  }

  if (canresize || bufi + size <= bufcap) return true; //already have space in the buffer (or can push past end)
  else if (aligned_flush()) { //need to flush first
    return (bufi + size <= bufcap); //return whether we have space in the buffer
  } else {
    return false;
  }
}

//return value is actual size of read/write if operation fails part way through
int serializer::read_bytes(char *buffer, size_t size) {
  //TODO: optimize for large read (e.g. bypass buffer)
  if (reserve_read(size)) {
    std::copy(buf + bufi, buf + bufi + size,buffer);
    bufi+=size;
  } else { //FIXME:need to handle parital read
    return -1;
  }
  return -1;
}

int serializer::write_bytes(const char *buffer, size_t size) {
  //TODO: optimize for large writes (e.g. bypass buffer)
  if (reserve_write(size)) {
    if (size==0) return true;
    //std::copy(buffer, buffer+size, buf + bufi);
    memcpy(buf+bufi,buffer,size);
    bufi+=size;
    return size;
  } else { //FIXME:need to handle parital write
    return -1;
  }
}

int serializer::write_string(const char *str) {
  size_t size = strlen(str) + 1; //+1 for '\0'
  //TODO: optimize for large writes (e.g. bypass buffer)
  if (reserve_write(size)) {
    if (size==0) return true;
    //std::copy(buffer, buffer+size, buf + bufi);
    memcpy(buf+bufi,str,size);
    bufi+=size;
    return size;
  } else { //FIXME:need to handle parital write
    return -1;
  }
}

int serializer::read_tok(char *buffer, char d, const size_t &max_size) {
  char *start = &buf[bufi];
  size_t len;
  if (buflen-bufi < max_size) { len = buflen-bufi; }
  else { len = max_size; }
  char *delim = (char*)memchr(start,d,len);
  if (NULL == delim) {
    aligned_refill();
    start = &buf[bufi];
    if (buflen-bufi < max_size) { len = buflen-bufi; }
    else { len = max_size; }
    delim = (char*)memchr(start,d,buflen-bufi);
  }
  if (NULL == delim) {
    if (len == 0) {
      return 0;
    } else {
      dief("no delim found after %lu characters\n",len);
    }
  //} else if (len > max_size) {
  //  len=delim-start;
  //  fprintf(stderr, "user buffer too small for %lu characters\n",len);
  //  return -1;
  } else {
    len=delim-start;
    if (len >= max_size) {
      dief("broke max length of %lu with %lu chars\n",max_size,len);
    }
    memcpy(buffer, start, len);
    buffer[len] = '\0';
    bufi += len+1;
    return len;
  }
}

int serializer::read_string(char *buffer, const size_t &max_size) {
  char *start = &buf[bufi];
  size_t len=buflen-bufi;
  if (len > max_size) { len = max_size; }
  char *delim = (char*)memchr(start,'\0',len);
  if (NULL == delim) {
    aligned_refill();
    start = &buf[bufi];
    len=buflen-bufi;
    if (len > max_size) { len = max_size; }
    delim = (char*)memchr(start,'\0',len);
  }
  if (NULL == delim) {
    if (len == 0) {
      return -1; //to differentiate between zero length string and empty read
    } else {
      dief("no delim found after %lu characters\n",len);
    }
  //} else if (len > max_size) {
  //  len=delim-start;
  //  fprintf(stderr, "user buffer too small for %lu characters\n",len);
  //  return -1;
  } else {
    len=delim-start;
    memcpy(buffer, start, len+1);
    bufi += len+1;
    return len;
  }
}


//note: these could all be overloads, but with non-self-describing serialization it is better to be explicit
//return value is whether operation was successful
bool serializer::read_uint8(uint8_t &v) {
  if (!reserve_read(sizeof(v))) return false;
  v = buf[bufi++];
  return true;
}

bool serializer::read_uint16(uint16_t &v) {
  if (!reserve_read(sizeof(v))) return false;
  v = (uint16_t)((uint16_t)buf[bufi]) |
      ((uint16_t)((uint16_t)buf[bufi+1]) << 8);
  bufi += sizeof(v);
  return true;
}

bool serializer::read_uint24(uint32_t &v) {
  if (!reserve_read(3)) return false;
  v = (uint32_t)((uint8_t)buf[bufi]) |
      ((uint32_t)((uint8_t)buf[bufi+1]) << 8) |
      ((uint32_t)((uint8_t)buf[bufi+2]) << 16);
  bufi += 3;
  return true;
}

bool serializer::read_uint32(uint32_t &v) {
  if (!reserve_read(sizeof(v))) return false;
  v = (uint32_t)((uint8_t)buf[bufi]) |
      ((uint32_t)((uint8_t)buf[bufi+1]) << 8) |
      ((uint32_t)((uint8_t)buf[bufi+2]) << 16) |
      ((uint32_t)((uint8_t)buf[bufi+3]) << 24);
  bufi += sizeof(v);
  return true;
}

bool serializer::read_uint64(uint64_t &v) {
  if (!reserve_read(sizeof(v))) return false;
  v = (uint64_t)((uint8_t)buf[bufi]) |
      ((uint64_t)((uint8_t)buf[bufi+1]) << 8) |
      ((uint64_t)((uint8_t)buf[bufi+2]) << 16) |
      ((uint64_t)((uint8_t)buf[bufi+3]) << 24) |
      ((uint64_t)((uint8_t)buf[bufi+4]) << 32) |
      ((uint64_t)((uint8_t)buf[bufi+5]) << 40) |
      ((uint64_t)((uint8_t)buf[bufi+6]) << 48) |
      ((uint64_t)((uint8_t)buf[bufi+7]) << 56);
  bufi += sizeof(v);
  return true;
}


bool serializer::write_uint8(uint8_t v) {
  if (!reserve_write(sizeof(v))) return false;
  WRITEBYTE(v);
  buflen += sizeof(v);
  return true;
}

bool serializer::write_uint16(uint16_t v) {
  if (!reserve_write(sizeof(v))) return false;
  SHIFTWRITEBYTE(v);
  WRITEBYTE(v);
  buflen += sizeof(v);
  return true;
}

bool serializer::write_uint24(uint32_t v) {
  if (!reserve_read(3)) return false;
  SHIFTWRITEBYTE(v);
  SHIFTWRITEBYTE(v);
  WRITEBYTE(v);
  buflen += 3;
  return true;
}

bool serializer::write_uint32(uint32_t v) {
  if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
  if (!reserve_write(sizeof(v))) return false;
  if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
  SHIFTWRITEBYTE(v);
  SHIFTWRITEBYTE(v);
  SHIFTWRITEBYTE(v);
  WRITEBYTE(v);
  buflen += sizeof(v);
  return true;
}

bool serializer::write_uint64(uint64_t v) {
  if (!reserve_write(sizeof(v))) return false;
  SHIFTWRITEBYTE(v);
  SHIFTWRITEBYTE(v);
  SHIFTWRITEBYTE(v);
  SHIFTWRITEBYTE(v);
  SHIFTWRITEBYTE(v);
  SHIFTWRITEBYTE(v);
  SHIFTWRITEBYTE(v);
  WRITEBYTE(v);
  buflen += sizeof(v);
  return true;
}

bool serializer::read_vbyte32(uint32_t &v) {
  uint8_t t;
  if (!read_uint8(t)) return false;
  v = (uint32_t)(t&127);
  if (t & 0x80) {
    if (!read_uint8(t)) return false;
    v |= (uint32_t)(t&127) << 7;
    if (t & 0x80) {
      if (!read_uint8(t)) return false;
      v |= (uint32_t)(t&127) << 14;
      if (t & 0x80) {
        if (!read_uint8(t)) return false;
        v |= (uint32_t)(t&127) << 21;
        if (t & 0x80) {
          if (!read_uint8(t)) return false;
          v |= (uint32_t)(t&127) << 28;
        }
      }
    }
  }
  return true;
}

bool serializer::write_vbyte32(uint32_t v) {
  if (v < (1<<7)) {
    if (!reserve_write(1)) return false;
    WRITEBYTE(v);
    buflen += 1;
  } else if (v < ((uint32_t)1<<14)) {
    if (!reserve_write(2)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 2;
  } else if (v < ((uint32_t)1<<21)) {
    if (!reserve_write(3)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 3;
  } else if (v < ((uint32_t)1<<28)) {
    if (!reserve_write(4)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 4;
  } else {
    if (!reserve_write(5)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 5;
  }
  return true;
}

bool serializer::read_vbyte64(uint64_t &v) {
  uint8_t t;
  if (!read_uint8(t)) return false;
  v = (uint64_t)(t&127);
  if (t & 0x80) {
    if (!read_uint8(t)) return false;
    v |= (uint64_t)(t&127) << 7;
    if (t & 0x80) {
      if (!read_uint8(t)) return false;
      v |= (uint64_t)(t&127) << 14;
      if (t & 0x80) {
        if (!read_uint8(t)) return false;
        v |= (uint64_t)(t&127) << 21;
        if (t & 0x80) {
          if (!read_uint8(t)) return false;
          v |= (uint64_t)(t&127) << 28;
          if (t & 0x80) {
            if (!read_uint8(t)) return false;
            v |= (uint64_t)(t&127) << 35;
            if (t & 0x80) {
              if (!read_uint8(t)) return false;
              v |= (uint64_t)(t&127) << 42;
              if (t & 0x80) {
                if (!read_uint8(t)) return false;
                v |= (uint64_t)(t&127) << 49;
                if (t & 0x80) {
                  if (!read_uint8(t)) return false;
                  v |= (uint64_t)(t&127) << 56;
                }
              }
            }
          }
        }
      }
    }
  }
  return true;
}

bool serializer::write_vbyte64(uint64_t v) {
  if (v < (1<<7)) {
    if (!reserve_write(1)) return false;
    WRITEBYTE(v);
    buflen += 1;
  } else if (v < ((uint64_t)1<<14)) {
    if (!reserve_write(2)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 2;
  } else if (v < ((uint64_t)1<<21)) {
    if (!reserve_write(3)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 3;
  } else if (v < ((uint64_t)1<<28)) {
    if (!reserve_write(4)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 4;
  } else if (v < ((uint64_t)1<<35)) {
    if (!reserve_write(5)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 5;
  } else if (v < ((uint64_t)1<<42)) {
    if (!reserve_write(6)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 6;
  } else if (v < ((uint64_t)1<<49)) {
    if (!reserve_write(7)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 7;
  } else if (v < ((uint64_t)1<<56)) {
    if (!reserve_write(8)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 8;
  } else {
    if (!reserve_write(9)) return false;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v|128); v >>= 7;
    WRITEBYTE(v);
    buflen += 9;
  }
  return true;
}

int serializer::decode_s9(uint32_t *v, uint32_t val) {
  switch (val & 0x0f) {
    case 0:
      v[0] = (val >> 4) & 1;
      v[1] = (val >> 5) & 1;
      v[2] = (val >> 6) & 1;
      v[3] = (val >> 7) & 1;
      v[4] = (val >> 8) & 1;
      v[5] = (val >> 9) & 1;
      v[6] = (val >> 10) & 1;
      v[7] = (val >> 11) & 1;
      v[8] = (val >> 12) & 1;
      v[9] = (val >> 13) & 1;
      v[10] = (val >> 14) & 1;
      v[11] = (val >> 15) & 1;
      v[12] = (val >> 16) & 1;
      v[13] = (val >> 17) & 1;
      v[14] = (val >> 18) & 1;
      v[15] = (val >> 19) & 1;
      v[16] = (val >> 20) & 1;
      v[17] = (val >> 21) & 1;
      v[18] = (val >> 22) & 1;
      v[19] = (val >> 23) & 1;
      v[20] = (val >> 24) & 1;
      v[21] = (val >> 25) & 1;
      v[22] = (val >> 26) & 1;
      v[23] = (val >> 27) & 1;
      v[24] = (val >> 28) & 1;
      v[25] = (val >> 29) & 1;
      v[26] = (val >> 30) & 1;
      v[27] = (val >> 31) & 1;
      return 28;
    case 1:
      v[0] = (val >> 4) & 3;
      v[1] = (val >> 6) & 3;
      v[2] = (val >> 8) & 3;
      v[3] = (val >> 10) & 3;
      v[4] = (val >> 12) & 3;
      v[5] = (val >> 14) & 3;
      v[6] = (val >> 16) & 3;
      v[7] = (val >> 18) & 3;
      v[8] = (val >> 20) & 3;
      v[9] = (val >> 22) & 3;
      v[10] = (val >> 24) & 3;
      v[11] = (val >> 26) & 3;
      v[12] = (val >> 28) & 3;
      v[13] = (val >> 30) & 3;
      //v[14] = (uint32_t)1 << 31; //stop flag
      return 14;
    case 2:
      v[0] = (val >> 4) & 7;
      v[1] = (val >> 7) & 7;
      v[2] = (val >> 10) & 7;
      v[3] = (val >> 13) & 7;
      v[4] = (val >> 16) & 7;
      v[5] = (val >> 19) & 7;
      v[6] = (val >> 22) & 7;
      v[7] = (val >> 25) & 7;
      v[8] = (val >> 28) & 7;
      return 9;
    case 3:
      v[0] = (val >> 4) & 15;
      v[1] = (val >> 8) & 15;
      v[2] = (val >> 12) & 15;
      v[3] = (val >> 16) & 15;
      v[4] = (val >> 20) & 15;
      v[5] = (val >> 24) & 15;
      v[6] = (val >> 28) & 15;
      return 7;
    case 4:
      v[0] = (val >> 4) & ((1<<5)-1);
      v[1] = (val >> 9) & ((1<<5)-1);
      v[2] = (val >> 14) & ((1<<5)-1);
      v[3] = (val >> 19) & ((1<<5)-1);
      v[4] = (val >> 24) & ((1<<5)-1);
      return 5;
    case 5:
      v[0] = (val >> 4) & ((1<<7)-1);
      v[1] = (val >> 11) & ((1<<7)-1);
      v[2] = (val >> 18) & ((1<<7)-1);
      v[3] = (val >> 25) & ((1<<7)-1);
      return 4;
    case 6:
      v[0] = (val >> 4) & ((1<<9)-1);
      v[1] = (val >> 13) & ((1<<9)-1);
      v[2] = (val >> 22) & ((1<<9)-1);
      return 3;
    case 7:
      v[0] = (val >> 4) & ((1<<14)-1);
      v[1] = (val >> 18) & ((1<<14)-1);
      return 2;
    case 8:
      v[0] = (val >> 4);
      return 1;
    default:
      return -(val & 0x0f); //flag value
  }
}

uint32_t serializer::pack_s9(const uint32_t *s9_buf, uint32_t code) {
  //int i;
  //for(i=0;i<s9_field_count[code];++i) {
  //  if (s9_buf[i] > s9_field_max[code]) {
  //    dief("Simple9 value( %u ) overflow for code( %d )\n",s9_buf[i],code);
  //  }
  //}
  switch(code) {
    case 0:
      return (s9_buf[0] << 4)   | (s9_buf[1] << 5)   | (s9_buf[2] << 6)   | (s9_buf[3] << 7)   |
            (s9_buf[4] << 8)   | (s9_buf[5] << 9)   | (s9_buf[6] << 10)  | (s9_buf[7] << 11)  |
            (s9_buf[8] << 12)  | (s9_buf[9] << 13)  | (s9_buf[10] << 14) | (s9_buf[11] << 15) |
            (s9_buf[12] << 16) | (s9_buf[13] << 17) | (s9_buf[14] << 18) | (s9_buf[15] << 19) |
            (s9_buf[16] << 20) | (s9_buf[17] << 21) | (s9_buf[18] << 22) | (s9_buf[19] << 23) |
            (s9_buf[20] << 24) | (s9_buf[21] << 25) | (s9_buf[22] << 26) | (s9_buf[23] << 27) |
            (s9_buf[24] << 28) | (s9_buf[25] << 29) | (s9_buf[26] << 30) | (s9_buf[27] << 31);
      break;
    case 1:
      return 1 | (s9_buf[0] << 4)   | (s9_buf[1] << 6)  | (s9_buf[2] << 8)   | (s9_buf[3] << 10) |
                (s9_buf[4] << 12)  | (s9_buf[5] << 14) | (s9_buf[6] << 16)  | (s9_buf[7] << 18) |
                (s9_buf[8] << 20)  | (s9_buf[9] << 22) | (s9_buf[10] << 24) | (s9_buf[11] << 26)|
                (s9_buf[12] << 28) | (s9_buf[13] << 30);
      break;
    case 2:
      return 2 | (s9_buf[0] << 4)  | (s9_buf[1] << 7)  | (s9_buf[2] << 10) | (s9_buf[3] << 13) |
                (s9_buf[4] << 16) | (s9_buf[5] << 19) | (s9_buf[6] << 22) | (s9_buf[7] << 25) |
                (s9_buf[8] << 28);
      break;
    case 3:
      return 3 | (s9_buf[0] << 4)  | (s9_buf[1] << 8)  | (s9_buf[2] << 12) | (s9_buf[3] << 16) |
                (s9_buf[4] << 20) | (s9_buf[5] << 24) | (s9_buf[6] << 28);
      break;
    case 4:
      return 4 | (s9_buf[0] << 4) | (s9_buf[1] << 9) | (s9_buf[2] << 14) | (s9_buf[3] << 19) |
                (s9_buf[4] << 24);
      break;
    case 5:
      return 5 | (s9_buf[0] << 4) | (s9_buf[1] << 11) | (s9_buf[2] << 18) | (s9_buf[3] << 25);
      break;
    case 6:
      return 6 | (s9_buf[0] << 4) | (s9_buf[1] << 13) | (s9_buf[2] << 22);
      break;
    case 7:
      return 7 | (s9_buf[0] << 4) | (s9_buf[1] << 18);
      break;
    case 8:
      return 8 | (s9_buf[0] << 4);
      break;
    default:
      dief("Simple9 code( %d ) out of bounds\n",code);
  }
}

int serializer::read_s9(uint32_t &v) {
  if (s9_i >= s9_len) {
    s9_i = 0;
    uint32_t val;
    if (!read_uint32(val)) return -1;
    s9_len = decode_s9(s9_buf,val);
    //s9_i = fbuffer_fill_s9list(s9_buf);
    if (s9_len < 0) {
      int ret = s9_i;
      s9_len = 0;
      return ret;
    }
  }
  if (s9_i >= s9_len) {
    die("Simple9 failed read\n");
  }
  v = s9_buf[s9_i++];
  return 0;
}

bool serializer::write_s9(int v) {
  if (s9_i < 0) { die("invalid count for s9_i"); }
  if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
  if (v < 0) {
    v=-v;
    if (v < 9 || v > 15) {
      dief("Simple9 flag( %d ) out of bounds\n",-v);
    } else {
      flush_s9();
      write_uint8((uint8_t)v);
      if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
      return true;
    }
  }
  if ((uint32_t)v > s9_field_max[8]) {
    dief("Simple9 value overflow( %ld )\n", (long)v);
  }
    if (s9_i < 0) { die("invalid count for s9_i"); }
  s9_buf[s9_i++] = (uint32_t)v;
  if (s9_i >= 28) { //s9 buffer is full
    int flushcode=0;
    int i;
    for(i=0;i<s9_i;++i) {
      int newcode=flushcode;
      while (s9_buf[i] > s9_field_max[newcode]) newcode++; //figure out code new item requires

      if (i >= s9_field_count[newcode]) { //flush required on prev item
        while (s9_field_count[flushcode] > i) flushcode++;
        break;
      } else if ((i+1) == s9_field_count[newcode]) { //flush required on cur item
        flushcode=newcode;
        break;
      } else { //still have more space
        flushcode=newcode;
      }
    }

    //fprintf(stderr, "Simple9 write flush\n");
    if (!write_uint32(pack_s9(s9_buf, flushcode))) return false;
    int count=s9_field_count[flushcode];
    if (s9_i < 0) { die("invalid count for s9_i"); }

    for(i=count; i < s9_i; i++) {
      s9_buf[i-count] = s9_buf[i];
    }
    s9_i -= count;
    if (s9_i < 0) { die("invalid count for s9_i"); }
  }
  if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
  return true;
}

void serializer::clear_s9() {
  s9_i = s9_len = 0;
}

bool serializer::flush_s9() {
  if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
  int flushcode;
  //first do as many full flushes as we can
  while(s9_i > 0) {
    flushcode=0;
    int i;
    bool doflush=false;
    for(i=0;i<s9_i;++i) {
      int newcode=flushcode;
      while (s9_buf[i] > s9_field_max[newcode]) newcode++; //figure out code new item requires
      if (newcode>8) {
        dief("Simple9 newcode( %d )\n",flushcode);
      }

      if (i >= s9_field_count[newcode]) { //flush required on prev item
        while (s9_field_count[flushcode] > i) flushcode++;
        doflush=true;
        break;
      } else if ((i+1) == s9_field_count[newcode]) { //flush required on cur item
        flushcode=newcode;
        doflush=true;
        break;
      } else { //still have more space
        flushcode=newcode;
      }
    }
    //fprintf(stderr, "Simple9 flush flush\n");
    if (!write_uint32(pack_s9(s9_buf, flushcode))) return false;
    int count=s9_field_count[flushcode];

    for(i=count; i < s9_i; i++) {
      s9_buf[i-count] = s9_buf[i];
    }
    s9_i -= count;
    if (s9_i < 0) { //at the end of a flush, it's ok if there are some garbage fields (since we already track num actual fields)
      s9_i = 0;
    }
  }
  if (s9_i > 0) { 
    die("Simple9 flush fail\n");
  }
  if (this->bufcap < 2) { dief("invalid bufcap %lu",this->bufcap); }
  return true;
}

uint64_t serializer::pack754(long double f, unsigned bits, unsigned expbits)
{
    long double fnorm;
    int shift;
    long long sign, exp, significand;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (f == 0.0) return 0; // get this special case out of the way

    // check sign and begin normalization
    if (f < 0) { sign = 1; fnorm = -f; }
    else { sign = 0; fnorm = f; }

    // get the normalized form of f and track the exponent
    shift = 0;
    while(fnorm >= 2.0) { fnorm /= 2.0; shift++; }
    while(fnorm < 1.0) { fnorm *= 2.0; shift--; }
    fnorm = fnorm - 1.0;

    // calculate the binary form (non-float) of the significand data
    significand = fnorm * ((1LL<<significandbits) + 0.5f);

    // get the biased exponent
    exp = shift + ((1<<(expbits-1)) - 1); // shift + bias

    // return the final answer
    return (sign<<(bits-1)) | (exp<<(bits-expbits-1)) | significand;
}

long double serializer::unpack754(uint64_t i, unsigned bits, unsigned expbits)
{
    long double result;
    long long shift;
    unsigned bias;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (i == 0) return 0.0;

    // pull the significand
    result = (i&((1LL<<significandbits)-1)); // mask
    result /= (1LL<<significandbits); // convert back to float
    result += 1.0f; // add the one back on

    // deal with the exponent
    bias = (1<<(expbits-1)) - 1;
    shift = ((i>>significandbits)&((1LL<<expbits)-1)) - bias;
    while(shift > 0) { result *= 2.0; shift--; }
    while(shift < 0) { result /= 2.0; shift++; }

    // sign it
    result *= (i>>(bits-1))&1? -1.0: 1.0;

    return result;
}

uint64_t serializer::get_SHA1(const void *buffer, int len) {
  unsigned char md[SHA_DIGEST_LENGTH];
  //SHA_CTX ctx;
  //SHA1_Init(&ctx);
  //SHA1_Update(&ctx, buffer, len);
  //SHA1_Final(md, &ctx);
  SHA1((const unsigned char *)buffer, len, md);
  return *((uint64_t*)md);
}


uint64_t serializer::get_MD5(const void *buffer, int len) {
  unsigned char md[MD5_DIGEST_LENGTH];
  //MD5_CTX ctx;
  //MD5_Init(&ctx);
  //MD5_Update(&ctx, buffer, len);
  //MD5_Final(md, &ctx);
  MD5((const unsigned char *)buffer, len, md);
  return *((uint64_t*)md);
}

size_t serializer::binary_str_search(char *buffer, const size_t max_size, const char *pat, size_t patlen, char delim) {
  off_t old = lseek( fd, 0, SEEK_CUR );
  size_t fsize = lseek( fd, 0, SEEK_END );
  lseek(fd,old,SEEK_SET);
  size_t l=0, r= fsize-patlen;
  int cmp;

  //FIXME: super naive binary search over file. only here because there is some error in the real code
  
  while (l<=r) {
    size_t m=(l+r)/2;
    seek(m);
    if (buflen-bufi < patlen) aligned_refill(); //refill buffer from 0 to buflen
    if (m == 0) {
      cmp = memcmp(buf+bufi,pat,patlen);
      if (0 == cmp) {
        const char *mark = (const char *)memchr(buf+bufi, delim, buflen);
        if (mark == NULL) {
          die("match too long for serializer buffer");
        } else {
          size_t matchlen = (mark-buf-bufi);
          if (matchlen > max_size) die("match too long for user buffer");
          memcpy(buffer,buf+bufi,matchlen);
          return matchlen;
        }
      } else {
        //die("not found 1");
        return 0;
      }
    }
    size_t m_=m;
    while (delim != buf[bufi] && m_ <= r) {
      bufi++;m_++;
      if (bufi >= buflen) {
        if (!aligned_refill()) {
          //die("not found 2");
          return 0;
        }
      }
    }
    if (m_ > r) { //no delimiters after m, so jump back
      r=m-1;
      continue;
    } else if (delim == buf[bufi]) {
      if (buflen-bufi < patlen) {
        if (m_ == r) { //trailing delimiter at end of search space (or file)
          r=m-1;
          continue;
        } else if (!aligned_refill()) {
          //die("not found 3");
          return 0;
        }
        die("not enough data to read");
      }
      cmp = memcmp(pat,buf+bufi+1,patlen);
      if (cmp < 0) {
        //printf("step left from %lu\n",m_);
        r=m-1;
      } else if (cmp > 0) {
        //printf("step right from %lu\n",m_);
        l=m+1;
      } else {
        const char *mark = (const char *)memchr(buf+bufi+1, delim, buflen);
        if (mark == NULL) {
          if (!aligned_refill()) { die("match too long for serializer buffer"); }
          mark = (const char *)memchr(buf+bufi+1, delim, buflen);
        }
        if (mark == NULL) {
          die("match too long for serializer buffer");
        } else {
          size_t matchlen = (mark-buf-bufi-1); //-1 for preceding delim
          //printf("found match at index %lu of length %lu\n",mark-buf,matchlen);
          if (matchlen > max_size) die("match too long for user buffer");
          memcpy(buffer,buf+bufi+1,matchlen);
          return matchlen;
        }
      }
    } else {
      //die("not found 4");
      return 0;
    }
  }
  return 0;


  //DO NOT USE == this is old version of some broken code
  //while (l<=r) {
  //  size_t m=(l+r)/2;
  //  if (m < bufcap) m=0;
  //  seek(m);
  //  aligned_refill(); //refill buffer from 0 to buflen
  //  if (m == 0) {
  //    cmp = memcmp(buf,pat,patlen);
  //    if (0 == cmp) {
  //      const char *mark = (const char *)memchr(buf, delim, buflen);
  //      if (mark == NULL) {
  //        die("match too long for serializer buffer");
  //      } else {
  //        size_t matchlen = (mark-buf);
  //        if (matchlen > max_size) die("match too long for user buffer");
  //        memcpy(buffer,buf,matchlen);
  //        return matchlen;
  //      }
  //    } else {
  //      size_t off;
  //      cmp = binary_str_search(off,buf,0,buflen,pat,patlen);
  //      if (cmp == 0) {
  //        const char *mark = (const char *)memchr(buf+off, delim, buflen);
  //        if (mark == NULL) {
  //          die("match too long for serializer buffer (needs refill)");
  //        } else {
  //          size_t matchlen = (mark-buf);
  //          if (matchlen > max_size) die("match too long for user buffer");
  //          memcpy(buffer,buf+off,matchlen);
  //          return matchlen;
  //        }
  //      } else return 0;
  //    }
  //  } else {
  //    size_t off;
  //    cmp = binary_str_search(off,buf,0,buflen,pat,patlen);
  //    if (cmp == 0) {
  //      const char *mark = (const char *)memchr(buf+off, delim, buflen);
  //      if (mark == NULL) {
  //        die("match too long for serializer buffer (needs refill)");
  //      } else {
  //        size_t matchlen = (mark-buf);
  //        if (matchlen > max_size) die("match too long for user buffer");
  //        memcpy(buffer,buf+off,matchlen);
  //        return matchlen;
  //      }
  //    } else if (cmp == -1) {
  //      return 0;
  //    } else if (cmp == -3) {
  //      l=m+1;
  //    } else if (cmp == -2) {
  //      r=m-1;
  //    } else if (cmp == -4) {
  //      die("need to handle -4");
  //    } else if (cmp == -5) {
  //      die("need to handle -5");
  //    } else {
  //      die("invalid cmp value");
  //    }
  //  }
  //}
  //return 0;
}

//DO NOT USE: == this is the old version of some broken code (will be fixed in future commit)
int serializer::binary_str_search(size_t &r, const char *buf, size_t bufi, size_t buflen, const char *pat, size_t patlen) {
  size_t i = bufi;
  const char *mark = (const char *)memchr(buf+i,pat[0],buflen-i);
  //const char *mark = (const char *)strstr(buf+i,pat);
  int first=1;
  while(mark != NULL) {
    if ((mark-buf)+patlen >= buflen) return -3; //result comes after this chunk
    int cmp = memcmp(mark,pat,patlen);
    if (cmp > 0) {
      if (first) return -2; //result comes before this chunk
      else return -1; //result should be in this chunk, but doesn't exist
    } else if (cmp == 0) {
      r = (mark-buf);
      return 0;
    } else {
      i = (mark-buf)+patlen;
      mark = (const char *)memchr(buf+i,pat[0],buflen-i);
      first = 0;
    }
  }
  if (first) return -4; //didn't find first character at all, so no information given
  else { //go back and see if we have a prefix match
    //memrchr is what we actually want to use, but not all systems support that so we just do a manual reverse traversal
    //mark = (const char *)memrchr(buf+bufi,pat[0],buflen-bufi); //TODO: probably not needed (just use previous memchr results)
    mark = NULL;
    for(const char *it=buf+bufi+buflen-bufi-1; it >= buf; --it) {
      if (*it == pat[0]) {
        mark = it;
        break;
      }
    }

    size_t rem = (buflen - (mark-buf)); //remaining chars
    size_t i = 1;
    while(i < rem) {
      if (pat[i] < mark[i]) return -1; // it was in this chunk, but just before this last entry
      else if (pat[i] != mark[i]) return -3; //after this chunk
    }
    r = (mark-buf);
    return -5; //possible prefix match
  }

}
