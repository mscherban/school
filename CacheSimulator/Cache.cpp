//
// Created by msche on 9/17/2018.
//

#include <cstdio>
#include "Cache.h"
#include "sim_cache.h"

unsigned long Cache::CacheCount = 0;

Cache::~Cache() {
    if (_size > 0) {
//        printf("Destroying L%i Cache\n", _Ln);
        for (unsigned int i = 0; i < _nsets; i++) {
            delete[] _cache[i];
        }
        delete[] _cache;

        if (_vcn > 0) {
            delete[] _vcache;
        }
    }

}

Cache::Cache(unsigned long BlockSize, unsigned long Size, unsigned long Assoc, unsigned long Victim) {
    unsigned long i, cnt;

    NextCache = NULL;
    _bs = BlockSize;
    _assoc = Assoc;
    _size = Size;
    _Ln = (int)CacheCount++ + 1;

    NumReadHit = 0;
    NumReadMiss = 0;
    NumWriteHit = 0;
    NumWriteMiss = 0;
    NumReads = 0;
    NumWrites = 0;
    NumWriteBacks = 0;
    NumMemTraffic = 0;
    NumSwaps = 0;
    NumSwapRequests = 0;


    if (_size == 0)
        return;

//    printf("Initializing L%i Cache\n", _Ln);

    /* Get the offset mask and shift of fields bits*/
    cnt = 0;
    for (i = 1; i < BlockSize; i <<= 1) /* this is a very crude way of doing log2(x) */
        cnt++;
    _oshift = 0; /* Offset bits are 0 shift */
    _ishift = cnt; /* index bits are shifted the amount of offset bits there are */
    _omask = i - 1; /* Offset gets its mask from blocksize */
    _tmask = cnt;

    _nsets = Size / (Assoc * BlockSize);
    cnt = 0;
    for (i = 1; i < _nsets; i <<= 1)
        cnt++;
    _tshift = _oshift + _ishift + cnt;
    _imask = i - 1;
    _tmask += cnt;
    _tmask = 32 - _tmask;
    _tmask = (unsigned long)(1 << _tmask) - 1;

    /* allocate all cache sets */
    _cache = new cache_data*[_nsets];
    for (i = 0; i < _nsets; i++)
    {
        _cache[i] = new cache_data[_assoc];
    }

    /* allocate all blocks in each cache set */
    for (unsigned int j = 0; j < _nsets; j++) {
        for (i = 0; i < _assoc; i++) {
            _cache[j][i].Valid = false;
            _cache[j][i].Dirty = false;
            _cache[j][i].Tag = 0;
            _cache[j][i].Accessed = i;
        }
    }

    _vcn = Victim;
    if (_vcn > 0) /* if there is a victim cache.... */
    {
        /* VC and cache share the same block size, also fully associative so
         * there are no index bits*/
        cnt = 0;
        for (i = 1; i < BlockSize; i <<= 1)
            cnt++;
        _vctshift = cnt;
        _vctmask = (unsigned long)(1 << (32 - cnt)) - 1;
        _vcache = new cache_data[_vcn];
        for (i = 0; i < _vcn; i++) {
            _vcache[i].Valid = false;
            _vcache[i].Dirty = false;
            _vcache[i].Tag = 0;
            _vcache[i].Accessed = i;
        }
    }

//    printf("  Tag mask = %lx, Tag shift = %lu\n", _tmask, _tshift);
//    printf("  Index mask = %lx, Index shift = %lu\n", _imask, _ishift);
//    printf("  Offset mask = %lx, Offset shift = %lu\n", _omask, _oshift);
}

void Cache::Test(unsigned long addr) {
    unsigned long Offset;
    unsigned long Index;
    unsigned long Tag;

    Offset = (addr >> _oshift) & _omask;
    Index = (addr >> _ishift) & _imask;
    Tag = (addr >> _tshift) & _tmask;

    printf("Offset: %lx, Index: %lx, Tag: %lx\n", Offset, Index, Tag);
}

/* sets the next cache in the hierarchy. if there is none it is main memory */
void Cache::SetNextCache(Cache *Next) {
    if (Next->_size > 0) {
        this->NextCache = Next;
    }
}

