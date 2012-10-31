// This file is part of Sienest.
// Based on:
// ccbi.container from CCBI, the Conforming Concurrent Befunge-98 Interpreter
// Copyright (c) 2006-2010 Matti Niemenmaa
// See license.txt, which you should have received together with this file, for
// licensing information.

// File created: 2007-01-21 12:14:27

// The stack is, by default, a Stack instead of a Deque even though the MODE
// fingerprint needs a Deque.
//
// This is because a simple Stack is measurably faster when the full Deque
// functionality is not needed.

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "stack.h"

size_t STACK_FNAME(size) (const Stack *this) { return this->head; }
bool   STACK_FNAME(empty)(const Stack *this) { return this->head == 0; }

Stack STACK_FNAME(init)(size_t n) {
   Stack x;
   x.head     = 0;
   x.capacity = n;
   x.array    = malloc(x.capacity * sizeof *x.array);
   return x;
}

Stack STACK_FNAME(initFromStack)(Stack s) {
   Stack x;
   x.head     = STACK_FNAME(size)(&s);
   x.capacity = s.capacity;
   x.array    = malloc(x.capacity * sizeof *x.array);
   memcpy(x.array, s.array, x.head);
   return x;
}

#ifdef MODE
Stack STACK_FNAME(initFromDeque)(Deque q) {
   Stack x;

#ifdef STACK_TY_IS_CELL
   struct Array arr = deque_elementsBottomToTop(&q);
   x.head     = arr.length;
   x.capacity = arr.length;
   x.array    = arr.ptr;
#else
   (void)q;
   assert (false && "Trying to make non-cell stack out of deque");
#endif
   return x;
}
#endif

void STACK_FNAME(free)(Stack *this) { free(this->array); }

STACK_TY STACK_FNAME(pop)(Stack *this) {
   // not an error to pop an empty stack
   if (STACK_FNAME(empty)(this)) {
#ifdef STACK_TY_IS_CELL
      return 0;
#else
      assert (false && "Attempted to pop empty non-cell stack.");
#endif
   }
   return this->array[--this->head];
}

void STACK_FNAME(popN)(Stack *this, size_t i) {
   if (i >= this->head)
      this->head = 0;
   else
      this->head -= i;
}
void STACK_FNAME(popNPushed)(Stack *this, size_t n) {
   STACK_FNAME(popN)(this, n);
}

void STACK_FNAME(clear)(Stack *this) {
   this->head = 0;
}

STACK_TY STACK_FNAME(top)(const Stack *this) {
   if (STACK_FNAME(empty)(this)) {
#ifdef STACK_TY_IS_CELL
      return 0;
#else
      assert (false && "Attempted to peek empty non-cell stack.");
#endif
   }
   return this->array[this->head-1];
}
STACK_TY STACK_FNAME(topPushed)(const Stack *this) {
   assert (!STACK_FNAME(empty)(this));
   return this->array[this->head-1];
}

void STACK_FNAME(push)(Stack *this, STACK_TY t) {
   size_t neededRoom = this->head + 1;
   if (neededRoom > this->capacity) {
      this->capacity = 2 * this->capacity +
         (neededRoom > 2 * this->capacity ? neededRoom : 0);

      this->array = realloc(this->array, this->capacity * sizeof *this->array);
   }
   this->array[this->head++] = t;
}



STACK_TY* STACK_FNAME(reserve)(Stack *this, size_t n) {
   if (this->capacity < n + this->head) {
      this->capacity = n + this->head;
      this->array = realloc(this->array, this->capacity * sizeof *this->array);
   }

   STACK_TY *ptr = &this->array[this->head];

   this->head += n;
   assert (this->head <= this->capacity);

   return ptr;
}

STACK_TY STACK_FNAME(at)(const Stack *this, size_t i) {
   return this->array[i];
}
STACK_TY STACK_FNAME(setAt)(Stack *this, size_t i, STACK_TY t) {
   return this->array[i] = t;
}

void STACK_FNAME(mapFirstN)(
   Stack *this, size_t n, void (*f)(STACK_TY*, size_t), void (*g)(size_t))
{
   if (n <= this->head)
      f(&this->array[this->head - n], n);
   else {
      g(n - this->head);
      f(this->array, this->head);
   }
}
void STACK_FNAME(mapFirstNHead)(
   Stack *this, size_t n, void (*f)(STACK_TY*, size_t), void (*g)(size_t))
{
   STACK_FNAME(mapFirstN)(this, n, f, g);
}
void STACK_FNAME(mapFirstNPushed)(
   Stack *this, size_t n, void (*f)(STACK_TY*, size_t), void (*g)(size_t))
{
   STACK_FNAME(mapFirstN)(this, n, f, g);
}

void STACK_FNAME(foreach)(Stack *this, int (*f)(STACK_TY*)) {
   for (size_t i = 0; i < this->head; ++i)
      if (!f(&this->array[i]))
         break;
}
void STACK_FNAME(foreachTopToBottom)(Stack *this, int (*f)(STACK_TY*)) {
   for (size_t i = this->head; i-- > 0;)
      if (!f(&this->array[i]))
         break;
}
void STACK_FNAME(foreachBottomToTop)(Stack *this, int (*f)(STACK_TY*)) {
   STACK_FNAME(foreach)(this, f);
}

#undef Stack
#undef STACK_FNAME
#undef STACK_TY
#undef STACK_TY_IS_CELL
