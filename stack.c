// This file is part of Sienest.
// Based on:
// ccbi.container from CCBI, the Conforming Concurrent Befunge-98 Interpreter
// Copyright (c) 2006-2010 Matti Niemenmaa
// See license.txt, which you should have received together with this file, for
// licensing information.

// File created: 2010-05-13 17:04:33

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// Docs and main API at the bottom thanks to C

#define Stack Stack_cell
#define STACK_FNAME(s) stack_cell_##s
#define STACK_TY cell
#define STACK_TY_IS_CELL
#include "stack_impl.inc.c"

#define Stack Stack_stack
#define STACK_FNAME(s) stack_stack_##s
#define STACK_TY CellContainer*
#include "stack_impl.inc.c"

// Deque and CellContainer like to have these
#define STACK_FNAME(s) stack_cell_##s
#define Stack Stack_cell

#ifdef MODE

size_t max(size_t a, size_t b) { return a > b ? a : b; }

static const size_t
   DEFAULT_DEQUE_SIZE = 0100,
   NEW_TAIL_SIZE      = 020000;

/////////// DEQUE IMPL until #endif

// Helpers first because that's how C rolls

static size_t chunk_size(const Chunk *c) { return c->head - c->tail; }

static bool deque_empty(const Deque *this) {
   return this->head == this->tail && this->head->head <= this->head->tail;
}
static size_t deque_size(const Deque *this) {
   size_t n = 0;
   for (const Chunk *c = this->tail; c; c = c->next)
      n += chunk_size(c);
   return n;
}

static void deque_newTailChunk(Deque *this, size_t minSize) {

   if (chunk_size(this->tail) == 0) {
      // Just resize the existing tail.

      // We shouldn't get into this situation unless something has detected
      // the capacity to be insufficient...
      assert (this->tail->capacity < minSize);

      this->tail->head = this->tail->tail = this->tail->capacity = minSize;
      this->tail->array = realloc(
         this->tail->array, this->tail->capacity * sizeof *this->tail->array);
      return;
   }

   if (this->tail->prev) {
      // We have an extra chunk ready and waiting: use that
      Chunk *prev = this->tail->prev;
      if (prev->capacity < minSize) {
         prev->capacity = minSize;
         prev->array =
            realloc(prev->array, prev->capacity * sizeof *prev->array);
      }
      prev->head = prev->tail = prev->capacity;
      this->tail = this->tail->prev;
      return;
   }

   Chunk *c = malloc(sizeof *c);
   c->head = c->tail = c->capacity = max(minSize, NEW_TAIL_SIZE);
   c->array = malloc(c->capacity * sizeof *c->array);
   c->prev = NULL;
   c->next = this->tail;
   this->tail = this->tail->prev = c;
}
static bool deque_dropHeadChunk(Deque *this) {
   if (this->head == this->tail) {
      this->head->head = this->head->tail = 0;
      return false;
   }

   Chunk *c = this->head;
   this->head = this->head->prev;
   this->head->next = NULL;

   free(c->array);
   free(c);
   return true;
}
static bool deque_dropTailChunk(Deque *this) {
   if (this->head == this->tail) {
      this->tail->head = this->tail->tail = this->tail->capacity;
      return false;
   }

   // Keep the old tail in case we'll be needing it soon, but leave at most
   // one unused tail chunk
   if (this->tail->prev) {
      free(this->tail->prev->array);
      free(this->tail->prev);

      this->tail->prev = NULL;
   }
   this->tail->head = this->tail->tail = this->tail->capacity;
   this->tail = this->tail->next;
   return true;
}

static Deque deque_init(size_t n) {
   Deque x;
   x.mode = 0;
   x.head = x.tail = malloc(sizeof *x.head);

   x.head->capacity = n;
   x.head->array = malloc(n * sizeof *x.head->array);
   x.head->head = x.head->tail = 0;
   x.head->next = x.head->prev = NULL;

   return x;
}

