/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

/****add new states, based on the protocol****/
enum{
	INVALID = 0,
	VALID,
	DIRTY,
   MODIFIED,
   SHARED,
   EXCLUSIVE,
   SHARED_MODIFIED,
   SHARED_CLEAN,
};

enum {
   MSI = 0,
   MESI,
   DRAGON,
   NONE,
};

class cacheLine 
{
protected:
   ulong tag;
   ulong Flags;   // 0:invalid, 1:valid, 2:dirty 
   ulong seq;
 
public:
   cacheLine()            { tag = 0; Flags = 0; }
   ulong getTag()         { return tag; }
   ulong getFlags()			{ return Flags;}
   ulong getSeq()         { return seq; }
   void setSeq(ulong Seq)			{ seq = Seq;}
   void setFlags(ulong flags)			{  Flags = flags;}
   void setTag(ulong a)   { tag = a; }
   void invalidate()      { tag = 0; Flags = INVALID;}//useful function
   bool isValid()         { return ((Flags) != INVALID); }
};

class Bus;
class Cache
{
protected:
   ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;
   ulong reads,readMisses,writes,writeMisses,writeBacks;
   int policy;
   

   //******///
   //add coherence counters here///
   //******///
   ulong invalidations, interventions, flushes, busrdx;
   ulong c2c_transfers, mem_transactions;

   cacheLine **cache;
   Bus *bus;
   ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
   ulong calcIndex(ulong addr)  { return ((addr >> log2Blk) & tagMask);}
   ulong calcAddr4Tag(ulong tag)   { return (tag << (log2Blk));}
   
public:
   ulong currentCycle; 
   static int CacheCount;
   int CacheNum;
     
   Cache(int,int,int, int);
   ~Cache() { delete cache;}
   
   cacheLine *findLineToReplace(ulong addr);
   cacheLine *fillLine(ulong addr);
   cacheLine * findLine(ulong addr);
   cacheLine * getLRU(ulong);
   
   ulong getRM(){return readMisses;} ulong getWM(){return writeMisses;} 
   ulong getReads(){return reads;}ulong getWrites(){return writes;}
   ulong getWB(){return writeBacks;}
   
   void writeBack(ulong)   {writeBacks++;}
   void Access(ulong,uchar);
   void printStats();
   void updateLRU(cacheLine *);

   //******///
   //add other functions to handle bus transactions///
   //******///
   void IncC2c() {c2c_transfers++;};
   void IncBusRdX() {busrdx++;};
   void IncFlushes() {flushes++;};
   void BusReg(Bus *b);
   bool InvalidateLine(ulong addr);
   bool MakeShared(ulong addr);
   bool Update(ulong);

   void CacheTest();
};

class Bus {
private:
   int policy;
   int num_procs;
   int reg;

public:
   Cache *Caches[8];

   Bus(int, int);
   void BusReg(Cache *c);
   void BusTest();

   bool BusRd(int, ulong);
   bool BusRdX(int, ulong);
   void BusUpgr(int, ulong);
   void Flush(int, ulong);
   bool BusUpd(int, ulong);
};

#endif
