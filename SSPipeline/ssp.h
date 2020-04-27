//
// Created by msche on 11/26/2018.
//

#ifndef SUPERSCALERPIPELINE_SSP_H
#define SUPERSCALERPIPELINE_SSP_H

#include <list>
#include <cstdio>
#include "sim_proc.h"

enum {
    FE = 0,
    DE,
    RN,
    RR,
    DI,
    IS,
    EX,
    WB,
    RT,

    PS_MAX,
};

typedef struct {
    bool e;
    int stage_cycle;
    int stage_stat;
}instr_stat;

typedef struct {
    int num;
    unsigned long int pc;
    int type;
    int dst;
    int src1;
    int src2;

    /* ROB table specific */
    bool src1rdy;
    bool src2rdy;
    int rdst;
    int rsrc1;
    int rsrc2;

    unsigned long ctr;
    instr_stat stats[PS_MAX];
} instr;

typedef struct {
    bool valid;
    int rob_tag;
} rmt_entry;

typedef struct {
    bool valid;
    int dst_tag;
    bool rs1_rdy;
    int rs1_tag;
    bool rs2_rdy;
    int rs2_tag;

    instr *instruction;
    int age;
} iq_entry;

typedef struct {
    bool rdy;
    int dst;

    instr *instruction;
} rob_entry;

typedef struct {
    bool valid;
    int life;
    instr *instruction;
} ex_entry;

typedef struct {
    bool valid;
    instr *instruction;
} wb_entry;

class ssp {
public:
    ssp(int rob_size, int iq_size, int widt);
    virtual ~ssp();
    void IncCycles();
    bool Busy();

    void Fetch(FILE *FP);
    void Decode();
    void Rename();
    void RegRead();
    void Dispatch();
    void Issue();
    void Execute();
    void Writeback();
    void Retire();

    //void Debug();

    int NumCycles;
    int InstrNum;
    int NumInstrInPipeline;

private:
    void RetireInstruction(instr *i);
    int RobFreeEntries();
    int AddEntryToRob(int dst);
    int IqFreeEntries();


    int _rs;
    int _iqs;
    int _w;

    int rob_size;
    int ex_num;
    int iq_num;

    int debug_symbol;
    int debug;

    rmt_entry rmt[N_IDA_ENTRIES];
    rmt_entry arf[N_IDA_ENTRIES];
    iq_entry *iq;
    rob_entry *rob;
    ex_entry *execute_list;
    wb_entry *wb;

    int rob_head;
    int rob_tail;

    instr **fetch;
    instr **decode;
    instr **rename;
    instr **regread;
    instr **dispatch;
    instr **issue;
};


#endif //SUPERSCALERPIPELINE_SSP_H