Deque deque_initFromDeque(Deque q) {
   Deque x;
   x.mode = q.mode;

   x.tail = malloc(sizeof *x.tail);

   // We don't care about q.tail->prev even if it exists
   x.tail->prev = NULL;

   for (Chunk *qc = q.tail, *c = x.tail;;) {
      c->head = qc->head;
      c->tail = qc->tail;
      c->capacity = qc->capacity;

      c->array = malloc(c->capacity * sizeof *c->array);
      memcpy(&c->array[c->tail], &qc->array[c->tail], c->tail - c->head);

      if (qc == q.head) {
         c->next = NULL;
         x.head = c;
         break;
      }
      c->next = malloc(sizeof *c->next);
      c->next->prev = c;
      c  =  c->next;
      qc = qc->next;
   }
   return x;
}
Deque deque_initFromStack(Stack s) {
   Deque x;
   x.mode = 0;
   x.head = x.tail = malloc(sizeof *x.tail);

   x.head->capacity = max(STACK_FNAME(size)(&s), DEFAULT_DEQUE_SIZE);
   x.head->array = malloc(x.head->capacity * sizeof *x.head->array);
   memcpy(x.head->array, s.array, STACK_FNAME(size)(&s));
   x.head->tail = 0;
   x.head->head = STACK_FNAME(size)(&s);
   x.head->next = x.head->prev = NULL;

   return x;
}

static void deque_free(Deque *this) {
   free(this->tail->array);
   if (this->tail->prev) {
      free(this->tail->prev->array);
      free(this->tail->prev);
   }
   if (this->tail->next) for (Chunk *c = this->tail->next;;) {
      free(c->array);
      free(c->prev);
      if (c->next)
         c = c->next;
      else {
         free(c);
         break;
      }
   } else
      free(this->tail);
   this->head = this->tail = NULL;
}

static cell deque_popHead(Deque *this) {
   cell c = this->head->array[--this->head->head];

   if (this->head->head <= this->head->tail)
      deque_dropHeadChunk(this);

   return c;
}
static cell deque_popTail(Deque *this) {
   cell c = this->tail->array[this->tail->tail++];

   if (this->tail->tail >= this->tail->head)
      deque_dropTailChunk(this);

   return c;
}
static cell deque_pop(Deque *this) {
   if (deque_empty(this))
      return 0;

   if (this->mode & QUEUE_MODE)
      return deque_popTail(this);
   else
      return deque_popHead(this);
}

static size_t deque_popHeadN(Deque *this, size_t i) {
   for (;;) {
      if (i <= chunk_size(this->head)) {
         this->head->head -= i;
         return 0;
      }
      i -= chunk_size(this->head);
      if (!deque_dropHeadChunk(this))
         return i;
   }
}
static size_t deque_popTailN(Deque *this, size_t i) {
   for (;;) {
      if (i < chunk_size(this->tail)) {
         this->tail->tail += i;
         return 0;
      }
      i -= chunk_size(this->tail);
      if (!deque_dropTailChunk(this))
         return i;
   }
}
static void deque_popN(Deque *this, size_t i)  {
   if (this->mode & QUEUE_MODE)
      deque_popTailN(this, i);
   else
      deque_popHeadN(this, i);
}
static void deque_popNPushed(Deque *this, size_t n) {
   if (this->mode & INVERT_MODE)
      deque_popTailN(this, n);
   else
      deque_popHeadN(this, n);
}

static void deque_clear(Deque *this) {
   // Drop back down to one chunk, which might as well be the current tail->
   //
   // Note that we might still have a tail->prev alive, which is fine.
   if (this->head != this->tail) {
      Chunk *c = this->tail->next;

      free(c->array);

      for (c = c->next;;) {
         free(c->array);
         free(c->prev);
         if (c->next)
            c = c->next;
         else {
            free(c);
            break;
         }
      }
      this->tail->next = NULL;
      this->head = this->tail;
   }
   this->tail->head = this->tail->tail = 0;
}

static cell deque_top(const Deque *this) {
   if (deque_empty(this))
      return 0;

   if (this->mode & QUEUE_MODE)
      return this->tail->array[this->tail->tail];
   else
      return this->head->array[this->head->head-1];
}
static cell deque_topPushed(const Deque *this) {
   assert (!deque_empty(this));

   if (this->mode & INVERT_MODE)
      return this->tail->array[this->tail->tail];
   else
      return this->head->array[this->head->head-1];
}

