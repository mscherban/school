//
// Created by msche on 9/17/2018.
//

#ifndef SIM_CACHE_CACHE_H
#define SIM_CACHE_CACHE_H


//#include <cstdint>

typedef struct cache_data {
    bool Valid;
    bool Dirty;
    unsigned long Tag;
    unsigned long Accessed;
} cache_data;

class Cache {
public:
    void DumpCacheContents();
    bool TryRead(unsigned long Addr);
    bool TryWrite(unsigned long Addr);
    void SetNextCache(Cache *Next);

    void Test(unsigned long addr);

    Cache(unsigned long BlockSize, unsigned long Size, unsigned long Assoc, unsigned long Victim);
    virtual ~Cache();

    /* Statistics */
    int NumReadHit;
    int NumReadMiss;
    int NumReads;
    int NumWriteHit;
    int NumWriteMiss;
    int NumWrites;
    int NumWriteBacks;
    int NumMemTraffic;
    int NumSwaps;
    int NumSwapRequests;

private:
    void VcSwapBlocks(unsigned long addr, unsigned long va, int CacheIndex, int LruBlock, int VcIndex);
    bool TryVcHit(unsigned long addr, unsigned long ev, int *assoc);
    bool TryHit(unsigned long Index, unsigned long Tag, int *a);
    int GetReplacementBlock(unsigned long Index);
    int GetReplacementVcBlock();
    void UpdateMru(unsigned long Index, int lru);
    void UpdateVcMru(unsigned long mru);

    static unsigned long CacheCount;
    int _Ln; /* Cache number */
    unsigned long _bs; /* block size internal */
    unsigned long _size; /* size internal */
    unsigned long _assoc; /* associativity internal */
    unsigned long _nsets; /* number of sets internal */
    unsigned long _vcn;

    unsigned long _omask; /* offset mask */
    unsigned long _oshift; /* offset shift */
    unsigned long _imask; /* index mask */
    unsigned long _ishift; /* index shift */
    unsigned long _tmask; /* tag mask */
    unsigned long _tshift; /* tag shift */
    unsigned long _vctmask; /* vc tag mask */
    unsigned long _vctshift; /* vc tag shift */


    Cache *NextCache;
    cache_data **_cache; /* cache data */
    cache_data *_vcache; /* victim cache */
};


#endif //SIM_CACHE_CACHE_H
