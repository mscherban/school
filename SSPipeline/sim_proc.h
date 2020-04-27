#ifndef SIM_PROC_H
#define SIM_PROC_H

#define N_IDA_ENTRIES   67

#define TYPE_0_CYCLES   1
#define TYPE_1_CYCLES   2
#define TYPE_2_CYCLES   5

typedef struct proc_params{
    unsigned long int rob_size;
    unsigned long int iq_size;
    unsigned long int width;
}proc_params;

// Put additional data structures here as per your requirement

#endif
