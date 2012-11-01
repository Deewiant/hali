// This file is part of Sienest.
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
cell cc_pop(CellContainer*);
void cc_popN(CellContainer*, size_t);
void cc_clear(CellContainer*);
cell cc_top(const CellContainer*);
void cc_push(CellContainer*, cell);
size_t cc_size(const CellContainer*);
int cc_empty(const CellContainer*);
void cc_free(CellContainer*);
void cc_foreach(CellContainer*, int (*)(cell *));
void cc_foreachTopToBottom(CellContainer*, int (*)(cell *));
void cc_foreachBottomToTop(CellContainer*, int (*)(cell *));
cell *cc_reserve(CellContainer*, size_t);
void cc_mapFirstN(CellContainer*, size_t, void (*)(cell*,size_t), void (*)(size_t));
cell cc_at(const CellContainer*, size_t);
cell cc_setAt(CellContainer*, size_t, cell);
void cc_popNPushed(CellContainer*, size_t);
cell cc_topPushed(const CellContainer*);
void cc_mapFirstNPushed(CellContainer*, size_t, void (*)(cell*,size_t), void (*)(size_t));
int cc_mode(const CellContainer*);

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
