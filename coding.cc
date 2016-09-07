#include "coding.h"
#include <inttypes.h>
#define CODING_DEBUG 1

void fbuffer_init(struct file_buffer *fb, const int fd, const int rw) {
  fb->fd = fd;
  fb->rw = rw;
  if (rw) {
    fb->i = BUFLEN;
    fb->s9_i = 28;
  } else {
    fb->i = 0;
    fb->s9_i = 0;
  }
  if (fb->fd >= 0) {
    off_t offset = lseek( fb->fd, 0, SEEK_CUR );
    if (offset == (off_t)-1) {
      fb->offset = (uint64_t)-1; //just in case bit widths aren't the same
    } else {
      fb->offset = (uint64_t)offset;
    }
  }
}

uint64_t fbuffer_get_file_offset(struct file_buffer *fb) {
  //off_t offset = lseek( fb->fd, 0, SEEK_CUR );
  if (fb->rw) {
    return fb->offset - (BUFLEN - fb->i);
  } else {
    return fb->i + fb->offset;
  }
}

int fbuffer_seek_file_offset(struct file_buffer *fb, uint64_t offset) {
  //TODO: optimize seeks better
  uint64_t cur_offset = fbuffer_get_file_offset(fb);
  uint64_t bufspace = BUFLEN-fb->i;

  if (fb->rw && offset >= cur_offset && (bufspace >= (offset-cur_offset))) { //it is within the current buffer (after current pos), and we are reading
    fb->i += offset-fb->offset; //just adjust offset into buffer, and clear s9 state
    if (fb->rw) {
      fb->s9_i = 28;
    } else {
      fb->s9_i = 0;
    }
  } else { //not an optimized case, so lseek and then clear all state
    off_t ret = lseek(fb->fd, offset, SEEK_SET);
    if (ret == (off_t)-1) return -1;
    if (fb->rw) {
      fb->i = BUFLEN;
      fb->s9_i = 28;
    } else {
      fb->i = 0;
      fb->s9_i = 0;
    }
    fb->offset = offset;
  }
  return 0;
}

void fbuffer_write(struct file_buffer *fb, const char *v, const int size) {
  if (size > BUFLEN) {
    fbuffer_flush(fb);
    write(fb->fd, v, size);
    fb->i = size;
  } else {
    fbuffer_reserve(fb, size);
    memcpy(&fb->buffer[fb->i], v, size);
    fb->i += size;
  }

}

//this currently has some problems with it - don't use
//void fbuffer_read(struct file_buffer *fb, char *v, int size) {
//    int _size = size;
//    if(_size > BUFLEN-READSIZE) {
//        memcpy(v, &fb->buffer[fb->i], BUFLEN-fb->i);
//        v += BUFLEN-fb->i;
//        _size-=BUFLEN-fb->i;
//        fb->i = BUFLEN;
//        int read_size = _size & ~(READSIZE-1);
//        int r;
//        while(read_size > 0) {
//            r = read(fb->fd, v, read_size); //TODO: check
//            v += read_size;
//            _size -= read_size;
//        }
//    }
//    if (_size > 0) {
//        if (-1 == fbuffer_ensure(fb, _size)) return -1;
//        memcpy(v, &fb[fb->i], _size);
//        fb->i += _size;
//    }
//    return size;
//}

int fbuffer_getline(struct file_buffer *fb, char *v, const int size) {
  char *start = &fb->buffer[fb->i];
  char *delim = (char*)memchr(start,'\n',BUFLEN-fb->i);
  if (NULL == delim) {
    fbuffer_refill(fb);
    start = &fb->buffer[fb->i];
    delim = (char*)memchr(start,'\n',BUFLEN-fb->i);
  }
  int len=delim-start;
  if (NULL == delim) {
    return -1;
  } else if (len > size) {
    fprintf(stderr, "user buffer too small for %d characters\n",len);
    return -1;
  } else {
    memcpy(v, start, len);
    v[delim-start] = '\0';
    fb->i += len+1;
    return len;
  }
}

int fbuffer_read_string(struct file_buffer *fb, char *v, const int size) {
  char *start = &fb->buffer[fb->i];
  char *delim = (char*)memchr(start,'\0',BUFLEN-fb->i);
  if (NULL == delim) {
    fbuffer_refill(fb);
    start = &fb->buffer[fb->i];
    delim = (char*)memchr(start,'\0',BUFLEN-fb->i);
  }
  int len=delim-start;
  if (NULL == delim) {
    fprintf(stderr, "no delin found after %d characters\n",BUFLEN-fb->i);
    return -1;
  } else if (len > size) {
    fprintf(stderr, "user buffer too small for %d characters\n",len);
    return -1;
  } else {
    memcpy(v, start, len+1);
    fb->i += len+1;
    return len;
  }
}

void fbuffer_write_uint8(struct file_buffer *fb, const uint8_t v) {
  fbuffer_reserve(fb,1);
  fb->buffer[fb->i++] = v;
}

void fbuffer_write_uint16(struct file_buffer *fb, uint16_t v) {
  fbuffer_reserve(fb,2);
  //TODO: for safety we should manually convert, but cast is faster
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff;
  //*((uint16_t*)(&fb->buffer[fb->i])) = v;
  //fb->i += 2;
}