static void deque_pushHead(Deque *this, cell c) {
   size_t newHead = this->head->head + 1;
   if (newHead > this->head->capacity) {
      this->head->capacity = 2 * this->head->capacity +
         (newHead > 2 * this->head->capacity ? newHead :  0);

      this->head->array = realloc(
         this->head->array, this->head->capacity * sizeof *this->head->array);
   }
   this->head->array[this->head->head++] = c;
}
static void deque_pushTail(Deque *this, cell c) {

   size_t newTail = this->tail->tail - 1;
   if (newTail > this->tail->capacity) {
      if (this->head == this->tail && chunk_size(this->head) == 0
       && 1 <= this->head->capacity)
      {
         // We can fixup the position in the chunk instead of having to
         // resort to resizing
         this->head->head = this->head->tail = max(1, this->head->capacity/2);

      } else if (this->tail->tail == 0)
         deque_newTailChunk(this, 1);
   }
   this->tail->array[--this->tail->tail] = c;
}
static void deque_push(Deque *this, cell c) {
   if (this->mode & INVERT_MODE)
      deque_pushTail(this, c);
   else
      deque_pushHead(this, c);
}

static cell* deque_reserveHead(Deque *this, size_t n) {

   size_t newHead = this->head->head + n;
   if (this->head->capacity < newHead) {
      this->head->capacity = newHead;
      this->head->array = realloc(
         this->head->array, this->head->capacity * sizeof *this->head->array);
   }

   cell *ptr = &this->head->array[this->head->head];
   this->head->head = newHead;
   return ptr;
}
static cell* deque_reserveTail(Deque *this, size_t n) {

   // Tricky.

   // If it fits in the tail chunk directly, just give that.
   {size_t newTail = this->tail->tail - n;
   if (newTail < this->tail->capacity) {
      this->tail->tail = newTail;
      return &this->tail->array[this->tail->tail];
   }}

   if (this->head == this->tail && chunk_size(this->head) == 0
    && n <= this->head->capacity)
   {
      // Just fixup the position in the chunk
      this->head->head = max(n, this->head->capacity / 2);
      this->head->tail = this->head->head - n;
      return &this->head->array[this->head->tail];
   }

   const size_t EXPENSIVE_RESIZE_LIMIT = NEW_TAIL_SIZE * 32;

   // If tail is small enough, resize it. More expensive than resizing the
   // head because we need to move the data to the right place. Thus also
   // pad it out to avoid having to resize it again in the near future.
   if (chunk_size(this->tail) <= EXPENSIVE_RESIZE_LIMIT) {

      Chunk *tail = this->tail;

      tail->capacity = 2 * tail->capacity + (n > 2 * tail->capacity ? n : 0);
      tail->array = realloc(tail->array, tail->capacity * sizeof *tail->array);

      if (this->head != this->tail) {
         // Need to move the data to the end
         size_t size = chunk_size(tail);
         size_t cap  = tail->capacity;
         if (cap - size < tail->head)
            memcpy (&tail->array[cap - size], &tail->array[tail->tail], size);
         else
            memmove(&tail->array[cap - size], &tail->array[tail->tail], size);

         tail->tail = cap - size;
         tail->head = cap;

      } else {
         // Moving to the very end of the capacity might not be what we
         // want... leave an equal amount of free space at the beginning and
         // the end
         size_t size = chunk_size(tail);
         size_t cap  = tail->capacity;

         size_t space = (cap - (size + n)) / 2;
         size_t odd   = (cap - (size + n)) % 2;

         size_t oldTailTgt = space + n + odd;

         if (tail->head <= space + n)
            memcpy (&tail->array[oldTailTgt], &tail->array[tail->tail], size);
         else
            memmove(&tail->array[oldTailTgt], &tail->array[tail->tail], size);

         tail->tail = oldTailTgt - n;
         tail->head = cap - space;
      }
      assert (chunk_size(tail) >= n);
      return &tail->array[tail->tail];
   }

   // If there is a tail->next and tail and tail->next are small enough,
   // resize tail->next (expensive again), copy tail into tail->next, and use
   // the now-empty tail->
   if (this->tail->next
    && chunk_size(this->tail) <= EXPENSIVE_RESIZE_LIMIT/2
    && chunk_size(this->tail->next) <= EXPENSIVE_RESIZE_LIMIT/2)
   {
      Chunk *tail = this->tail;

      tail->next->capacity = 2 * tail->next->capacity +
         (chunk_size(tail) > 2 * tail->next->capacity ? chunk_size(tail) : 0);

      tail->next->array = realloc(
         tail->next->array, tail->next->capacity * sizeof *tail->next->array);

      Chunk *next = tail->next;
      size_t ncap  = next->capacity;
      size_t nsize = chunk_size(next);

      if (next->head <= ncap - nsize)
         memcpy (&next->array[ncap - nsize], &next->array[next->tail], nsize);
      else
         memmove(&next->array[ncap - nsize], &next->array[next->tail], nsize);

      next->tail = ncap - nsize;
      next->head = ncap;

      memcpy(next->array, tail->array, chunk_size(tail));

      if (tail->capacity < n) {
         tail->capacity = 2 * tail->capacity + (n > 2*tail->capacity ? n : 0);
         tail->array =
            realloc(tail->array, tail->capacity * sizeof *tail->array);
      }
      tail->tail = tail->capacity - n;
      tail->head = tail->capacity;
      return &tail->array[tail->tail];
   }

   // Tail has no suitable neighbour and/or is too large to justify the
   // expensive size increasing: instead shrink tail's capacity to its
   // current size and use a different chunk for the reserve request.
   Chunk *tail = this->tail;

   // First move the data to the beginning of tail so that the realloc
   // doesn't lose any of it.
   if (tail->tail > 0) {
      if (chunk_size(tail) <= tail->tail)
         memcpy (tail->array, &tail->array[tail->tail], chunk_size(tail));
      else
         memmove(tail->array, &tail->array[tail->tail], chunk_size(tail));
   }

   tail->capacity = chunk_size(tail);
   tail->array = realloc(tail->array, tail->capacity * sizeof *tail->array);

   deque_newTailChunk(this, n);

   tail->tail = tail->capacity - n;
   return &tail->array[tail->tail];
}
static cell* deque_reserve(Deque *this, size_t n) {
   if (this->mode & INVERT_MODE)
      return deque_reserveTail(this, n);
   else
      return deque_reserveHead(this, n);
}

