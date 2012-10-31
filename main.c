// This file is part of Sienest.
// File created: 2012-10-31 19:16:56

#define _POSIX_SOURCE
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

#include "stack.h"

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

   mushcoords2 delta = MUSHCOORDS2(1,0);
   bool stringmode = false;
   CellContainer  cc_buf = cc_init(0),
                 *cc     = &cc_buf;

   for (;;) {
      if (stringmode ? mushcursor2_skip_to_last_space(cursor, delta)
                     : mushcursor2_skip_markers(cursor, delta))
      {
         mushcoords2 pos = mushcursor2_get_pos(cursor);
         fprintf(stderr, "%s: cursor infloops at ( %ld %ld )\n",
                 argv[0], pos.x, pos.y);
         return 1;
      }

      if (stringmode) {
         cell c = mushcursor2_get(cursor);
         if (c == '"')
            stringmode = false;
         else
            cc_push(cc, c);
         mushcursor2_advance(cursor, delta);
         continue;
      }

      cell i = mushcursor2_get_unsafe(cursor);
      switch (i) {
      case '>': delta = MUSHCOORDS2( 1, 0); break;
      case '<': delta = MUSHCOORDS2(-1, 0); break;
      case '^': delta = MUSHCOORDS2( 0,-1); break;
      case 'v': delta = MUSHCOORDS2( 0, 1); break;

      case 'r': goto reverse;

      case '#':
         mushcursor2_advance(cursor, delta);
         break;

      case '@': goto done;

      case 'z': break;

      case '!': cc_push(cc, !cc_pop(cc)); break;
      case '`': {
         cell c = cc_pop(cc);
         cc_push(cc, cc_pop(cc) > c);
         break;
      }

      case '_':
         if (cc_pop(cc))
            delta = MUSHCOORDS2(-1,0);
         else
            delta = MUSHCOORDS2( 1,0);
         break;
      case '|':
         if (cc_pop(cc))
            delta = MUSHCOORDS2(0,-1);
         else
            delta = MUSHCOORDS2(0, 1);
         break;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
         cc_push(cc, i - '0');
         break;

      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
         cc_push(cc, i - 'a' + 10);
         break;

      case '+': cc_push(cc, cc_pop(cc) + cc_pop(cc)); break;
      case '*': cc_push(cc, cc_pop(cc) * cc_pop(cc)); break;
      case '-': {
         cell c = cc_pop(cc);
         cc_push(cc, cc_pop(cc) - c);
         break;
      }
      case '/': {
         cell a = cc_pop(cc),
              b = cc_pop(cc);
         cc_push(cc, a ? b / a : a);
         break;
      }
      case '%': {
         cell a = cc_pop(cc),
              b = cc_pop(cc);
         cc_push(cc, a ? b % a : a);
         break;
      }

      case '"': stringmode = true; break;

      case '\'':
         mushcursor2_advance(cursor, delta);
         cc_push(cc, mushcursor2_get(cursor));
         break;

      case '$': cc_popN(cc, 1); break;

      case ':': {
         cell c = cc_pop(cc);
         cc_push(cc, c);
         cc_push(cc, c);
         break;
      }

      case '\\': {
         cell a = cc_pop(cc),
              b = cc_pop(cc);
         cc_push(cc, a);
         cc_push(cc, b);
         break;
      }

      case 'n': cc_clear(cc); break;

      case 'g': {
         mushcoords2 vec;
         vec.y = cc_pop(cc);
         vec.x = cc_pop(cc);
         cc_push(cc, mushspace2_get(space, vec));
         break;
      }
      case 'p': {
         mushcoords2 vec;
         vec.y = cc_pop(cc);
         vec.x = cc_pop(cc);
         mushspace2_put(space, vec, cc_pop(cc));
         break;
      }

      case '.': printf("%ld ", cc_pop(cc)); break;
      case ',': {
         cell c = cc_pop(cc);
         putchar_unlocked(c);
         if (c == '\n')
            fflush(stdout);
         break;
      }

      case '~': {
         int i;
         unsigned char c;
         fflush(stdout);
         if ((i = getchar_unlocked()) == EOF)
            goto reverse;
         if ((c = i) == '\r') {
            if ((i = getchar_unlocked()) != '\n')
               ungetc(i, stdin);
            c = '\n';
         }
         cc_push(cc, c);
         break;
      }

reverse:
      default: delta = mushcoords2_sub(MUSHCOORDS2(0,0), delta); break;
      }
      mushcursor2_advance(cursor, delta);
   }
done:;
}
