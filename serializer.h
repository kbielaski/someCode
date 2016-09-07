#ifndef _SERIALIZER_H_
#define _SERIALIZER_H_

#include <inttypes.h>
#include <cstdio>
#include <vector>
#include <string>

#define pack754_32(f) (serializer::pack754((f), 32, 8))
#define pack754_64(f) (serializer::pack754((f), 64, 11))
#define unpack754_32(i) (serializer::unpack754((i), 32, 8))
#define unpack754_64(i) (serializer::unpack754((i), 64, 11))


class serializer {
  public:
    static uint64_t pack754(long double f, unsigned bits, unsigned expbits);
    static long double unpack754(uint64_t i, unsigned bits, unsigned expbits);
    static uint64_t get_SHA1(const void *buffer, int len);
    static uint64_t get_MD5(const void *buffer, int len);

    enum ser_mode { READ, WRITE, READWRITE };
    //fd - file descriptor to use for backend
    //mode - READ, WRITE, or READWRITE
    //bufcap - buffer capacity
    //watermark - should flush when fill level crosses watermark (X bytes in the buffer, or X bytes left to read)
    //
    serializer(int fd, enum ser_mode mode, size_t bufcap = (size_t)(1<<20), size_t watermark = (size_t)-1, size_t alignment = 8192, bool canresize = false);
    serializer(std::string filename, enum ser_mode mode, size_t bufcap = (size_t)(1<<20), size_t watermark = (size_t)-1, size_t alignment = 8192, bool canresize = false);
    serializer(char *buf, size_t buflen, size_t bufcap, bool canresize = false);
    ~serializer();

    //WARNING: flushes can break alignment, so use them with care (best if they are only at the end of the file)
    bool flush();

    //return value is actual size of read/write if operation fails part way through
    int read_bytes(char *buf, size_t size);
    int write_bytes(const char *buf, size_t size);

    int write_string(const char *str);

    int read_tok(char *buffer, char d, const size_t &max_size);
    inline int read_line(char *buffer, const size_t &max_size) { return read_tok(buffer, '\n', max_size); }
    int read_string(char *buf, const size_t &max_size); //read null-terminated string, returns number of chars read or -1

    bool seek(size_t offset);
    size_t pos();

    //note: these could all be overloads, but with non-self-describing serialization it is better to be explicit
    //return value is whether operation was successful
    bool read_uint8(uint8_t &v);
    bool read_uint16(uint16_t &v);
    bool read_uint24(uint32_t &v);
    bool read_uint32(uint32_t &v);
    bool read_uint64(uint64_t &v);

    bool write_uint8(uint8_t v);
    bool write_uint16(uint16_t v);
    bool write_uint24(uint32_t v);
    bool write_uint32(uint32_t v);
    bool write_uint64(uint64_t v);

    bool read_vbyte32(uint32_t &v);
    bool write_vbyte32(uint32_t v);
    bool read_vbyte64(uint64_t &v);
    bool write_vbyte64(uint64_t v);

    //return value is either in -9 through -15 (flag value), -1 (error or failed read), 0 (success)
    //reads zero, 1, or 4 bytes (zero if we already have values, 1 if we read a flag, 4 otherwise)
    int read_s9(uint32_t &v);
    //writes zero, 1, or 4 bytes (zero if we are stil collecting values, 1 if v is in -9 to -15 (flag), 4 if we are flushing a uint32
    bool write_s9(int v);
    bool flush_s9();
    void clear_s9();

    inline bool can_read() { return mode != WRITE; }
    inline bool can_write() { return mode != READ; }
    inline bool can_rw() { return mode == READWRITE; }

    size_t binary_str_search(char *buffer, const size_t max_size, const char *pat, size_t patlen, char delim);

  protected:
    static const int s9_field_length[9];
    static const int s9_field_count[9];
    static const uint32_t s9_field_max[9];
    int fd;
    enum ser_mode mode;
    char *buf;
    size_t bufcap;
    size_t bufi;
    size_t buflen;
    size_t offset;
    size_t alignment;

    size_t watermark;
    bool canresize;

    uint32_t s9_buf[28];
    int s9_i;
    int s9_len;

    bool aligned_refill();
    bool reserve_read(size_t size);
    bool reserve_write(size_t size);
    bool put_back(size_t size); //put back bytes we have already read (only works if they are still in the buffer)
    bool aligned_flush(); //flush a multiple of the alignment size (may not result in empty buffer)

    static int decode_s9(uint32_t *v, uint32_t val);
    static uint32_t pack_s9(const uint32_t *s9_buf, uint32_t code);
    static int binary_str_search(size_t &r, const char *buf, size_t bufi, size_t buflen, const char *pat, size_t patlen);

};

#endif