void fbuffer_write_uint32(struct file_buffer *fb, uint32_t v) {
  //fprintf(stderr, "writing: %u\n",v);
  fbuffer_reserve(fb,4);
  //TODO: for safety we should manually convert, but cast is faster
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff;
  //*((uint32_t*)(&fb->buffer[fb->i])) = v;
  //fb->i += 4;
  //fprintf(stderr, "wrote: %u\n", *((uint32_t*)(&fb->buffer[fb->i-4])));
}

void fbuffer_write_uint64(struct file_buffer *fb, uint64_t v) {
  //printf("writing %" PRIx64 "(%" PRIx8 ")\n",v,(uint32_t)(v&0xff));
  fbuffer_reserve(fb,8);
  //TODO: for safety we should manually convert, but cast is faster
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff; v >>= 8;
  fb->buffer[fb->i++] = v & 0xff;
  //*((uint64_t*)(&fb->buffer[fb->i])) = v;
  //fb->i += 8;
}

int fbuffer_read_uint8(struct file_buffer *fb, uint8_t *v) {
  if (fbuffer_ensure(fb,1)) return -1;
  *v = fb->buffer[fb->i++];
  return 0;
}
int fbuffer_read_uint16(struct file_buffer *fb, uint16_t *v) {
  if (fbuffer_ensure(fb,2)) return -1;
  //TODO: for safety we should manually convert, but cast is faster
  *v = (uint16_t)((unsigned char)fb->buffer[fb->i]) |
      ((uint16_t)((unsigned char)fb->buffer[fb->i+1]) << 8);
  //*v = *((uint16_t*)(&fb->buffer[fb->i]));
  fb->i+=2;
  return 0;
}
int fbuffer_read_uint24(struct file_buffer *fb, uint32_t *v) {
  if (fbuffer_ensure(fb,3)) return -1;

  *v = (uint32_t)((unsigned char)fb->buffer[fb->i]) |
      ((uint32_t)((unsigned char)fb->buffer[fb->i+1]) << 8) |
      ((uint32_t)((unsigned char)fb->buffer[fb->i+2]) << 16);
  fb->i+=3;
  return 0;
}
int fbuffer_read_uint32(struct file_buffer *fb, uint32_t *v) {
  if (fbuffer_ensure(fb,4)) return -1;

  //TODO: for safety we should manually convert, but cast is faster
  *v = (uint32_t)((unsigned char)fb->buffer[fb->i]) |
      ((uint32_t)((unsigned char)fb->buffer[fb->i+1]) << 8) |
      ((uint32_t)((unsigned char)fb->buffer[fb->i+2]) << 16) |
      ((uint32_t)((unsigned char)fb->buffer[fb->i+3]) << 24);
  //*v = *((uint32_t*)(&fb->buffer[fb->i]));
  //fprintf(stderr, "read: %u from %u\n",*v, *((uint32_t*)(&fb->buffer[fb->i])));
  fb->i+=4;
  return 0;
}
int fbuffer_read_uint64(struct file_buffer *fb, uint64_t *v) {
  if (fbuffer_ensure(fb,8)) return -1;
  //TODO: for safety we should manually convert, but cast is faster
  *v = (uint64_t)((unsigned char)fb->buffer[fb->i]) |
      ((uint64_t)((unsigned char)fb->buffer[fb->i+1]) << 8) |
      ((uint64_t)((unsigned char)fb->buffer[fb->i+2]) << 16) |
      ((uint64_t)((unsigned char)fb->buffer[fb->i+3]) << 24) |
      ((uint64_t)((unsigned char)fb->buffer[fb->i+4]) << 32) |
      ((uint64_t)((unsigned char)fb->buffer[fb->i+5]) << 40) |
      ((uint64_t)((unsigned char)fb->buffer[fb->i+6]) << 48) |
      ((uint64_t)((unsigned char)fb->buffer[fb->i+7]) << 56);
  //*v = *((uint64_t*)(&fb->buffer[fb->i]));
  fb->i+=8;
  return 0;
}

void fbuffer_write_vbyte(struct file_buffer *fb, uint32_t v) {
  if (v < (1<<7)) {
    fbuffer_reserve(fb,1);
    fb->buffer[fb->i++] = v;
  } else if (v < ((uint32_t)1<<14)) {
    fbuffer_reserve(fb,2);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  } else if (v < ((uint32_t)1<<21)) {
    fbuffer_reserve(fb,3);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  } else if (v < ((uint32_t)1<<28)) {
    fbuffer_reserve(fb,4);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  } else {
    fbuffer_reserve(fb,5);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  }
}

void fbuffer_write_vbyte64(struct file_buffer *fb, uint64_t v) {
  if (v < ((uint64_t)1<<7)) {
    fbuffer_reserve(fb,1);
    fb->buffer[fb->i++] = v;
  } else if (v < ((uint64_t)1<<14)) {
    fbuffer_reserve(fb,2);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  } else if (v < ((uint64_t)1<<21)) {
    fbuffer_reserve(fb,3);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  } else if (v < ((uint64_t)1<<28)) {
    fbuffer_reserve(fb,4);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  } else if (v < ((uint64_t)1<<35)) {
    fbuffer_reserve(fb,5);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  } else if (v < ((uint64_t)1<<42)) {
    fbuffer_reserve(fb,6);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  } else if (v < ((uint64_t)1<<49)) {
    fbuffer_reserve(fb,7);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  } else if (v < ((uint64_t)1<<56)) {
    fbuffer_reserve(fb,8);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  } else {
    fbuffer_reserve(fb,9);
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = ((v & 0xff) | 128); v >>= 7;
    fb->buffer[fb->i++] = v;
  }
}

