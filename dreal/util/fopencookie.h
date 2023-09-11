// #include <stdio.h>
// #include <stdlib.h>
#include <sstream>
struct memfile_cookie {
  std::stringstream* buf; /* Dynamically sized buffer for data */
  // size_t allocated;      /* Size of buf */
  // size_t endpos;         /* Number of characters in buf */
  // off_t offset;          /* Current file offset in buf */
};

void create_stream_as_file_memfile_func(cookie_io_functions_t& memfile_func);
FILE* create_cookie_stream(struct memfile_cookie& mycookie,
                           cookie_io_functions_t& memfile_func);
