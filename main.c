// This file is part of Sienest.
// File created: 2012-10-31 19:16:56

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mush/cursor.h>
#include <mush/space.h>

static int fail(const char *arg0, const char *s) {
   fprintf(stderr, "%s: ", arg0);
   perror(s);
   return 2;
}

int main(int argc, char **argv) {
   if (argc != 2) {
      fprintf(stderr, "Usage: %s <srcfile>\n", argv[0]);
      return 3;
   }
   const int code_fd = open(argv[1], O_RDONLY);
   if (code_fd == -1)
      return fail(argv[0], "open");

   struct stat code_stat;
   if (fstat(code_fd, &code_stat) == -1)
      return fail(argv[0], "fstat");

   // Assumes that type size differences translate directly to differences in
   // the value ranges supported. If off_t had a defined maximum value in some
   // header, this wouldn't be necessary.
   if ((sizeof(off_t) >  sizeof(size_t) && code_stat.st_size > (off_t)SIZE_MAX)
    || (sizeof(off_t) == sizeof(size_t) && code_stat.st_size > PTRDIFF_MAX))
   {
      fprintf(stderr, "%s: too large file size %jd, can only load up to %zu\n",
              argv[0], (intmax_t)code_stat.st_size, SIZE_MAX);
      return 2;
   }

   const size_t code_len = code_stat.st_size;

   unsigned char *code = mmap(0, code_len, PROT_READ, MAP_PRIVATE, code_fd, 0);
   if (code == MAP_FAILED)
      return fail(argv[0], "mmap");

   close(code_fd);

   mushspace2 *space = mushspace2_init(NULL, NULL);
   if (mushspace2_load_string(space, code, code_len, NULL, MUSHCOORDS2(0,0),
                              false))
   {
      fprintf(stderr, "%s: loading failed\n", argv[0]);
      return 2;
   }
   munmap(code, code_len);

   mushcursor2 *cursor = NULL;
   if (mushcursor2_init(&cursor, space, MUSHCOORDS2(0,0), MUSHCOORDS2(0,0))) {
      fprintf(stderr, "%s: creating cursor failed\n", argv[0]);
      return 2;
   }
}