int fbuffer_read_vbyte(struct file_buffer *fb, uint32_t *v) {
  uint8_t t;
  if (fbuffer_read_uint8(fb, &t)) return -1;
  *v = (uint32_t)(t&127);
  if (t & 0x80) {
    fbuffer_read_uint8(fb, &t);
    *v |= (uint32_t)(t&127) << 7;
    if (t & 0x80) {
      fbuffer_read_uint8(fb, &t);
      *v |= (uint32_t)(t&127) << 14;
      if (t & 0x80) {
        fbuffer_read_uint8(fb, &t);
        *v |= (uint32_t)(t&127) << 21;
        if (t & 0x80) {
          fbuffer_read_uint8(fb, &t);
          *v |= (uint32_t)(t&127) << 28;
        }
      }
    }
  }
  return 0;
}

int fbuffer_read_vbyte64(struct file_buffer *fb, uint64_t *v) {
  uint8_t t;
  if (fbuffer_read_uint8(fb, &t)) return -1;
  *v = (uint64_t)(t&127);
  if (t & 0x80) {
    fbuffer_read_uint8(fb, &t);
    *v |= (uint64_t)(t&127) << 7;
    if (t & 0x80) {
      fbuffer_read_uint8(fb, &t);
      *v |= (uint64_t)(t&127) << 14;
      if (t & 0x80) {
        fbuffer_read_uint8(fb, &t);
        *v |= (uint64_t)(t&127) << 21;
        if (t & 0x80) {
          fbuffer_read_uint8(fb, &t);
          *v |= (uint64_t)(t&127) << 28;
          if (t & 0x80) {
            fbuffer_read_uint8(fb, &t);
            *v |= (uint64_t)(t&127) << 35;
            if (t & 0x80) {
              fbuffer_read_uint8(fb, &t);
              *v |= (uint64_t)(t&127) << 42;
              if (t & 0x80) {
                fbuffer_read_uint8(fb, &t);
                *v |= (uint64_t)(t&127) << 49;
                if (t & 0x80) {
                  fbuffer_read_uint8(fb, &t);
                  *v |= (uint64_t)(t&127) << 56;
                }
              }
            }
          }
        }
      }
    }
  }
  return 0;
}

