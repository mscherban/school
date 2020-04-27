#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "sim_cache.h"
#include "Cache.h"

/*  argc holds the number of command line arguments
    argv[] holds the commands themselves

    Example:-
    sim_cache 32 8192 4 7 262144 8 gcc_trace.txt
    argc = 8
    argv[0] = "sim_cache"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
*/
int main (int argc, char* argv[])
{
    FILE *FP;               // File handler
    char *trace_file;       // Variable that holds trace file name;
    cache_params params;    // look at sim_cache.h header file for the the definition of struct cache_params
    char rw;                // variable holds read/write type read from input file. The array size is 2 because it holds 'r' or 'w' and '\0'. Make sure to adapt in future projects
    unsigned long int addr; // Variable holds the address read from input file
    unsigned long count = 1;

    if(argc != 8)           // Checks if correct number of inputs have been given. Throw error and exit if wrong
    {
        printf("Error: Expected inputs:7 Given inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }

    // strtoul() converts char* to unsigned long. It is included in <stdlib.h>
    params.block_size       = strtoul(argv[1], NULL, 10);
    params.l1_size          = strtoul(argv[2], NULL, 10);
    params.l1_assoc         = strtoul(argv[3], NULL, 10);
    params.vc_num_blocks    = strtoul(argv[4], NULL, 10);
    params.l2_size          = strtoul(argv[5], NULL, 10);
    params.l2_assoc         = strtoul(argv[6], NULL, 10);
    trace_file              = argv[7];

    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if(FP == NULL)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }

    // Print params
    printf("  ===== Simulator configuration =====\n"
           "  BLOCKSIZE:                  %8lu\n"
           "  L1_SIZE:                    %8lu\n"
           "  L1_ASSOC:                   %8lu\n"
           "  VC_NUM_BLOCKS:              %8lu\n"
           "  L2_SIZE:                    %8lu\n"
           "  L2_ASSOC:                   %8lu\n"
           "  trace_file:      %20s\n\n", params.block_size, params.l1_size, params.l1_assoc, params.vc_num_blocks, params.l2_size, params.l2_assoc, trace_file);

    /* create both caches */
    Cache L1(params.block_size, params.l1_size, params.l1_assoc, params.vc_num_blocks);
    Cache L2(params.block_size, params.l2_size, params.l2_assoc, 0); /* L2 has no victim cache */

    /* Link L1 and L2 (does not link if L2 is size 0) */
    L1.SetNextCache(&L2);

    char str[2];
    while(fscanf(FP, "%s %lx", str, &addr) != EOF)
    {
        M(printf("----------------------------------------\n"));

        rw = str[0];
//        if (rw == 'r')
//            printf("%s %lx\n", "read", addr);           // Print and test if file is read correctly
//        else if (rw == 'w')
//            printf("%s %lx\n", "write", addr);          // Print and test if file is read correctly
        /*************************************
                  Add cache code here
        **************************************/
        if (rw == 'r') {
            M(printf("# %lu: read %lx\n", count++, addr));
            L1.TryRead(addr);
        } else {
            M(printf("# %lu: write %lx\n", count++, addr));
            L1.TryWrite(addr);
        }
    }

    L1.DumpCacheContents();
    L2.DumpCacheContents();
    printf("===== Simulation results =====\n"
           "  a. number of L1 reads:                     %7i\n"
           "  b. number of L1 read misses:               %7i\n"
           "  c. number of L1 writes:                    %7i\n"
           "  d. number of L1 write misses:              %7i\n"
           "  e. number of swap requests:                %7i\n"
           "  f. swap request rate:                      %7.04f\n"
           "  g. number of swaps:                        %7i\n"
           "  h. combined L1+VC miss rate:               %7.04f\n"
           "  i. number writebacks from L1/VC:           %7i\n"
           "  j. number of L2 reads:                     %7i\n"
           "  k. number of L2 read misses:               %7i\n"
           "  l. number of L2 writes:                    %7i\n"
           "  m. number of L2 write misses:              %7i\n"
           "  n. L2 miss rate:                           %7.04f\n"
           "  o. number of writebacks from L2:           %7i\n"
           "  p. total memory traffic:                   %7i\n",
           /* a */ L1.NumReads,
           /* b */ L1.NumReadMiss,
           /* c */ L1.NumWrites,
           /* d */ L1.NumWriteMiss,
           /* e */ L1.NumSwapRequests,
           /* f */ (L1.NumSwapRequests / (float)(L1.NumReads + L1.NumWrites)),
           /* g */ L1.NumSwaps,
           /* h */ ((L1.NumReadMiss + L1.NumWriteMiss - L1.NumSwaps) / (float)(L1.NumReads + L1.NumWrites)),
           /* i */ L1.NumWriteBacks,
           /* j */ L2.NumReads,
           /* k */ L2.NumReadMiss,
           /* l */ L2.NumWrites,
           /* m */ L2.NumWriteMiss,
           /* n */ ((L2.NumReadMiss) / (float)(L2.NumReads + .0001)),
           /* o */ L2.NumWriteBacks,
           /* p */ (L1.NumMemTraffic + L2.NumMemTraffic));

    return 0;
}