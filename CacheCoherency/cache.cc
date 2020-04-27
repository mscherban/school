/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
#include <cstdio>
using namespace std;

int Cache::CacheCount = 0;

Cache::Cache(int s,int a,int b, int p )
{
   ulong i, j;
   reads = readMisses = writes = 0; 
   writeMisses = writeBacks = currentCycle = 0;
   policy = p;
   CacheNum = CacheCount++;

   invalidations=interventions=flushes=busrdx = 0;
   c2c_transfers=mem_transactions = 0;

   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);   
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b));   
  
   //*******************//
   //initialize your counters here//
   //*******************//
 
   tagMask =0;
   for(i=0;i<log2Sets;i++)
   {
		tagMask <<= 1;
        tagMask |= 1;
   }
   
   /**create a two dimentional cache, sized as cache[sets][assoc]**/ 
   cache = new cacheLine*[sets];
   for(i=0; i<sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for(j=0; j<assoc; j++) 
      {
	   cache[i][j].invalidate();
      }
   }
}



/**you might add other parameters to Access()
since this function is an entry point 
to the memory hierarchy (i.e. caches)**/
void Cache::Access(ulong addr,uchar op)
{
	currentCycle++;/*per cache global counter to maintain LRU order 
			among cache ways, updated on every cache access*/
        	
	if(op == 'w') writes++;
	else          reads++;
	
	cacheLine * line = findLine(addr);
	if(line == NULL)/*miss*/
	{
		if(op == 'w') writeMisses++;
		else readMisses++;

		cacheLine *newline = fillLine(addr);
      switch (policy) {
         case NONE:
            if(op == 'w') newline->setFlags(DIRTY);  
            break;
         case MSI:
            if (op == 'w') {
               newline->setFlags(MODIFIED);
               bus->BusRdX(CacheNum, addr);
            } else {
               newline->setFlags(SHARED);
               bus->BusRd(CacheNum, addr);
            }
            break;
         case MESI:
            if (op == 'w') {
               newline->setFlags(MODIFIED);
               if (bus->BusRdX(CacheNum, addr))
                  IncC2c();
            } else {
               if (bus->BusRd(CacheNum, addr)) { /* COPIES_EXIST */
                  newline->setFlags(SHARED);
                  IncC2c();
               } else { /* !COPIES_EXIST */
                  newline->setFlags(EXCLUSIVE);
               }
            }
            break;
         case DRAGON:
            if (op == 'w') {
               if (bus->BusRd(CacheNum, addr)) {
                  newline->setFlags(SHARED_MODIFIED);
                  bus->BusUpd(CacheNum, addr);
               } else {
                  newline->setFlags(MODIFIED);
               }
            } else {
               if (bus->BusRd(CacheNum, addr)) {
                  newline->setFlags(SHARED_CLEAN);
               } else {
                  newline->setFlags(EXCLUSIVE);
               }
            }
            break;
         default: break;
      }
		
	}
	else
	{
		/**since it's a hit, update LRU and update dirty flag**/
		updateLRU(line);
		switch (policy) {
         case NONE:
            if(op == 'w') line->setFlags(DIRTY);  
            break;
         case MSI:
            /* move from shared to modified if there is a write to the cache line */
            if (op == 'w' && line->getFlags() == SHARED) {
               line->setFlags(MODIFIED);
               bus->BusRdX(CacheNum, addr);
            }
            break;
         case MESI:
            if (op == 'w' && line->getFlags() == EXCLUSIVE) {
               line->setFlags(MODIFIED);
            } else if (op == 'w' && line->getFlags() == SHARED) {
               line->setFlags(MODIFIED);
               bus->BusUpgr(CacheNum, addr);
            }
            break;
         case DRAGON:
            if (op == 'w') {
               if (line->getFlags() == EXCLUSIVE) {
                  line->setFlags(MODIFIED);
               } else if (line->getFlags() == SHARED_CLEAN) {
                  if (bus->BusUpd(CacheNum, addr)) {
                     line->setFlags(SHARED_MODIFIED);
                  } else {
                     line->setFlags(MODIFIED);
                  }
               } else if (line->getFlags() == SHARED_MODIFIED) {
                  if (bus->BusUpd(CacheNum, addr)) {
                     line->setFlags(SHARED_MODIFIED);
                  } else {
                     line->setFlags(MODIFIED);
                  }
               }
            }
            break;
         default: break;
      }
	}
}

