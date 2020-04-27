#ifndef SIM_BP_H
#define SIM_BP_H

typedef struct bp_params{
    unsigned long int K;
    unsigned long int M1;
    unsigned long int M2;
    unsigned long int N;
    char*             bp_name;
}bp_params;

typedef enum {
    BIMODAL = 0,
    GSHARE = 1,
    HYBRID = 2,
} E_BP_TYPE;

// Put additional data structures here as per your requirement

#endif
