#include "fopencookie.h"

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/types.h>
// #include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>

#define INIT_BUF_SIZE 4
#define UNUSED(expr) \
  do {               \
    (void)(expr);    \
  } while (0)

ssize_t memfile_write(void* c, const char* buf, size_t size) {
  // char* new_buff;
  struct memfile_cookie* cookie = static_cast<memfile_cookie*>(c);

  /* Buffer too small? Keep doubling size until big enough. */

  cookie->buf->write(buf, size);
  return cookie->buf->gcount();

  // while (size + cookie->offset > cookie->allocated) {
  //   new_buff = static_cast<char*>(realloc(cookie->buf, cookie->allocated *
  //   2)); if (new_buff == NULL) return -1; cookie->allocated *= 2; cookie->buf
  //   = new_buff;
  // }

  // memcpy(cookie->buf + cookie->offset, buf, size);

  // cookie->offset += size;
  // if (static_cast<size_t>(cookie->offset) > cookie->endpos)
  //   cookie->endpos = cookie->offset;

  return size;
}

ssize_t memfile_read(void* c, char* buf, size_t size) {
  // ssize_t xbytes;
  struct memfile_cookie* cookie = static_cast<memfile_cookie*>(c);

  /* Fetch minimum of bytes requested and bytes available. */
  cookie->buf->read(buf, size);
  return cookie->buf->gcount();

  // xbytes = size;
  // if (cookie->offset + size > cookie->endpos)
  //   xbytes = cookie->endpos - cookie->offset;
  // if (xbytes < 0) /* offset may be past endpos */
  //   xbytes = 0;

  // memcpy(buf, cookie->buf + cookie->offset, xbytes);

  // cookie->offset += xbytes;
  // return xbytes;
}

int memfile_seek(void* c, off64_t* offset, int whence) {
  // off64_t new_offset;
  struct memfile_cookie* cookie = static_cast<memfile_cookie*>(c);

  if (whence == SEEK_SET) {
    std::streampos spos = static_cast<std::streampos>(*offset);
    cookie->buf->seekp(spos);
  } else if (whence == SEEK_END || whence == SEEK_CUR) {
    std::streamoff soff = static_cast<std::streamoff>(*offset);
    std::ios_base::seekdir way;
    if (whence == SEEK_END)
      way = std::ios_base::end;
    else
      way = std::ios_base::cur;
    cookie->buf->seekp(soff, way);
  } else
    return -1;

  // if (new_offset < 0) return -1;

  // cookie->offset = new_offset;
  // *offset = new_offset;
  return 0;
}

int memfile_close(void* c) {
  // struct memfile_cookie* cookie = static_cast<memfile_cookie*>(c);
  UNUSED(c);
  // free(cookie->buf);
  // cookie->allocated = 0;
  // cookie->buf = NULL;

  return 0;
}

void create_stream_as_file_memfile_func(cookie_io_functions_t& memfile_func) {
  memfile_func.read = memfile_read;
  memfile_func.write = memfile_write;
  memfile_func.seek = memfile_seek;
  memfile_func.close = memfile_close;
}

FILE* create_cookie_stream(struct memfile_cookie& mycookie,
                           cookie_io_functions_t& memfile_func) {
  FILE* stream;

  /* Set up the cookie before calling fopencookie(). */

  // mycookie.buf = static_cast<char*>(malloc(INIT_BUF_SIZE));
  // if (mycookie.buf == NULL) {
  //   perror("malloc");
  //   exit(EXIT_FAILURE);
  // }

  // mycookie.allocated = INIT_BUF_SIZE;
  // mycookie.offset = 0;
  // mycookie.endpos = 0;

  stream = fopencookie(&mycookie, "w+", memfile_func);
  if (stream == NULL) {
    perror("fopencookie");
    exit(EXIT_FAILURE);
  }
  return stream;
}