static cell deque_at(const Deque *this, size_t i) {
   if (this->mode & QUEUE_MODE)
      i = deque_size(this) - 1 - i;

   for (const Chunk *c = this->tail;; c = c->next) {
      if (i < chunk_size(c))
         return c->array[c->tail + i];
      else
         i -= chunk_size(c);
   }
}
static cell deque_setAt(Deque *this, size_t i, cell x) {
   if (this->mode & QUEUE_MODE)
      i = deque_size(this) - 1 - i;

   for (Chunk *c = this->tail;; c = c->next) {
      if (i < chunk_size(c))
         return c->array[c->tail + i] = x;
      else
         i -= chunk_size(c);
   }
}

static void deque_mapFirstNHead(
   Deque *this, size_t n, void (*f)(cell*, size_t), void (*g)(size_t))
{
   if (n <= chunk_size(this->head)) {
      f(&this->head->array[this->head->head - n], n);
      return;
   }

   // Didn't fit into the head chunk... since we want to map from tail to
   // head, find the tailmost relevant chunk and the start position in it.
   Chunk *tailMost = this->head->prev;
   n -= chunk_size(this->head);
   while (tailMost && n > chunk_size(tailMost)) {
      n -= chunk_size(tailMost);
      tailMost = tailMost->prev;
   }

   if (!tailMost) {
      // Ran out of chunks: underflow by n
      g(n);
      tailMost = this->tail;
   } else if (n > 0) {
      // Didn't run out of chunks but want only n out of the last one
      f(&tailMost->array[tailMost->head], n);
      tailMost = tailMost->next;
   }

   do {
      f(&tailMost->array[tailMost->tail], chunk_size(tailMost));
      tailMost = tailMost->next;
   } while (tailMost);
}
static void deque_mapFirstNTail(
   Deque *this, size_t n, void (*f)(cell*, size_t), void (*g)(size_t))
{
   for (Chunk *c = this->tail; c; c = c->next) {
      if (n <= chunk_size(c)) {
         f(&c->array[c->tail], n);
         return;
      }

      f(&c->array[c->tail], chunk_size(c));
      n -= chunk_size(c);
   }
   g(n);
}
static void deque_mapFirstN(
   Deque *this, size_t n, void (*f)(cell*, size_t), void (*g)(size_t))
{
   if (this->mode & QUEUE_MODE)
      deque_mapFirstNTail(this, n, f, g);
   else
      deque_mapFirstNHead(this, n, f, g);
}
static void deque_mapFirstNPushed(
   Deque *this, size_t n, void (*f)(cell*, size_t), void (*g)(size_t))
{
   if (this->mode & INVERT_MODE)
      deque_mapFirstNTail(this, n, f, g);
   else
      deque_mapFirstNHead(this, n, f, g);
}

