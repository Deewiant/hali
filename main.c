// This file is part of Sienest.
// File created: 2012-10-31 19:16:56

#define _POSIX_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mush/cursor.h>
#include <mush/space.h>

#include "stack.h"

typedef struct {
   char *ptr;
   size_t len;
} char_arr;
static void char_arr_push(char_arr *buf, size_t i, char c);

static char_arr pop_string (CellContainer *cc, char_arr *arr);
static void     push_string(CellContainer *cc, char_arr  arr);

static char_arr strn_buf = {NULL, 0};

static mushcursor2 *strn_cursor = NULL;
static cell cells_length_n;
static int strn_put_foreach(cell *c);
static int cells_length(cell *c);

static mushspace2 *space;
static mushcoords2 delta, offset;
static mushcursor2 *cursor;
static CellContainer cc_buf, *cc = &cc_buf;

static Stack_stack stackstack_buf, *stackstack = NULL;

static bool stringmode = false,
            strn_enabled = false;

static int execute(cell i);

static cell *block_transfer_p;
static void block_transfer_f(cell*, size_t);
static void block_transfer_g(size_t);
static void stack_under_stack_f(cell*, size_t);
static void stack_under_stack_g(size_t);

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

   offset = MUSHCOORDS2(0,0);

   space = mushspace2_init(NULL, NULL);
   if (mushspace2_load_string(space, code, code_len, NULL, offset, false)) {
      fprintf(stderr, "%s: loading failed\n", argv[0]);
      return 2;
   }
   munmap(code, code_len);

   delta = MUSHCOORDS2(1,0);

   cursor = NULL;
   if (mushcursor2_init(&cursor, space, offset, delta))
      goto infloop;

   mushcursor2_init(&strn_cursor, space, offset, MUSHCOORDS2(0,0));

   cc_buf = cc_init(0);

   for (;;) {
      if (stringmode ? mushcursor2_skip_to_last_space(cursor, delta)
                     : mushcursor2_skip_markers(cursor, delta))
      {
infloop:;
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

      switch (execute(mushcursor2_get_unsafe(cursor))) {
      case 0: break;
      case 1: mushcursor2_advance(cursor, delta);
      case 2: continue;
      }
      break;
   }
}