void Cache::DumpCacheContents() {
    unsigned int i, j, k;

    if (_size == 0 )
        return;

    /* go through cache printing all blocks from MRU to LRU */
    printf("===== L%i contents =====\r\n", _Ln);
    for (i = 0; i < _nsets; i++) {
        printf("  set %3i: ", i);
        for (j = 0; j < _assoc; j++) {
            for (k = 0; k < _assoc; k++) {
                if (_cache[i][k].Accessed == j) {
                    if (_cache[i][k].Valid)
                        printf("%8lx %s", _cache[i][k].Tag, (_cache[i][k].Dirty) ? "D" : " ");
                    break;
                }
            }
        }
        printf("\n");
    };

    printf("\n");

    if (_vcn > 0) {
        printf("===== VC contents =====\r\n");
        printf("  set %3i: ", 0);
        for (i = 0; i < _vcn; i++) {
            for (j = 0; j < _vcn; j++) {
                if (_vcache[j].Accessed == i) {
                    if (_vcache[j].Valid)
                        printf("%8lx %s", _vcache[j].Tag, (_vcache[j].Dirty) ? "D" : " ");
                    break;
                }
            }
        }

        printf("\n\n");
    }
}

/* update the VC LRU block to MRU after it is allocated */
void Cache::UpdateVcMru(unsigned long mru) {
    /* elevate all access counts lower than our current replaced block */
    for (unsigned int i = 0; i < _vcn; i++) {
        if (_vcache[i].Accessed < _vcache[mru].Accessed) {
            _vcache[i].Accessed++;
        }
    }

    _vcache[mru].Accessed = 0; /* Set block to MRU */
    M(printf(" VC update LRU\n"));
}

/* update cache LRU to MRU after it has been allocated */
void Cache::UpdateMru(unsigned long Index, int mru)
{
    /* Go through each associativity incrementing counts that are less than our least recent */
    for (unsigned int i = 0; i < _assoc; i++) {
        if (_cache[Index][i].Accessed < _cache[Index][mru].Accessed) {
            _cache[Index][i].Accessed++;
        }
    }
    /* Most recent is set to 0 */
    _cache[Index][mru].Accessed = 0;

    M(printf(" L%i update LRU\n", _Ln));
}

/* find the replacement VC block */
int Cache::GetReplacementVcBlock() {
    /* VC replacement policy is LRU */
    int vclru = 0;
    /* find the LRU (highest access counter) */
    for (unsigned int i = 0; i < _vcn; i++) {
        if (_vcache[i].Accessed == _vcn - 1) {
            vclru = i;
        }
    }

    /* if its valid, this is a victim block */
    if (_vcache[vclru].Valid) {
        unsigned long vt = _vcache[vclru].Tag;
        M(printf(" VC victim: %lx (tag %lx, index 0, %s)\n",
                (vt << _vctshift), _vcache[vclru].Tag, (_vcache[vclru].Dirty) ? "dirty" : "clean"));
    }
    else
    {
        M(printf(" VC victim: none\n"));
    }

    return vclru;
}

int Cache::GetReplacementBlock(unsigned long Index) {
    unsigned long vt;
    /* Use replacement policy LRU or invalid to find block to replace */
    int lru = 0;
    /* find the block with the highest counter (LRU) */
    for (unsigned int i = 0; i < _assoc; i++) {
        if (_cache[Index][i].Accessed == _assoc - 1) {
            lru = i;
        }
    }

    /* if its valid this is a victim block */
    if (_cache[Index][lru].Valid) {
        vt = _cache[Index][lru].Tag;
        M(printf(" L%i victim: %lx (tag %lx, index %lu, %s)\n",
                _Ln, ((vt << _tshift) | (Index << _ishift)), vt, Index, (_cache[Index][lru].Dirty) ? "dirty" : "clean")
        );
    }
    else {
        M(printf(" L%i victim: none\n", _Ln));
    }

    return lru;
}

/* check the cache line for a hit */
bool Cache::TryHit(unsigned long Index, unsigned long Tag, int *a) {
    /* go through each set + associates, looking for hit */
    for (unsigned int i = 0; i < _assoc; i++) {
        if (_cache[Index][i].Valid && _cache[Index][i].Tag == Tag) {
            *a = i;
            return true;
        }
    }
    return false;
}

/* check the VC for a hit */
                         /* new address, victim address */