static void deque_foreach(Deque *this, int (*f)(cell*)) {
   for (Chunk *ch = this->tail; ch; ch = ch->next)
      for (size_t i = ch->tail; i < ch->head; ++i)
         if (!f(&ch->array[i]))
            break;
}
static void deque_foreachTopToBottom(Deque *this, int (*f)(cell*)) {
   for (Chunk *ch = this->head; ch; ch = ch->prev)
      for (size_t i = ch->head; i-- > ch->tail;)
         if (!f(&ch->array[i]))
            break;
}
static void deque_foreachBottomToTop(Deque *this, int (*f)(cell*)) {
   deque_foreach(this, f);
}

static struct Array deque_elementsBottomToTop(const Deque *this) {
   struct Array elems;
   elems.length = deque_size(this);
   elems.ptr    = malloc(elems.length * sizeof *elems.ptr);

   cell *p = elems.ptr;
   for (Chunk *c = this->tail; c; c = c->next) {
      memcpy(p, &c->array[c->tail], chunk_size(c));
      p += chunk_size(c);
   }
   return elems;
}
#endif

/////////// CELLCONTAINER

#ifdef MODE
#define GET_STACK(cc) ((cc).u.stack)
#else
#define GET_STACK(cc) (cc)
#endif

CellContainer cc_init(int isDeque) {
   CellContainer cc;
#ifdef MODE
   cc.isDeque = isDeque;
   if (isDeque)
      cc.u.deque = deque_init(DEFAULT_DEQUE_SIZE);
   else
#else
   assert (!isDeque && "Cannot make a Deque when MODE is not defined!");
#endif
      GET_STACK(cc) = STACK_FNAME(init)(0100);
   return cc;
}

cell cc_pop(CellContainer *this) {
#ifdef MODE
   if (this->isDeque)
      return deque_pop(&this->u.deque);
   else
#endif
      return STACK_FNAME(pop)(&GET_STACK(*this));
}

// pop n elements, ignoring their values
void cc_popN(CellContainer *this, size_t n) {
#ifdef MODE
   if (this->isDeque)
      deque_popN(&this->u.deque, n);
   else
#endif
      STACK_FNAME(popN)(&GET_STACK(*this), n);
}

void cc_clear(CellContainer *this) {
#ifdef MODE
   if (this->isDeque)
      deque_clear(&this->u.deque);
   else
#endif
      STACK_FNAME(clear)(&GET_STACK(*this));
}

cell cc_top(const CellContainer *this) {
#ifdef MODE
   if (this->isDeque)
      return deque_top(&this->u.deque);
   else
#endif
      return STACK_FNAME(top)(&GET_STACK(*this));
}

void cc_push(CellContainer *this, cell c) {
#ifdef MODE
   if (this->isDeque)
      deque_push(&this->u.deque, c);
   else
#endif
      STACK_FNAME(push)(&GET_STACK(*this), c);
}

size_t cc_size(const CellContainer *this) {
#ifdef MODE
   if (this->isDeque)
      return deque_size(&this->u.deque);
   else
#endif
      return STACK_FNAME(size)(&GET_STACK(*this));
}
int cc_empty(const CellContainer *this) {
#ifdef MODE
   if (this->isDeque)
      return deque_empty(&this->u.deque);
   else
#endif
      return STACK_FNAME(empty)(&GET_STACK(*this));
}

void cc_free(CellContainer *this) {
#ifdef MODE
   if (this->isDeque)
      deque_free(&this->u.deque);
   else
#endif
      STACK_FNAME(free)(&GET_STACK(*this));
}

