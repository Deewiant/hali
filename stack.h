// This file is part of Hali.
// File created: 2010-05-13 18:06:46

#ifndef STACK_H
#define STACK_H

typedef long cell;

typedef struct { cell* array; size_t capacity; size_t head; } Stack_cell;

struct Chunk;

typedef struct {
   // tail may have a nonnull prev and head may have a nonnull next: this is
   // so that if we keep pop/pushing one cell at a chunk boundary we don't
   // have to constantly reallocate.
   struct Chunk *head, *tail;

   unsigned char mode;
} Deque;

#ifdef MODE
typedef struct {
   unsigned char isDeque;
   union {
      Stack_cell stack;
      Deque      deque;
   } u;
} CellContainer;
#else
typedef Stack_cell CellContainer;
#endif

typedef struct { CellContainer** array; size_t capacity; size_t head; }
               Stack_stack;

CellContainer cc_init(int isDeque);

#ifndef Stack
#define Stack CellContainer
#define STACK_FNAME(s) cc_##s
#define STACK_TY cell
#include "stack.inc.h"

Stack_stack stack_stack_init(size_t n);

#define Stack Stack_stack
#define STACK_FNAME(s) stack_stack_##s
#define STACK_TY CellContainer*
#include "stack.inc.h"
#endif

#ifdef MODE
enum { INVERT_MODE = 1 << 0, QUEUE_MODE = 1 << 1 };

// Chunk-style implementation: keeps a doubly linked list of chunks, each of
// which contains an array of data. Grows forwards as a stack and backwards as
// a queue.
//
// Abnormally, new chunks are only created when growing backwards: when growing
// forwards, the headmost chunk is merely resized.
typedef struct Chunk {
   cell *array;     // Typically not resizable
   size_t capacity; // Typically constant

   // head: the index of one past the topmost value: (0, capacity]
   // tail: the index of the bottommost value:       [0,capacity)
   //
   // Note that head can be zero and tail can be the
   // capacity when the chunk is empty.
   size_t head, tail;

   struct Chunk *next, *prev;
} Chunk;

struct Array {
   size_t length;
   cell *ptr;
};

static struct Array deque_elementsBottomToTop(const Deque *);
#endif

#endif
