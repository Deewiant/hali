// This file is part of Hali.
// File created: 2012-11-01 13:19:46

STACK_TY STACK_FNAME(pop)(Stack*);
void STACK_FNAME(popN)(Stack*, size_t);
void STACK_FNAME(clear)(Stack*);
STACK_TY STACK_FNAME(top)(const Stack*);
void STACK_FNAME(push)(Stack*, STACK_TY);
size_t STACK_FNAME(size)(const Stack*);
int STACK_FNAME(empty)(const Stack*);
void STACK_FNAME(free)(Stack*);
void STACK_FNAME(foreach)(Stack*, int (*)(STACK_TY *));
void STACK_FNAME(foreachTopToBottom)(Stack*, int (*)(STACK_TY *));
void STACK_FNAME(foreachBottomToTop)(Stack*, int (*)(STACK_TY *));
STACK_TY *STACK_FNAME(reserve)(Stack*, size_t);
void STACK_FNAME(mapFirstN)(Stack*, size_t, void (*)(STACK_TY*,size_t), void (*)(size_t));
STACK_TY STACK_FNAME(at)(const Stack*, size_t);
STACK_TY STACK_FNAME(setAt)(Stack*, size_t, STACK_TY);
void STACK_FNAME(popNPushed)(Stack*, size_t);
STACK_TY STACK_FNAME(topPushed)(const Stack*);
void STACK_FNAME(mapFirstNPushed)(Stack*, size_t, void (*)(STACK_TY*,size_t), void (*)(size_t));
int STACK_FNAME(mode)(const Stack*);

#undef Stack
#undef STACK_FNAME
#undef STACK_TY