void cc_foreach(CellContainer *this, int (*f)(cell*)) {
#ifdef MODE
   if (this->isDeque)
      deque_foreach(&this->u.deque, f);
   else
#endif
      STACK_FNAME(foreach)(&GET_STACK(*this), f);
}
void cc_foreachTopToBottom(CellContainer *this, int (*f)(cell*)) {
#ifdef MODE
   if (this->isDeque)
      deque_foreachTopToBottom(&this->u.deque, f);
   else
#endif
      STACK_FNAME(foreachTopToBottom)(&GET_STACK(*this), f);
}
void cc_foreachBottomToTop(CellContainer *this, int (*f)(cell*)) {
#ifdef MODE
   if (this->isDeque)
      deque_foreachBottomToTop(&this->u.deque, f);
   else
#endif
      STACK_FNAME(foreachBottomToTop)(&GET_STACK(*this), f);
}

// Abstraction-breaking stuff

// Makes sure that there's capacity for at least the given number of cells.
// Modifies size, but doesn't guarantee any values for the reserved
// elements.
//
// Returns a pointer to the top of the array overlaying the storage that
// backs this Container, guaranteeing that following the pointer there is
// space for least the given number of Ts.
//
// Altogether, this is an optimization on top of "push".
//
// Beware invertmode! If invertmode is enabled, you should fill the data
// after the pointer appropriately (i.e. in reverse order)!
cell* cc_reserve(CellContainer *this, size_t n) {
#ifdef MODE
   if (this->isDeque)
      return deque_reserve(&this->u.deque, n);
   else
#endif
      return STACK_FNAME(reserve)(&GET_STACK(*this), n);
}

// Calls the first given function over consecutive sequences of the top n
// elements, whose union is the top n elements. Traverses bottom-to-top.
//
// If n is greater than the size, instead first calls the second given
// function with the difference of n and the size, then proceeds to call the
// first function as though the size had been passed as n.
//
// Altogether, this is an optimization on top of "pop".
//
// But once again, beware modes! In queuemode, you will get the /bottom/ n
// elements, still in bottom-to-top order, and the second function will be
// called /after/ the first with the size difference, not before!
void cc_mapFirstN(
   CellContainer *this, size_t n,
   void (*f)(cell*, size_t), void (*g)(size_t))
{
#ifdef MODE
   if (this->isDeque)
      deque_mapFirstN(&this->u.deque, n, f, g);
   else
#endif
      STACK_FNAME(mapFirstN)(&GET_STACK(*this), n, f, g);
}

// at(x) gives the x'th element from the bottom without allocating.
//
// Another optimization on top of "pop".
//
// And as usual: in queuemode, you'll get the x'th element from the top, not
// the bottom.
cell cc_at(const CellContainer *this, size_t i) {
#ifdef MODE
   if (this->isDeque)
      return deque_at(&this->u.deque, i);
   else
#endif
      return STACK_FNAME(at)(&GET_STACK(*this), i);
}

// setAt(x,c) is the writing form of at(x): an optimization on top of both
// "pop" and "push".
cell cc_setAt(CellContainer *this, size_t i, cell c) {
#ifdef MODE
   if (this->isDeque)
      return deque_setAt(&this->u.deque, i, c);
   else
#endif
      return STACK_FNAME(setAt)(&GET_STACK(*this), i, c);
}

// *Pushed() forms take invertmode into account, applying from the correct
// direction so as to work on cells that have just been pushed. They assume a
// nonempty stack.
void cc_popNPushed(CellContainer *this, size_t n) {
#ifdef MODE
   if (this->isDeque)
      deque_popNPushed(&this->u.deque, n);
   else
#endif
      STACK_FNAME(popNPushed)(&GET_STACK(*this), n);
}
cell cc_topPushed(const CellContainer *this) {
#ifdef MODE
   if (this->isDeque)
      return deque_topPushed(&this->u.deque);
   else
#endif
      return STACK_FNAME(topPushed)(&GET_STACK(*this));
}
void cc_mapFirstNPushed(
   CellContainer *this, size_t n,
   void (*f)(cell*, size_t), void (*g)(size_t))
{
#ifdef MODE
   if (this->isDeque)
      deque_mapFirstNPushed(&this->u.deque, n, f, g);
   else
#endif
      STACK_FNAME(mapFirstNPushed)(&GET_STACK(*this), n, f, g);
}

int cc_mode(const CellContainer *this) {
   (void)this;
   return
#ifdef MODE
      this->isDeque ? this->u.deque.mode :
#endif
      0;
}