bool Cache::TryVcHit(unsigned long Addr, unsigned long va, int *assoc) {
    unsigned long vct = (Addr >> _vctshift) & _vctmask; /* addr tag in vc */
    M(printf(" VC swap req: [%lx, %lx]\n", vct << _vctshift, va));
    NumSwapRequests++;

     for (unsigned i = 0; i < _vcn; i++) {
         if (_vcache[i].Valid && _vcache[i].Tag == vct) {
             /* a hit in VC */
            *assoc = i;
            M(printf(" VC hit: %lx %s\n", vct << _vctshift, _vcache[i].Dirty ? "(dirty)" : "(clean)" ));
            return true;
         }
     }
     M(printf(" VC miss \n"));
    return false;
}

/* Swap the dirty and cache block. Need to recreate the tags since they have different lengths */
void Cache::VcSwapBlocks(unsigned long addr, unsigned long va, int CacheIndex, int LruBlock, int VcIndex) {
    /* copy out the dirty status of VC block, and set the to-be L1 tag in a tmp block */
    cache_data tmp;
    tmp.Dirty = _vcache[VcIndex].Dirty;
    tmp.Tag = (addr >> _tshift) & _tmask;

    /* push L1 cache to VC */
    _vcache[VcIndex].Dirty = _cache[CacheIndex][LruBlock].Dirty;
    _vcache[VcIndex].Tag = (va >> _vctshift) & _vctmask;

    /* Push the temp block to the L1 */
    _cache[CacheIndex][LruBlock].Dirty = tmp.Dirty;
    _cache[CacheIndex][LruBlock].Tag = tmp.Tag;

    NumSwaps++;
}

bool Cache::TryRead(unsigned long Addr) {
    bool hit;
    int lru = 0;
    int assoc;
    //unsigned long Offset;
    unsigned long Index;
    unsigned long Tag;

    /* cache not in use if block size = 0 */
    if (_size == 0)
        return false;

    NumReads++;

    /* cache identifiers */
    //Offset = (Addr >> _oshift) & _omask; //not used
    Index = ((Addr >> _ishift) & _imask) % _nsets;
    Tag = (Addr >> _tshift) & _tmask;

    M(printf(" L%i read: %lx (tag %lx, index %lu)\n", _Ln, ((Tag << _tshift) | (Index << _ishift)), Tag, Index));

    /* Look for a hit */
    hit = TryHit(Index, Tag, &assoc);

    /* Was there a hit? */
    if (hit) {
        NumReadHit++;
        M(printf(" L%i hit\n", _Ln));
        UpdateMru(Index, assoc); /* make the hit the MRU */
        return true;
    }
    else { /* missed */
        NumReadMiss++;
        M(printf(" L%i miss\n", _Ln));

        /* need to identify block associativity to replace */
        lru = GetReplacementBlock(Index);
        unsigned long vt = _cache[Index][lru].Tag; /* victim tag */
        unsigned long va = ((vt << _tshift) | (Index << _ishift)); /* victim address */

        /* Victim cache swap- requires a VC, and for no blocks to be invalid (invalid blocks are not evicted) */
        if (_vcn > 0 && _cache[Index][lru].Valid) {
            hit = TryVcHit(Addr, va, &assoc); /* try VC hit */
            if (hit) {
                /* a hit in VC:
                 * Addr exists in vcache, so swap blocks*/
                VcSwapBlocks(Addr, va, (int)Index, lru, assoc);
                UpdateVcMru((unsigned long)assoc);
            }
            else {
                /* single out a victim cache block */
                int vclru = GetReplacementVcBlock();
                if (_vcache[vclru].Valid && _vcache[vclru].Dirty) {
                    /* there is valid block to evict */
                    unsigned long vcva = _vcache[vclru].Tag << _vctshift;
                    NumWriteBacks++;
                    /* send it to next level */
                    if (NextCache != NULL) {
                        NextCache->TryWrite(vcva);
                    }
                    else {
                        NumMemTraffic++;
                    }
                }
                /* update vcache block with newly allocated address */
                _vcache[vclru].Tag = (va >> _vctshift) & _vctmask;
                _vcache[vclru].Valid = true;
                _vcache[vclru].Dirty = _cache[Index][lru].Dirty;
                UpdateVcMru((unsigned long)vclru);
            }
        }

        /* need to push dirty (written to blocks) to the next level (writeback)
         * from this cache (if there is no VC that does it) */
        if (_vcn == 0) {
            if (_cache[Index][lru].Dirty) {
                NumWriteBacks++;
                if (NextCache != NULL) {
                    NextCache->TryWrite(va);
                }
                else {
                    NumMemTraffic++; /* hitting a null next level means we accessed main memory */
                }
            }
        }

        /* issue a read to the next level. We do this 100% of the time when we miss with no VC.
         * but if there is a hit with the VC we don't read (if VC hit, then hit == 1) */
        if (hit == 0) {
            if (NextCache != NULL) {
                NextCache->TryRead(Addr);
            }
            else {
                NumMemTraffic++;
            }

            /* should have identified a block to replace, set the identifiers */
            _cache[Index][lru].Dirty = false; /* Dirty only for writes */
            _cache[Index][lru].Tag = Tag;
        }
        _cache[Index][lru].Valid = true;
        UpdateMru(Index, lru);
    }
    return false;
}

