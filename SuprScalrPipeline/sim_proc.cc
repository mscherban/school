#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_proc.h"
#include "ssp.h"

FILE *FP;         // File handler
ssp *SSP;
unsigned int CycleCounter = 0;
unsigned int NumInstr = 0;


bool Advance_Cycle() {
    bool keep_going = true;

    CycleCounter++;
    SSP->IncCycles();

    if (feof(FP) && !SSP->Busy()) {
        keep_going = false;
    }
    else {

    }

    return keep_going;
}

/*  argc holds the number of command line arguments
    argv[] holds the commands themselves

    Example:-
    sim 256 32 4 gcc_trace.txt
    argc = 5
    argv[0] = "sim"
    argv[1] = "256"
    argv[2] = "32"
    ... and so on
*/
int main (int argc, char* argv[])
{

    char *trace_file;       // Variable that holds trace file name;
    proc_params params;       // look at sim_bp.h header file for the the definition of struct proc_params
    //int op_type, dest, src1, src2;  // Variables are read from trace file
    //unsigned long int pc; // Variable holds the pc read from input file
    
    if (argc != 5)
    {
        printf("Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }
    
    params.rob_size     = strtoul(argv[1], NULL, 10);
    params.iq_size      = strtoul(argv[2], NULL, 10);
    params.width        = strtoul(argv[3], NULL, 10);
    trace_file          = argv[4];
//    printf("rob_size:%lu "
//            "iq_size:%lu "
//            "width:%lu "
//            "tracefile:%s\n", params.rob_size, params.iq_size, params.width, trace_file);
    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if(FP == NULL)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }

    SSP =  new ssp((int)params.rob_size, (int)params.iq_size, (int)params.width);
#if 0
    while(fscanf(FP, "%lx %d %d %d %d", &pc, &op_type, &dest, &src1, &src2) != EOF)
    {
        
        printf("%lx %d %d %d %d\n", pc, op_type, dest, src1, src2); //Print to check if inputs have been read correctly
        /*************************************
            Add calls to OOO simulator here
        **************************************/
    }
#endif
    do {
        SSP->Retire();
        SSP->Writeback();
        SSP->Execute();
        SSP->Issue();
        SSP->Dispatch();
        SSP->RegRead();
        SSP->Rename();
        SSP->Decode();
        SSP->Fetch(FP);

        //SSP->Debug();
    } while (Advance_Cycle());


    printf(
            "# === Simulator Command =========\n"
            "# %s %s %s %s %s\n"
            "# === Processor Configuration ===\n"
            "# ROB_SIZE = %lu\n"
            "# IQ_SIZE  = %lu\n"
            "# WIDTH    = %lu\n"
            "# === Simulation Results ========\n"
            "# Dynamic Instruction Count    = %i\n"
            "# Cycles                       = %i\n"
            "# Instructions Per Cycle (IPC)  = %0.02f\n",
            argv[0], argv[1], argv[2], argv[3], argv[4],
            params.rob_size,
            params.iq_size,
            params.width,
            SSP->InstrNum,
            SSP->NumCycles,
            (SSP->InstrNum/(float)SSP->NumCycles)
            );

    delete SSP;
    return 0;
}