static int execute(mushcell i) {
   switch (i) {
   case '>': delta = MUSHCOORDS2( 1, 0); break;
   case '<': delta = MUSHCOORDS2(-1, 0); break;
   case '^': delta = MUSHCOORDS2( 0,-1); break;
   case 'v': delta = MUSHCOORDS2( 0, 1); break;

turn_right:
   case ']': {
      cell x  = delta.x;
      delta.x = -delta.y;
      delta.y = x;
      break;
   }
turn_left:
   case '[': {
      cell x  = delta.x;
      delta.x = delta.y;
      delta.y = -x;
      break;
   }

   case 'r': goto reverse;

   case 'x':
      delta.y = cc_pop(cc);
      delta.x = cc_pop(cc);
      break;

   case '#':
      mushcursor2_advance(cursor, delta);
      break;

   case '@': return 0;

   case 'z': break;

   case 'j': {
      // FIXME multiplication can overflow
      cell n = cc_pop(cc);
      mushcoords2 jump;
      jump.x = mushcell_mul(delta.x, n);
      jump.y = mushcell_mul(delta.y, n);
      mushcursor2_advance(cursor, jump);
      break;
   }

   case 'k': {
      cell n = cc_pop(cc);
      mushcoords2 pos = mushcursor2_get_pos(cursor);
      mushcursor2_advance(cursor, delta);
      if (n <= 0)
         break;
      mushcursor2_skip_markers(cursor, delta);
      cell i = mushcursor2_get_unsafe(cursor);
      mushcursor2_set_pos(cursor, pos);
      int ret = execute(i);
      if (!ret)
         return 0;
      while (--n)
         execute(i);
      return ret;
   }

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
   case 'w': {
      cell a = cc_pop(cc),
           b = cc_pop(cc);
      if (a > b)
         goto turn_left;
      else if (a < b)
         goto turn_right;
      break;
   }

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
   case 's':
      mushcursor2_advance(cursor, delta);
      mushcursor2_put(cursor, cc_pop(cc));
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

   case '{': {
      if (!stackstack) {
         stackstack_buf = stack_stack_init(2);
         stackstack = &stackstack_buf;
         stack_stack_push(stackstack, cc);
      }
      CellContainer *toss = malloc(sizeof *toss);
      *toss = cc_init(0);
      stack_stack_push(stackstack, toss);
      CellContainer *soss = cc;
      cell n = cc_pop(soss);
      if (n > 0) {
         block_transfer_p = cc_reserve(toss, n);
         cc_mapFirstN(soss, n, block_transfer_f, block_transfer_g);
         cc_popN(soss, n);
      } else if (n < 0) {
         // FIXME -cell_min < 0
         n = -n;
         cell *p = cc_reserve(soss, n);
         while (n--)
            *p++ = 0;
      }
      cc_push(cc, offset.x);
      cc_push(cc, offset.y);
      cc = toss;
      mushcursor2_advance(cursor, delta);
      offset = mushcursor2_get_pos(cursor);
      return 2;
   }
   case '}': {
      if (!stackstack || stack_stack_size(stackstack) == 1)
         goto reverse;
      CellContainer *old = stack_stack_pop(stackstack);
      cc = stack_stack_top(stackstack);
      cell n = cc_pop(old);
      offset.y = cc_pop(cc);
      offset.x = cc_pop(cc);
      if (n > 0) {
         block_transfer_p = cc_reserve(cc, n);
         cc_mapFirstN(old, n, block_transfer_f, block_transfer_g);
      } else if (n < 0) {
         // FIXME -cell_min < 0
         cc_popN(cc, -n);
      }
      cc_free(old);
      free(old);
      break;
   }
   case 'u': {
      if (!stackstack || stack_stack_size(stackstack) == 1)
         goto reverse;
      cell n = cc_pop(cc);
      CellContainer *soss =
         stack_stack_at(stackstack, stack_stack_size(stackstack) - 2);

      CellContainer *src, *tgt;
      if (n > 0) {
         src = soss;
         tgt = cc;
      } else if (n < 0) {
         // FIXME -cell_min < 0
         n = -n;
         src = cc;
         tgt = soss;
      } else
         break;

      block_transfer_p = cc_reserve(tgt, n) + n - 1;
      cc_mapFirstN(src, n, stack_under_stack_f, stack_under_stack_g);
      cc_popN(src, n);
      break;
   }

   case 'g': {
      mushcoords2 vec;
      vec.y = cc_pop(cc) + offset.y;
      vec.x = cc_pop(cc) + offset.x;
      cc_push(cc, mushspace2_get(space, vec));
      break;
   }
   case 'p': {
      mushcoords2 vec;
      vec.y = cc_pop(cc) + offset.y;
      vec.x = cc_pop(cc) + offset.x;
      mushspace2_put(space, vec, cc_pop(cc));
      break;
   }

   case '.': printf("%ld ", cc_pop(cc)); break;
   case ',': {
      char c = (char)cc_pop(cc);
      putchar_unlocked(c);
      if (c == '\n')
         fflush(stdout);
      break;
   }

   case '&': {
      int i;
      unsigned char c;
      fflush(stdout);
      do {
         if ((i = getchar_unlocked()) == EOF)
            goto reverse;
         c = i;
      } while (c < '0' || c > '9');
      ungetc(c, stdin);

      cell n = 0;
      char s[21];
      uint8_t j = 0;

      errno = 0;
      while (j < 20) {
         if ((i = getchar_unlocked()) == EOF)
            break;

         c = i;
         if (c < '0' || c > '9')
            break;

         s[j++] = c;
         s[j]   = 0;
         cell tmp = strtol(s, NULL, 10);
         if (errno)
            break;
         n = tmp;
      }
      if (i != EOF)
         ungetc(c, stdin);
      cc_push(cc, n);
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

   case 'y': {
      cell n = cc_pop(cc);
      switch (n) {
      case  9: cc_push(cc, mushcursor2_get_pos(cursor).x); break;
      case 10: cc_push(cc, mushcursor2_get_pos(cursor).y); break;
      }
      break;
   }

   case '(': {
      cell n = cc_pop(cc);
      if (n <= 0)
         goto reverse;
      cell fing = 0;
      while (n--)
         fing = (fing << 8) + cc_pop(cc);
      if (fing != 0x5354524e)
         goto reverse;
      strn_enabled = true;
      cc_push(cc, fing);
      cc_push(cc, 1);
      break;
   }

   case 'A':
      if (!strn_enabled)
         goto reverse;

      push_string(cc, pop_string(cc, &strn_buf));
      break;
   case 'D': {
      if (!strn_enabled)
         goto reverse;
      bool flush = false;
      char str[1024];
      unsigned i = UINT_MAX;
      do {
         ++i;
         if (i == 1024) {
            fwrite(str, 1, i, stdout);
            i = 0;
         }
         if ((str[i] = (char)cc_pop(cc)) == '\n')
            flush = true;
      } while (str[i]);
      fwrite(str, 1, i, stdout);
      if (flush)
         fflush(stdout);
      break;
   }
   case 'G': {
      if (!strn_enabled)
         goto reverse;
      mushcoords2 vec;
      vec.y = cc_pop(cc) + offset.y;
      vec.x = cc_pop(cc) + offset.x;
      size_t i = 0;
      mushcursor2_set_pos(strn_cursor, vec);
      for (;;) {
         char c = (char)mushcursor2_get(strn_cursor);
         if (c == 0)
            break;
         char_arr_push(&strn_buf, i++, c);
         mushcursor2_advance(strn_cursor, MUSHCOORDS2(1,0));
      }
      cc_push(cc, 0);
      push_string(cc, (char_arr){strn_buf.ptr, i});
      break;
   }
   case 'N':
      if (!strn_enabled)
         goto reverse;
      cells_length_n = 0;
      cc_foreachTopToBottom(cc, cells_length);
      cc_push(cc, cells_length_n - 1);
      break;
   case 'P': {
      if (!strn_enabled)
         goto reverse;
      mushcoords2 vec;
      vec.y = cc_pop(cc) + offset.y;
      vec.x = cc_pop(cc) + offset.x;
      mushcursor2_set_pos(strn_cursor, vec);
      cells_length_n = 0;
      cc_foreachTopToBottom(cc, strn_put_foreach);
      cc_popN(cc, cells_length_n);
      break;
   }

reverse:
   default: delta = mushcoords2_sub(MUSHCOORDS2(0,0), delta); break;
   }
   return 1;
}

static void block_transfer_f(cell* a, size_t n) {
   memcpy(block_transfer_p, a, n * sizeof *a);
   block_transfer_p += n;
}
static void block_transfer_g(size_t n) {
   while (n--)
      *block_transfer_p++ = 0;
}
static void stack_under_stack_f(cell* a, size_t n) {
   while (n--)
      *block_transfer_p-- = *a++;
}
static void stack_under_stack_g(size_t n) {
   while (n--)
      *block_transfer_p-- = 0;
}

static void char_arr_push(char_arr *buf, size_t i, char c) {
   if (i == buf->len) {
      if (!buf->len)
         buf->len = 1024/2;
      buf->ptr = realloc(buf->ptr, (buf->len *= 2) * sizeof *buf->ptr);
   }
   buf->ptr[i] = c;
}
static char_arr pop_string(CellContainer *cc, char_arr *buf) {
   size_t i = SIZE_MAX;
   do char_arr_push(buf, ++i, (char)cc_pop(cc));
   while (buf->ptr[i]);
   return (char_arr){buf->ptr, i};
}
static void push_string(CellContainer *cc, char_arr arr) {
   // FIXME: handle invert mode correctly.
   cell *p = cc_reserve(cc, arr.len);
   for (size_t i = arr.len; i--;)
      *p++ = arr.ptr[i];
}

static int strn_put_foreach(cell *c) {
   mushcursor2_put    (strn_cursor, *c);
   mushcursor2_advance(strn_cursor, MUSHCOORDS2(1,0));
   ++cells_length_n;
   return *c != 0;
}
static int cells_length(cell *c) {
   ++cells_length_n;
   return *c != 0;
}