/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++)
	if(cache[i][j].isValid())
	        if(cache[i][j].getTag() == tag)
		{
		     pos = j; break; 
		}
   if(pos == assoc)
	return NULL;
   else
	return &(cache[i][pos]); 
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
  line->setSeq(currentCycle);  
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) return &(cache[i][j]);     
   }   
   for(j=0;j<assoc;j++)
   {
	 if(cache[i][j].getSeq() <= min) { victim = j; min = cache[i][j].getSeq();}
   } 
   assert(victim != assoc);
   
   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{ 
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);
   if(victim->getFlags() == DIRTY) writeBack(addr);
   else if (victim->getFlags() == MODIFIED) writeBack(addr);
   else if (victim->getFlags() == SHARED_MODIFIED) writeBack(addr);
    	
   tag = calcTag(addr);   
   victim->setTag(tag);
   victim->setFlags(VALID);    
   /**note that this cache line has been already 
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

bool Cache::InvalidateLine(ulong addr) {
   bool COPIES_EXIST = false;
   cacheLine *victim = findLine(addr);
   if (victim != NULL) {
      if (victim->getFlags() == MODIFIED) IncFlushes();
      if (victim->getFlags() == MODIFIED) writeBack(addr);
      victim->invalidate();
      invalidations++;
      COPIES_EXIST = true;
   }
   return COPIES_EXIST;
}

bool Cache::MakeShared(ulong addr) {
   bool COPIES_EXIST = false;
   cacheLine *victim = findLine(addr);
   if (victim != NULL && victim->getFlags() == MODIFIED && (policy == MSI || policy == MESI)) {
      IncFlushes();
      writeBack(addr);
      victim->setFlags(SHARED);
      interventions++;
   } else if (victim != NULL && victim->getFlags() == EXCLUSIVE && (policy == MSI || policy == MESI)) {
      //IncFlushes();
      //writeBack(addr);
      victim->setFlags(SHARED);
      interventions++;
   } else if (victim != NULL && victim->getFlags() == EXCLUSIVE && policy == DRAGON) {
      victim->setFlags(SHARED_CLEAN);
      interventions++;
   } else if (victim != NULL && victim->getFlags() == MODIFIED && policy == DRAGON) {
      victim->setFlags(SHARED_MODIFIED);
      IncFlushes();
      interventions++;
   } else if (victim != NULL && victim->getFlags() == SHARED_MODIFIED && policy == DRAGON) { 
      IncFlushes();
   }
   if (victim != NULL) COPIES_EXIST = true;
   return COPIES_EXIST;
}

bool Cache::Update(ulong addr) {
   bool COPIES_EXIST = false;
   cacheLine *victim = findLine(addr);
   if (victim != NULL) {
      victim->setFlags(SHARED_CLEAN);
   }

   if (victim != NULL) COPIES_EXIST = true;
   return COPIES_EXIST;
}

void Cache::printStats()
{
   int memxactadd = (policy==DRAGON)?writeMisses:busrdx;
	/****print out the rest of statistics here.****/
	/****follow the ouput file format**************/
   printf("============ Simulation results (Cache %d) ============\n", CacheNum);
   printf("01. number of reads:\t\t\t\t%lu\n", reads);
   printf("02. number of read misses:\t\t\t%lu\n", readMisses);
   printf("03. number of writes:\t\t\t\t%lu\n", writes);
   printf("04. number of write misses:\t\t\t%lu\n", writeMisses);
   printf("05. total miss rate:\t\t\t\t%.02f%%\n", ((float)(readMisses+writeMisses)/(reads+writes))*100);
   printf("06. number of writebacks:\t\t\t%lu\n", writeBacks);
   printf("07. number of cache-to-cache transfers:\t\t%lu\n", c2c_transfers);
   printf("08. number of memory transactions:\t\t%lu\n", readMisses+writeBacks-c2c_transfers+memxactadd);
   printf("09. number of interventions:\t\t\t%lu\n", interventions);
   printf("10. number of invalidations:\t\t\t%lu\n", invalidations);
   printf("11. number of flushes:\t\t\t\t%lu\n", flushes);
   printf("12. number of BusRdX:\t\t\t\t%lu\n", busrdx);

}

void Cache::BusReg(Bus *b) {
   bus = b;
}

void Cache::CacheTest() {
   //bus->BusTest();
}

Bus::Bus(int n, int p) {
   num_procs = n;
   policy = p;
   reg = 0;
}
void Bus::BusReg(Cache *c) {
   Caches[reg++] = c;
}

void Bus::BusTest() {
   for (int i = 0; i < reg; i++) {
         Caches[i]->printStats();
   }
}

bool Bus::BusRd(int proc, ulong addr) {
   bool COPIES_EXIST = false;
      for (int i = 0; i < reg; i++) {
      if (i != proc)
         if (Caches[i]->MakeShared(addr))
            COPIES_EXIST = true;
   }
   return COPIES_EXIST;
}

bool Bus::BusRdX(int proc, ulong addr) {
   bool COPIES_EXIST = false;
   Caches[proc]->IncBusRdX();
   for (int i = 0; i < reg; i++) {
      if (i != proc)
         if (Caches[i]->InvalidateLine(addr))
            COPIES_EXIST = true;
   }
   return COPIES_EXIST;
}

void Bus::BusUpgr(int proc, ulong addr) {
   for (int i = 0; i < reg; i++) {
      if (i != proc)
         Caches[i]->InvalidateLine(addr);
   }
}

void Bus::Flush(int proc, ulong addr) {
   Caches[proc]->IncFlushes();
}

bool Bus::BusUpd(int proc, ulong addr) {
   bool COPIES_EXIST = false;
   for (int i = 0; i < reg; i++) {
      if (i != proc) {
         if (Caches[i]->Update(addr))
            COPIES_EXIST = true;
      }
   }
   return COPIES_EXIST;
}
// bool Bus::CopiesExist(int proc, ulong addr) {
//    for (int i = 0; i < reg; i++) {
//       cacheLine *line;
//       if (i != proc) {
//          line = Caches[i]->findLine(addr);
//          if (line != NULL) return true;
//       }
//    }
//    return false;
// }