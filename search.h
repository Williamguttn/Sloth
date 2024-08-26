#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include <cstdarg>

#include "position.h"
#include "types.h"
#include "movegen.h"


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

        struct SearchStack {
            int ply;
            int staticEval;
        };

        extern int hashEntries;

        extern HASHE *hashTable;

        extern U64 repetitionTable[1000];
        extern int repetitionIndex;

        extern int ply;

        extern int bestMove;

        extern int contempt;

        void clearHashTable();
        void initHashTable(int mb);

        void printMoveScores(Movegen::MoveList* moveList, Position& pos);

        extern  int scoreMove(int move, Position& pos);
        extern  void sortMoves(Movegen::MoveList* moveList, int bestMove, Position& pos);

        extern  int negamax(int alpha, int beta, int depth, bool cutnode, Position& pos);

        void search(Position& pos, int depth);
    }
}
#endif