bool Cache::TryWrite(unsigned long Addr) {
    bool hit;
    int lru = 0;
    int assoc;
    //unsigned long Offset;
    unsigned long Index;
    unsigned long Tag;

    /* cache not in use if block size = 0 */
    if (_size == 0)
        return false;

    NumWrites++;

    /* cache identifiers */
    //Offset = (Addr >> _oshift) & _omask; //not used
    Index = ((Addr >> _ishift) & _imask) % _nsets;
    Tag = (Addr >> _tshift) & _tmask;

    M(printf(" L%i write: %lx (tag %lx, index %lu)\n", _Ln, ((Tag << _tshift) | (Index << _ishift)), Tag, Index));

    /* Look for a hit */
    hit = TryHit(Index, Tag, &assoc);

    if (hit) {
        NumWriteHit++;
        M(printf(" L%i hit\n", _Ln));
        UpdateMru(Index, assoc); /* update the hit to MRU */
        M(printf(" L%i set dirty\n", _Ln));
        _cache[Index][assoc].Dirty = true; /* write marks dirty */
        return true;
    }
    else {
        NumWriteMiss++;
        M(printf(" L%i miss\n", _Ln));

        /* need to identify block to replace */
        lru = GetReplacementBlock(Index);
        unsigned int vt = _cache[Index][lru].Tag; /* victim tag */
        unsigned int va = ((vt << _tshift) | (Index << _ishift)); /* victim address */


        /* Victim cache swap- only if there is a VC, and there are no invalids (invalids won't be evicted) */
        if (_vcn > 0 && _cache[Index][lru].Valid) {
            hit = TryVcHit(Addr, va, &assoc); /* try VC hit */
            if (hit) {
                /* a hit in VC:
                 * Addr exists in vcache, so swap blocks*/
                VcSwapBlocks(Addr, va, (int)Index, lru, assoc);
                UpdateVcMru((unsigned long)assoc);
            }
            else {
                /* single out a victim cache block */
                int vclru = GetReplacementVcBlock();
                if (_vcache[vclru].Valid && _vcache[vclru].Dirty) {
                    /* there is valid block to evict */
                    unsigned long vcva = _vcache[vclru].Tag << _vctshift;
                    NumWriteBacks++;
                    /* write to next level in hierarchy */
                    if (NextCache != NULL) {
                        NextCache->TryWrite(vcva);
                    }
                    else {
                        NumMemTraffic++; /* null next level means we accessed main memory */
                    }
                }
                /* update the vcache data */
                _vcache[vclru].Tag = (va >> _vctshift) & _vctmask;
                _vcache[vclru].Valid = true;
                _vcache[vclru].Dirty = _cache[Index][lru].Dirty;
                UpdateVcMru((unsigned long)vclru);
            }
        }

        /* if its a dirty block (written to) send it to the next layer (writeback).
         * This happens here when there is no VC to do it */
        if (_vcn == 0) {
            if (_cache[Index][lru].Dirty) {
                NumWriteBacks++;
                if (NextCache != NULL) {
                    NextCache->TryWrite(va);
                }
                else {
                    NumMemTraffic++;
                }
            }
        }

        /* check if there is still a hit or not. if there is no VC it will keep the miss.
         * if there is a VC, it depends on if it hit there or not */
        if (hit == 0) {
            /* if we also missed in vcache read memory location from next level. */
            if (NextCache != NULL) {
                NextCache->TryRead(Addr);
            }
            else {
                NumMemTraffic++;
            }

            /* should have identified a block to replace, set the identifiers */
            _cache[Index][lru].Tag = Tag;
        }

        _cache[Index][lru].Valid = true;
        UpdateMru(Index, lru);
        M(printf(" L%i set dirty\n", _Ln));
        _cache[Index][lru].Dirty = true; /* write marks dirty */
    }

    return false;
}