int countOnes(uint32_t i){ //bit twiddling to count 1s in a 32 bit number
  i = i - ((i >> 1) & 0x55555555);
  i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
  return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

const int field_length[9] = { 1,  2, 3, 4, 5, 7, 9, 14, 28 };
const int field_count[9] = { 28, 14, 9, 7, 5, 4, 3,  2,  1 };
const uint32_t s9_field_max[9] = { 1, (1<<2)-1, (1<<3)-1, (1<<4)-1, (1<<5)-1, (1<<7)-1, (1<<9)-1, (1<<14)-1, (1<<28)-1 };

int decode_s9val(uint32_t *v, uint32_t code) {
  switch (code & 0x0f) { //assumes fbuffer_read_uint32 does little-endian read
    case 0:
      v[0] = (code >> 4) & 1;
      v[1] = (code >> 5) & 1;
      v[2] = (code >> 6) & 1;
      v[3] = (code >> 7) & 1;
      v[4] = (code >> 8) & 1;
      v[5] = (code >> 9) & 1;
      v[6] = (code >> 10) & 1;
      v[7] = (code >> 11) & 1;
      v[8] = (code >> 12) & 1;
      v[9] = (code >> 13) & 1;
      v[10] = (code >> 14) & 1;
      v[11] = (code >> 15) & 1;
      v[12] = (code >> 16) & 1;
      v[13] = (code >> 17) & 1;
      v[14] = (code >> 18) & 1;
      v[15] = (code >> 19) & 1;
      v[16] = (code >> 20) & 1;
      v[17] = (code >> 21) & 1;
      v[18] = (code >> 22) & 1;
      v[19] = (code >> 23) & 1;
      v[20] = (code >> 24) & 1;
      v[21] = (code >> 25) & 1;
      v[22] = (code >> 26) & 1;
      v[23] = (code >> 27) & 1;
      v[24] = (code >> 28) & 1;
      v[25] = (code >> 29) & 1;
      v[26] = (code >> 30) & 1;
      v[27] = (code >> 31) & 1;
      return 28;
    case 1:
      v[0] = (code >> 4) & 3;
      v[1] = (code >> 6) & 3;
      v[2] = (code >> 8) & 3;
      v[3] = (code >> 10) & 3;
      v[4] = (code >> 12) & 3;
      v[5] = (code >> 14) & 3;
      v[6] = (code >> 16) & 3;
      v[7] = (code >> 18) & 3;
      v[8] = (code >> 20) & 3;
      v[9] = (code >> 22) & 3;
      v[10] = (code >> 24) & 3;
      v[11] = (code >> 26) & 3;
      v[12] = (code >> 28) & 3;
      v[13] = (code >> 30) & 3;
      return 14;
    case 2:
      v[0] = (code >> 4) & 7;
      v[1] = (code >> 7) & 7;
      v[2] = (code >> 10) & 7;
      v[3] = (code >> 13) & 7;
      v[4] = (code >> 16) & 7;
      v[5] = (code >> 19) & 7;
      v[6] = (code >> 22) & 7;
      v[7] = (code >> 25) & 7;
      v[8] = (code >> 28) & 7;
      return 9;
    case 3:
      v[0] = (code >> 4) & 15;
      v[1] = (code >> 8) & 15;
      v[2] = (code >> 12) & 15;
      v[3] = (code >> 16) & 15;
      v[4] = (code >> 20) & 15;
      v[5] = (code >> 24) & 15;
      v[6] = (code >> 28) & 15;
      return 7;
    case 4:
      v[0] = (code >> 4) & ((1<<5)-1);
      v[1] = (code >> 9) & ((1<<5)-1);
      v[2] = (code >> 14) & ((1<<5)-1);
      v[3] = (code >> 19) & ((1<<5)-1);
      v[4] = (code >> 24) & ((1<<5)-1);
      return 5;
    case 5:
      v[0] = (code >> 4) & ((1<<7)-1);
      v[1] = (code >> 11) & ((1<<7)-1);
      v[2] = (code >> 18) & ((1<<7)-1);
      v[3] = (code >> 25) & ((1<<7)-1);
      return 4;
    case 6:
      v[0] = (code >> 4) & ((1<<9)-1);
      v[1] = (code >> 13) & ((1<<9)-1);
      v[2] = (code >> 22) & ((1<<9)-1);
      return 3;
    case 7:
      v[0] = (code >> 4) & ((1<<14)-1);
      v[1] = (code >> 18) & ((1<<14)-1);
      return 2;
    case 8:
      v[0] = (code >> 4);
      return 1;
    default:
      return -(code & 0x0f);
  }
}

uint32_t get_s9_val(uint32_t *s9_buf, int s9_code) {
  int i;
  for(i=0;i<field_count[s9_code];++i) {
    if (s9_buf[i] > s9_field_max[s9_code]) {
      fprintf(stderr, "Simple9 value( %u ) overflow for code( %d )\n",s9_buf[i],s9_code);
      exit(-1);
    }
  }
  uint32_t ret;
  switch(s9_code) {
    case 0:
      ret = (s9_buf[0] << 4)   | (s9_buf[1] << 5)   | (s9_buf[2] << 6)   | (s9_buf[3] << 7)   |
            (s9_buf[4] << 8)   | (s9_buf[5] << 9)   | (s9_buf[6] << 10)  | (s9_buf[7] << 11)  |
            (s9_buf[8] << 12)  | (s9_buf[9] << 13)  | (s9_buf[10] << 14) | (s9_buf[11] << 15) |
            (s9_buf[12] << 16) | (s9_buf[13] << 17) | (s9_buf[14] << 18) | (s9_buf[15] << 19) |
            (s9_buf[16] << 20) | (s9_buf[17] << 21) | (s9_buf[18] << 22) | (s9_buf[19] << 23) |
            (s9_buf[20] << 24) | (s9_buf[21] << 25) | (s9_buf[22] << 26) | (s9_buf[23] << 27) |
            (s9_buf[24] << 28) | (s9_buf[25] << 29) | (s9_buf[26] << 30) | (s9_buf[27] << 31);
      break;
    case 1:
      ret = 1 | (s9_buf[0] << 4)   | (s9_buf[1] << 6)  | (s9_buf[2] << 8)   | (s9_buf[3] << 10) |
                (s9_buf[4] << 12)  | (s9_buf[5] << 14) | (s9_buf[6] << 16)  | (s9_buf[7] << 18) |
                (s9_buf[8] << 20)  | (s9_buf[9] << 22) | (s9_buf[10] << 24) | (s9_buf[11] << 26)|
                (s9_buf[12] << 28) | (s9_buf[13] << 30);
      break;
    case 2:
      ret = 2 | (s9_buf[0] << 4)  | (s9_buf[1] << 7)  | (s9_buf[2] << 10) | (s9_buf[3] << 13) |
                (s9_buf[4] << 16) | (s9_buf[5] << 19) | (s9_buf[6] << 22) | (s9_buf[7] << 25) |
                (s9_buf[8] << 28);
      break;
    case 3:
      ret = 3 | (s9_buf[0] << 4)  | (s9_buf[1] << 8)  | (s9_buf[2] << 12) | (s9_buf[3] << 16) |
                (s9_buf[4] << 20) | (s9_buf[5] << 24) | (s9_buf[6] << 28);
      break;
    case 4:
      ret = 4 | (s9_buf[0] << 4) | (s9_buf[1] << 9) | (s9_buf[2] << 14) | (s9_buf[3] << 19) |
                (s9_buf[4] << 24);
      break;
    case 5:
      ret = 5 | (s9_buf[0] << 4) | (s9_buf[1] << 11) | (s9_buf[2] << 18) | (s9_buf[3] << 25);
      break;
    case 6:
      ret = 6 | (s9_buf[0] << 4) | (s9_buf[1] << 13) | (s9_buf[2] << 22);
      break;
    case 7:
      ret = 7 | (s9_buf[0] << 4) | (s9_buf[1] << 18);
      break;
    case 8:
      ret = 8 | (s9_buf[0] << 4);
      break;
    default:
      fprintf(stderr, "Simple9 code( %d ) out of bounds\n",s9_code);
      exit(-1);
  }

#ifdef CODING_DEBUG
  uint32_t vals[28];
  int n = decode_s9val(vals,ret);
  if (n != field_count[s9_code]) {
    fprintf(stderr, "Simple9 code( %d ) redecode fail\n",s9_code);
    int j;
    for(j=0;j<field_count[s9_code];++j) {
      fprintf(stderr, "%u ",s9_buf[i]);
    }
    fprintf(stderr, "\n");
    for(j=0;j<n;++j) {
      fprintf(stderr, "%u ",vals[i]);
    }
    fprintf(stderr, "\nCODE FAIL for code( %d )\n",s9_code);
    exit(-1);
  }
  int DIE=0;
  for(i=0;i<field_count[s9_code];++i) {
    if (s9_buf[i] != vals[i]) DIE=1;
  }
  if (DIE) {
    for(i=0;i<n;++i) {
      fprintf(stderr, "%u ",s9_buf[i]);
    }
    fprintf(stderr, "\n");
    for(i=0;i<n;++i) {
      fprintf(stderr, "%u ",vals[i]);
    }
    fprintf(stderr, "\nVALUE FAIL for code( %d )\n",s9_code);
    exit(-1);
  } else {
    //fprintf(stderr, "NOT DEAD\n");
  }
#endif

  return ret;
}

void fbuffer_write_s9(struct file_buffer *fb, int v) {
  if (v < 0) {
    v=-v;
    if (v < 9 || v > 15) {
      fprintf(stderr, "Simple9 flag( %d ) out of bounds\n",-v);
      exit(-1);
    } else {
      fbuffer_flush_s9(fb);
      fbuffer_write_uint8(fb,(uint8_t)v);
      return;
    }
  }
  if ((uint32_t)v > s9_field_max[8]) {
    fprintf(stderr, "Simple9 value overflow( %ld )\n", (long)v);
    exit(-1);
  }
  fb->s9_buf[fb->s9_i++] = (uint32_t)v;
  if (fb->s9_i >= 28) { //s9 buffer is full
    int flushcode=0;
    int i;
    for(i=0;i<fb->s9_i;++i) {
      int newcode=flushcode;
      while (fb->s9_buf[i] > s9_field_max[newcode]) newcode++; //figure out code new item requires

      if (i > field_count[newcode]) { //flush required on prev item
        while (field_count[flushcode] >= i) flushcode++;
        break;
      } else if (i == field_count[newcode]) { //flush required on cur item
        flushcode=newcode;
        break;
      } else { //still have more space
        flushcode=newcode;
      }
    }

    //fprintf(stderr, "Simple9 write flush\n");
    fbuffer_write_uint32(fb,get_s9_val(fb->s9_buf, flushcode));
    int count=field_count[flushcode];

    for(i=count; i < fb->s9_i; i++) {
      fb->s9_buf[i-count] = fb->s9_buf[i];
    }
    fb->s9_i -= count;
  }
}

//flush the s9 buffer
void fbuffer_flush_s9(struct file_buffer *fb) {
  int flushcode;
  //first do as many cull flushes as we can
  while(fb->s9_i > 0) {
    flushcode=0;
    int i;
    int doflush=0;
    for(i=0;i<fb->s9_i;++i) {
      int newcode=flushcode;
      while (fb->s9_buf[i] > s9_field_max[newcode]) newcode++; //figure out code new item requires
      if (newcode>8) {
        fprintf(stderr, "Simple9 newcode( %d )\n",flushcode);
        exit(-1);
      }

      if (i > field_count[newcode]) { //flush required on prev item
        while (field_count[flushcode] >= i) flushcode++;
#ifdef CODING_DEBUG
        if (flushcode>8) {
          fprintf(stderr, "Simple9 code( %d ) out of bounds in climb\n",flushcode);
          int j;
          for(j=0;j<i;++j) {
            fprintf(stderr, "%u ",fb->s9_buf[i]);
          }
          fprintf(stderr, "\n");
          exit(-1);
        }
#endif
        doflush=1;
        break;
      } else if (i == field_count[newcode]) { //flush required on cur item
        flushcode=newcode;
        doflush=1;
        break;
      } else { //still have more space
        flushcode=newcode;
      }
    }
    if (doflush) {
      //fprintf(stderr, "Simple9 flush flush\n");
      fbuffer_write_uint32(fb,get_s9_val(fb->s9_buf, flushcode));
      int count=field_count[flushcode];

      for(i=count; i < fb->s9_i; i++) {
        fb->s9_buf[i-count] = fb->s9_buf[i];
      }
      fb->s9_i -= count;
    } else {
      break;
    }
  }
  flushcode=0;
  //if we have leftovers (but not enough to fill a uint32), find the smallest flush code that we can fill and flush
  while(fb->s9_i > 0) {
    while (field_count[flushcode] > fb->s9_i) flushcode++;
    //fprintf(stderr, "Simple9 flush cleanup\n");
    fbuffer_write_uint32(fb,get_s9_val(fb->s9_buf, flushcode));
    int i;
    int count=field_count[flushcode];
    for(i=count; i < fb->s9_i; i++) {
      fb->s9_buf[i-count] = fb->s9_buf[i];
    }
    fb->s9_i -= count;
  }
  if (fb->s9_i > 0) { 
    fprintf(stderr, "Simple9 flush fail\n");
    exit(-1);
  }
}

int fbuffer_read_s9list(struct file_buffer *fb, uint32_t *v) {
  uint32_t code;
  if (fbuffer_read_uint32(fb, &code)) return -1;
  switch (code & 0x0f) { //assumes fbuffer_read_uint32 does little-endian read
    case 0:
      v[0] = (code >> 4) & 1;
      v[1] = (code >> 5) & 1;
      v[2] = (code >> 6) & 1;
      v[3] = (code >> 7) & 1;
      v[4] = (code >> 8) & 1;
      v[5] = (code >> 9) & 1;
      v[6] = (code >> 10) & 1;
      v[7] = (code >> 11) & 1;
      v[8] = (code >> 12) & 1;
      v[9] = (code >> 13) & 1;
      v[10] = (code >> 14) & 1;
      v[11] = (code >> 15) & 1;
      v[12] = (code >> 16) & 1;
      v[13] = (code >> 17) & 1;
      v[14] = (code >> 18) & 1;
      v[15] = (code >> 19) & 1;
      v[16] = (code >> 20) & 1;
      v[17] = (code >> 21) & 1;
      v[18] = (code >> 22) & 1;
      v[19] = (code >> 23) & 1;
      v[20] = (code >> 24) & 1;
      v[21] = (code >> 25) & 1;
      v[22] = (code >> 26) & 1;
      v[23] = (code >> 27) & 1;
      v[24] = (code >> 28) & 1;
      v[25] = (code >> 29) & 1;
      v[26] = (code >> 30) & 1;
      v[27] = (code >> 31) & 1;
      return 28;
    case 1:
      v[0] = (code >> 4) & 3;
      v[1] = (code >> 6) & 3;
      v[2] = (code >> 8) & 3;
      v[3] = (code >> 10) & 3;
      v[4] = (code >> 12) & 3;
      v[5] = (code >> 14) & 3;
      v[6] = (code >> 16) & 3;
      v[7] = (code >> 18) & 3;
      v[8] = (code >> 20) & 3;
      v[9] = (code >> 22) & 3;
      v[10] = (code >> 24) & 3;
      v[11] = (code >> 26) & 3;
      v[12] = (code >> 28) & 3;
      v[13] = (code >> 30) & 3;
      return 14;
    case 2:
      v[0] = (code >> 4) & 7;
      v[1] = (code >> 7) & 7;
      v[2] = (code >> 10) & 7;
      v[3] = (code >> 13) & 7;
      v[4] = (code >> 16) & 7;
      v[5] = (code >> 19) & 7;
      v[6] = (code >> 22) & 7;
      v[7] = (code >> 25) & 7;
      v[8] = (code >> 28) & 7;
      return 9;
    case 3:
      v[0] = (code >> 4) & 15;
      v[1] = (code >> 8) & 15;
      v[2] = (code >> 12) & 15;
      v[3] = (code >> 16) & 15;
      v[4] = (code >> 20) & 15;
      v[5] = (code >> 24) & 15;
      v[6] = (code >> 28) & 15;
      return 7;
    case 4:
      v[0] = (code >> 4) & ((1<<5)-1);
      v[1] = (code >> 9) & ((1<<5)-1);
      v[2] = (code >> 14) & ((1<<5)-1);
      v[3] = (code >> 19) & ((1<<5)-1);
      v[4] = (code >> 24) & ((1<<5)-1);
      return 5;
    case 5:
      v[0] = (code >> 4) & ((1<<7)-1);
      v[1] = (code >> 11) & ((1<<7)-1);
      v[2] = (code >> 18) & ((1<<7)-1);
      v[3] = (code >> 25) & ((1<<7)-1);
      return 4;
    case 6:
      v[0] = (code >> 4) & ((1<<9)-1);
      v[1] = (code >> 13) & ((1<<9)-1);
      v[2] = (code >> 22) & ((1<<9)-1);
      return 3;
    case 7:
      v[0] = (code >> 4) & ((1<<14)-1);
      v[1] = (code >> 18) & ((1<<14)-1);
      return 2;
    case 8:
      v[0] = (code >> 4);
      return 1;
    default:
      if (fb->i < 3) {
        fprintf(stderr, "internal fbuffer failure, s9 index < 3 on flag");
      }
      fb->i -= 3; //return extra bytes to buffer
      return -(code & 0x0f);

  }
}

//this version fills the end of the buffer instead of the start for slightly simpler reads
int fbuffer_fill_s9list(struct file_buffer *fb, uint32_t *v) {
  uint32_t code;
  if (fbuffer_read_uint32(fb, &code)) return -1;
  switch (code & 0x0f) { //assumes fbuffer_read_uint32 does little-endian read
    case 0:
      v[0] = (code >> 4) & 1;
      v[1] = (code >> 5) & 1;
      v[2] = (code >> 6) & 1;
      v[3] = (code >> 7) & 1;
      v[4] = (code >> 8) & 1;
      v[5] = (code >> 9) & 1;
      v[6] = (code >> 10) & 1;
      v[7] = (code >> 11) & 1;
      v[8] = (code >> 12) & 1;
      v[9] = (code >> 13) & 1;
      v[10] = (code >> 14) & 1;
      v[11] = (code >> 15) & 1;
      v[12] = (code >> 16) & 1;
      v[13] = (code >> 17) & 1;
      v[14] = (code >> 18) & 1;
      v[15] = (code >> 19) & 1;
      v[16] = (code >> 20) & 1;
      v[17] = (code >> 21) & 1;
      v[18] = (code >> 22) & 1;
      v[19] = (code >> 23) & 1;
      v[20] = (code >> 24) & 1;
      v[21] = (code >> 25) & 1;
      v[22] = (code >> 26) & 1;
      v[23] = (code >> 27) & 1;
      v[24] = (code >> 28) & 1;
      v[25] = (code >> 29) & 1;
      v[26] = (code >> 30) & 1;
      v[27] = (code >> 31) & 1;
      return 0;
    case 1:
      v[14] = (code >> 4) & 3;
      v[15] = (code >> 6) & 3;
      v[16] = (code >> 8) & 3;
      v[17] = (code >> 10) & 3;
      v[18] = (code >> 12) & 3;
      v[19] = (code >> 14) & 3;
      v[20] = (code >> 16) & 3;
      v[21] = (code >> 18) & 3;
      v[22] = (code >> 20) & 3;
      v[23] = (code >> 22) & 3;
      v[24] = (code >> 24) & 3;
      v[25] = (code >> 26) & 3;
      v[26] = (code >> 28) & 3;
      v[27] = (code >> 30) & 3;
      return 14;
    case 2:
      v[19] = (code >> 4) & 7;
      v[20] = (code >> 7) & 7;
      v[21] = (code >> 10) & 7;
      v[22] = (code >> 13) & 7;
      v[23] = (code >> 16) & 7;
      v[24] = (code >> 19) & 7;
      v[25] = (code >> 22) & 7;
      v[26] = (code >> 25) & 7;
      v[27] = (code >> 28) & 7;
      return 19;
    case 3:
      v[21] = (code >> 4) & 15;
      v[22] = (code >> 8) & 15;
      v[23] = (code >> 12) & 15;
      v[24] = (code >> 16) & 15;
      v[25] = (code >> 20) & 15;
      v[26] = (code >> 24) & 15;
      v[27] = (code >> 28) & 15;
      return 21;
    case 4:
      v[23] = (code >> 4) & ((1<<5)-1);
      v[24] = (code >> 9) & ((1<<5)-1);
      v[25] = (code >> 14) & ((1<<5)-1);
      v[26] = (code >> 19) & ((1<<5)-1);
      v[27] = (code >> 24) & ((1<<5)-1);
      return 23;
    case 5:
      v[24] = (code >> 4) & ((1<<7)-1);
      v[25] = (code >> 11) & ((1<<7)-1);
      v[26] = (code >> 18) & ((1<<7)-1);
      v[27] = (code >> 25) & ((1<<7)-1);
      return 24;
    case 6:
      v[25] = (code >> 4) & ((1<<9)-1);
      v[26] = (code >> 13) & ((1<<9)-1);
      v[27] = (code >> 22) & ((1<<9)-1);
      return 25;
    case 7:
      v[26] = (code >> 4) & ((1<<14)-1);
      v[27] = (code >> 18) & ((1<<14)-1);
      return 26;
    case 8:
      v[27] = (code >> 4);
      return 27;
    default:
      if (fb->i < 3) {
        fprintf(stderr, "internal fbuffer failure, s9 index < 3 on flag");
      }
      fb->i -= 3; //return extra bytes to buffer
      return -(code & 0x0f);
  }
}

int fbuffer_read_s9(struct file_buffer *fb, uint32_t *v) {
  if (fb->s9_i == 28) {
    fb->s9_i = fbuffer_fill_s9list(fb,fb->s9_buf);
    if (fb->s9_i < 0) {
      int ret = fb->s9_i;
      fb->s9_i = 28;
      return ret;
    }
  }
  *v = fb->s9_buf[fb->s9_i++];
  return 0;
}

void bitset_clearall(const struct coding::bitset *b) {
  memset(b->b,0,b->nbits/8);
}
void bitset_setall(const struct coding::bitset *b) {
  memset(b->b,0xff,b->nbits/8);
}


struct coding::bitset *bitset_alloc(int nbits) {
  struct coding::bitset *b = (struct coding::bitset*)malloc(sizeof(struct coding::bitset));
  b->nbits = nbits;
  b->b = (uint32_t*)malloc(((nbits + 7) & ~0x07)/8); //round up number of bytes needed
  bitset_clearall(b);
  return b;
}

void bitset_free(struct coding::bitset *b) {
  free(b->b);
  free(b);
}

uint8_t bitset_getuint8(struct coding::bitset *b, int bytenum) {
  return (b->b[bytenum / 4] >> ((bytenum % 4) * 8)) & 0xff;
}

inline uint32_t bitset_getuint32(struct coding::bitset *b, int uint32_num) {
  return b->b[uint32_num];
}

void bitset_setbit(const struct coding::bitset *b, int n) {
  b->b[n/32] |= 1 << (n%32);
}

void bitset_clearbit(const struct coding::bitset *b, int n) {
  b->b[n/32] &= ~(1 << (n%32));
}
int bitset_test(const struct coding::bitset *b, int n) {
  return (b->b[n/32] & (1 << (n%32)));
}

//&s two bitvectors together. lhs is modified to contain the result of the & operation
void bitset_and(const struct coding::bitset *lhs,const struct coding::bitset *rhs) {
  int i;
  int size=(lhs->nbits < rhs->nbits ? lhs->nbits : rhs->nbits) /32;
  for(i = 0; i < size; i++) {
    lhs->b[i] &= rhs->b[i];
  }
}

//|s two bitvectors together. lhs is modified to contain the result of the | operation
void bitset_or(const struct coding::bitset *lhs,const struct coding::bitset *rhs) {
  int i;
  int size=(lhs->nbits < rhs->nbits ? lhs->nbits : rhs->nbits) /32;
  for(i = 0; i < size; i++) {
    lhs->b[i] |= rhs->b[i];
  }
}

void write_bitset_raw(struct file_buffer *fb,struct coding::bitset *b) {
  uint8_t tmp;
  int size = b->nbits / 8;
  int i;
  for(i = 0; i < size; i++) {
    fbuffer_write_uint8(fb,bitset_getuint8(b,i));
  }
}

////TODO: fix this, it is very inneficient now that we are directly using arrays instead of std::bitset
//void write_bitset_layered1(struct bitset *b) {
//    uint8_t l0 = 0;
//    //uint8_t l1[8];
//    struct bitset *l1_bits = bitset_alloc(8*8);
//    uint8_t l2[8*8];
//    struct bitset *l2_bits = bitset_alloc(8*8*8);
//    //memset(l1,0,8);
//    //memset(l2,0,8*8);
//    int max_bits = l2_bits->nbits;
//    //if (bits > max_bits) {
//    //    fprintf(stderr,"too many versions( %d )\n",bits);
//    //    exit(1);
//    //}
//    //int count = b.count();
//    int i,j;
//    for(i = 0; i < bits; i+= 8) {
//        //if (i+8 >= bits) {
//        //    for(j = i; j < bits; j++) {
//        //        if (bitset_test(b,j)) setbit(l2,j);
//        //    }
//        //} else {
//        //    //l2[i/8] = (b[i] ? 1 : 0) | (b[i+1] ? 2 : 0) | (b[i+2] ? 4 : 0) | (b[i+3] ? 8 : 0) |
//        //    //    (b[i+4] ? 16 : 0) | (b[i+5] ? 32 : 0) | (b[i+6] ? 64 : 0) | (b[i+7] ? 128 : 0);
//        //    l2[i/8] = bitset_getuint8(b,i/8);
//        //}
//        //No special cases as with std::bitset
//        l2[i/8] = bitset_getuint8(b,i/8);
//        if((int)l2[i/8] != 0) {
//            setbit(l1,i/8);
//            setbit(&l0,i/8/8);
//        }
//    }
//    write_uint8(l0);
//    //for(i = 0; i+4 <= 
//    for(i = 0; i < 8; i++) {
//        if (l1[i] != 0) write_uint8(l1[i]);
//    }
//    for(i = 0; i < 8*8; i++) {
//        if (l2[i] != 0) write_uint8(l2[i]);
//    }
//}
//
//void write_bitset_layered0(struct bitset *b, int bits) {
//    uint8_t l0 = 0;
//    uint8_t l1[8];
//    uint8_t l2[8*8];
//    memset(l1,0,8);
//    memset(l2,0,8*8);
//    int max_bits = 8*8*8;
//    if (bits > max_bits) {
//        fprintf(stderr,"too many versions( %d )\n",bits);
//        exit(1);
//    }
//    int i,j;
//    //int count = b.count();
//    for(i = 0; i < bits; i+= 8) {
//        if (i+8 >= bits) {
//            for(j = i; j < bits; j++) {
//                if (bitset_test(b,j)) setbit(l2,j);
//            }
//        } else {
//            //l2[i/8] = (b[i] ? 1 : 0) | (b[i+1] ? 2 : 0) | (b[i+2] ? 4 : 0) | (b[i+3] ? 8 : 0) |
//            //    (b[i+4] ? 16 : 0) | (b[i+5] ? 32 : 0) | (b[i+6] ? 64 : 0) | (b[i+7] ? 128 : 0);
//            l2[i/8] = bitset_getuint8(b,i/8);
//        }
//        if((int)l2[i/8] != 0) {
//            setbit(l1,i/8);
//            setbit(&l0,i/8/8);
//        }
//    }
//    write_uint8(l0);
//    for(i = 0; i < 8; i++) {
//        if (l1[i] != 0) write_uint8(l1[i]);
//    }
//    for(i = 0; i < 8*8; i++) {
//        if (l2[i] != 0) write_uint8(l2[i]);
//    }
//}


//uint8_t result[MD5_DIGEST_LENGTH];
//uint32_t get_MD5(const char *word) {
//    MD5((uint8_t*)word, strlen(word), (uint8_t*)result);
//    return *((uint32_t*)result);
//}

uint64_t get_SHA1(const void *buffer, int len) {
  unsigned char md[SHA_DIGEST_LENGTH];
  //SHA_CTX ctx;
  //SHA1_Init(&ctx);
  //SHA1_Update(&ctx, buffer, len);
  //SHA1_Final(md, &ctx);
  SHA1((const unsigned char *)buffer, len, md);
  return *((uint64_t*)md);
}


uint64_t get_MD5(const void *buffer, int len) {
  unsigned char md[MD5_DIGEST_LENGTH];
  //MD5_CTX ctx;
  //MD5_Init(&ctx);
  //MD5_Update(&ctx, buffer, len);
  //MD5_Final(md, &ctx);
  MD5((const unsigned char *)buffer, len, md);
  return *((uint64_t*)md);
}
