#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include <cstdarg>

#include "position.h"
#include "types.h"
#include "movegen.h"
#include "threads.h"


#undef ASSERT
#  define ASSERT(a)

static void my_free(void * address) {

   ASSERT(address!=NULL);

   free(address);
}


static void my_fatal(const char format[], ...) {

   va_list ap;

   ASSERT(format!=NULL);

   va_start(ap,format);
   vfprintf(stderr,format,ap);
   va_end(ap);

   exit(EXIT_FAILURE);
   // abort();
}


static void * my_malloc(int size) {

   void * address;

   ASSERT(size>0);

   address = malloc(size);
   if (address == NULL) my_fatal("my_malloc(): malloc(): %s\n",strerror(errno));

   return address;
}




namespace Sloth {

   namespace Search {

      extern HashEntry *hashTable;
      extern int hashEntries;
      extern int bestMove;
      extern int contempt;

      void clearHashTable();
      void initHashTable(int mb);
      void printMoveScores(Movegen::MoveList* moveList, Position& pos, Threads::ThreadData* threadData);

      extern  int negamax(int alpha, int beta, int depth, bool cutnode, Position& pos, Threads::ThreadData* threadData);
      extern void iterativeDeepen(Threads::ThreadData* threadData);
   }
}
#endif