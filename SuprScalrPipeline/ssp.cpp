//
// Created by msche on 11/26/2018.
//

#include <cstring>
#include "ssp.h"

void ssp::RetireInstruction(instr *i) {
    printf("%i fu{%i} src{%i,%i} dst{%i} FE{%i,%i} DE{%i,%i} RN{%i,%i} RR{%i,%i} DI{%i,%i} IS{%i,%i} EX{%i,%i} WB{%i,%i} RT{%i,%i}\n",
            i->num, i->type, i->src1, i->src2, i->dst,
            i->stats[FE].stage_cycle, i->stats[FE].stage_stat,
            i->stats[DE].stage_cycle, i->stats[DE].stage_stat,
            i->stats[RN].stage_cycle, i->stats[RN].stage_stat,
            i->stats[RR].stage_cycle, i->stats[RR].stage_stat,
            i->stats[DI].stage_cycle, i->stats[DI].stage_stat,
            i->stats[IS].stage_cycle, i->stats[IS].stage_stat,
            i->stats[EX].stage_cycle, i->stats[EX].stage_stat,
            i->stats[WB].stage_cycle, i->stats[WB].stage_stat,
            i->stats[RT].stage_cycle, i->stats[RT].stage_stat);

    NumInstrInPipeline--;
    if (i != NULL) {
        delete i;
    }
}

int ssp::AddEntryToRob(int dst) {
    int ret = rob_tail;
    rob[rob_tail].dst = dst;
    rob[rob_tail].rdy = false;
    rob_tail = (rob_tail + 1) % _rs;
    rob_size++;
    return ret;
}

int ssp::IqFreeEntries() {
    int i;
    int num = 0;

    for (i = 0; i < _iqs; i++) {
        if (iq[i].valid) {
            num++;
        }
    }
    iq_num = num;
    return _iqs - num;
}

int ssp::RobFreeEntries() {
    return _rs - this->rob_size;
}

void ssp::Retire() {
    int H = rob_head;
    int T = rob_tail;
    int cnt = 0;
    bool go = true;
    bool tryonceiffull = (this->rob_size == this->_rs);
    //rob_entry rbe;
    //instr ise;

    //int DH = H;
   // int DT = T;
#if 0
    if (H == 4 && T == 4 && debug == 0) {
        debug = 1;
        printf("%i: dst: %i, rdy:%i\n", H, rob[H].dst, rob[H].rdy);
        printf("p:%p\n", rob[H].instruction);
        //if (rob[H].instruction != NULL) {
        //    printf("  istr.num:%i\n", rob[H].instruction->num);
        //}
    }




        if (debug == 1) {
            printf("=== ROB DUMP ===\n");
            while (DH != DT) {

                printf("%i: dst: %i, rdy:%i\n", DH, rob[DH].dst, rob[DH].rdy);
                if (rob[DH].instruction != NULL) {
                    printf("  istr.num:%i\n", rob[DH].instruction->num);
                }
                DH = (DH + 1) % _rs;
            }
        }
#endif
    while (H != T || tryonceiffull) {
        tryonceiffull = false;
        if (rob[H].instruction) {
            //ise = *rob[H].instruction;
            if (!rob[H].instruction->stats[RT].e) {
                rob[H].instruction->stats[RT].e = true;
                rob[H].instruction->stats[RT].stage_cycle = NumCycles;
            }
            rob[H].instruction->stats[RT].stage_stat++;
        }

        /* check instructions for retirement */
        if (rob[H].instruction && cnt < _w && go) {
            if (rob[rob_head].rdy) {
                cnt++;
                //rob[rob_head].rdy = false;
                //rbe = rob[rob_head];
                if (rob[rob_head].dst >= 0) {
                    if (rob_head == rmt[rob[rob_head].dst].rob_tag) {
                        rmt[rob[rob_head].dst].valid = false;
                    }
                }
                RetireInstruction(rob[rob_head].instruction);
                rob[rob_head].instruction = NULL;
                rob_head = (rob_head + 1) % _rs;
                this->rob_size--;
            }
            else {
                go = false;
            }
        }

        H = (H + 1) % _rs;
    }

}

void ssp::Writeback() {
    int i;
    instr *is;

    /* increment stats */
    for (i = 0; i < _w * 5; i++) {
        if (wb[i].valid && wb[i].instruction != NULL) {
            is = wb[i].instruction;
            if (!is->stats[WB].e) {
                is->stats[WB].e = true;
                is->stats[WB].stage_cycle = NumCycles;
            }
            is->stats[WB].stage_stat++;
        }
    }

    /* go through all instructions in WB and mark them on the ROB */
    for (i = 0; i < _w * 5; i++) {
        if (wb[i].valid && wb[i].instruction != NULL) {
            wb[i].valid = false;
            is = wb[i].instruction;
            rob[is->rdst].rdy = true;
            rob[is->rdst].instruction = is;
        }
    }
}

void ssp::Execute() {
    int i, j, k;
    int wakeup;
    instr *is;
    //ex_entry exe;

    /* go through entire execute list incrementing stats and decrementing life */
    for (i = 0; i < _w * 5; i++) {
        if (execute_list[i].valid && execute_list[i].instruction != NULL) {
            execute_list[i].life--;
            //exe = execute_list[i];
            is = execute_list[i].instruction;
            if (!is->stats[EX].e) {
                is->stats[EX].e = true;
                is->stats[EX].stage_cycle = NumCycles;
            }
            is->stats[EX].stage_stat++;
        }
    }

    /* go through the execution list and find all at a 0 life and remove them */
    for (i = 0; i < _w * 5; i++) {
        if (execute_list[i].valid && execute_list[i].life == 0) {
            //exe = execute_list[i];
            //is = execute_list[i].instruction;

            execute_list[i].valid = false; //remove from execute_list
            /* insert into wb */
            for (j = 0; j < _w * 5; j++) {
                if (!wb[j].valid) {
                    wb[j].valid = true;
                    wb[j].instruction = execute_list[i].instruction;
                    break;
                }
            }

            /* wakeup - check if any instructions in the pipeline have a matching tag */
            is = execute_list[i].instruction;
            wakeup = is->rdst; //rob tag

            /* issue queue */
            for (k = 0; k < _iqs; k++) {
                if (iq[k].valid) {
                    if (iq[k].rs1_tag == wakeup) {
                        iq[k].rs1_rdy = true;
                    }
                    if (iq[k].rs2_tag == wakeup) {
                        iq[k].rs2_rdy = true;
                    }
                }
            }

            /* dispatch */
            for (k = 0; k < _w; k++) {
                if (dispatch[k] != NULL) {
                    if (dispatch[k]->rsrc1 == wakeup) {
                        dispatch[k]->src1rdy = true;
                    }
                    if (dispatch[k]->rsrc2 == wakeup) {
                        dispatch[k]->src2rdy = true;
                    }
                }
            }

            /* RegRead */
            for (k = 0; k < _w; k++) {
                if (regread[k] != NULL) {
                    if (regread[k]->rsrc1 == wakeup) {
                        regread[k]->src1rdy = true;
                    }
                    if (regread[k]->rsrc2 == wakeup) {
                        regread[k]->src2rdy = true;
                    }
                }
            }
        }
    }
}

void ssp::Issue() {
    int i, j, e;
    int oldest, youngest = 0, curr;
    bool go = false;
    instr *is;
    iq_entry iqe;

    /* increment all stats */
    for (i = 0; i < _iqs; i++) {
        if (iq[i].valid) {
            if (iq[i].instruction != NULL) {
                if (!iq[i].instruction->stats[IS].e) {
                    iq[i].instruction->stats[IS].e = true;
                    iq[i].instruction->stats[IS].stage_cycle = NumCycles;
                }
                iq[i].instruction->stats[IS].stage_stat++;
            }
        }
    }

    /* find the youngest instruction (highest value) */
    for (i = 0; i < _iqs; i++) {
        if (iq[i].valid && iq[i].age > youngest) {
            youngest = iq[i].age;
        }
    }

    /* Do this WIDTH times */
    for (j = 0; j < _w; j++) {
        oldest = youngest + 1;
        curr = 0;
        go = false;

        /* find the oldest */
        for (i = 0; i < _iqs; i++) {
            if (iq[i].valid && iq[i].rs1_rdy && iq[i].rs2_rdy && iq[i].age < oldest) {
                is = iq[i].instruction;
                oldest = iq[i].age;
                iqe = iq[i];
                curr = i;
                go = true;
            }
        }

        /* curr should be the index of the oldest instruction */
        if (go && iq[curr].valid) {
            iq[curr].valid = false;

            /* pull out the instruction */
            is = iq[curr].instruction;

            /* give it to execute list */
            /* find first free entry */
            for (e = 0; e < _w * 5; e++) {
                if (!execute_list[e].valid) {
                    execute_list[e].valid = true;
                    execute_list[e].instruction = is;
                    execute_list[e].life = (is->type == 0) ? 1 : (is->type == 1) ? 2 : 5;
                    ex_num++;
                    break;
                }
            }
        }
    }
}

void ssp::Dispatch() {
    int i, j;
    int num = 0;
    int num_in_iq = 0;
    instr *is;

    /* update stats of the instructions in this register */
    for (i = 0; i < _w; i++) {
        if (dispatch[i] != NULL) {
            /* tag the cycle number if it hasnt been */
            if (!dispatch[i]->stats[DI].e) {
                dispatch[i]->stats[DI].e = true;
                dispatch[i]->stats[DI].stage_cycle = NumCycles;
            }
            dispatch[i]->stats[DI].stage_stat++; /* increment cycle counter */
        }
    }

    /* get number of items in this bundle */
    for (i = 0; i < _w; i++) {
        if (dispatch[i] != NULL) {
            num++;
        }
    }

    num_in_iq = IqFreeEntries();

    /* send to issue queue if there is room */
    if (num && num_in_iq >= num) {
        for (j = 0; j < num; j++) {
            for (i = 0; i < _iqs; i++) {
                /* find the next empty iq spot */
                if (!iq[i].valid) {
                    iq[i].valid = true;
                    is = dispatch[j];
                    iq[i].dst_tag = is->rdst;
                    iq[i].rs1_tag = is->rsrc1;
                    iq[i].rs2_tag = is->rsrc2;
                    iq[i].rs1_rdy = is->src1rdy;
                    iq[i].rs2_rdy = is->src2rdy;
                    iq[i].age = is->num;
                    iq[i].instruction = is;
                    dispatch[j] = NULL;
                    break;
                }
            }
        }
    }
}

void ssp::RegRead() {
    int i;
    int num = 0;
    int num_in_di = 0;
    instr *is;

    /* update stats of the instructions in this register */
    for (i = 0; i < _w; i++) {
        if (regread[i] != NULL) {
            /* tag the cycle number if it hasnt been */
            if (!regread[i]->stats[RR].e) {
                regread[i]->stats[RR].e = true;
                regread[i]->stats[RR].stage_cycle = NumCycles;
            }
            regread[i]->stats[RR].stage_stat++; /* increment cycle counter */
        }
    }

    /* count how many there are here and in dispatch */
    for (i = 0; i < _w; i++) {
        if (regread[i] != NULL) {
            num++;
        }
        if (dispatch[i] != NULL) {
            num_in_di++;
        }
    }

    if (num_in_di == 0 && num > 0) {
        /* At this point we have our instructions with the ROB assigned:
         * rdst --> ROB tag with destination
         * rsrc1 --> ROB tag with our source, -1 if there is no valid source
         * rsrc2 --> ROB tag with our source, -1 if there is no valid source
         * Need to set src1rdy and src2rdy here */
        for (i = 0; i < _w; i++) {
            if (regread[i] != NULL) {
                is = regread[i];

                /* check if src1 is ready if its not already */
                if (is->src1rdy == false) {
                    if (is->rsrc1 == -1) { /* no real source, it's always ready */
                        is->src1rdy = true;
                    }
                    else { /* check ROB for ready */
                        is->src1rdy = rob[is->rsrc1].rdy;
                    }
                }

                if (is->src2rdy == false) {
                    if (is->rsrc2 == -1) {
                        is->src2rdy = true;
                    }
                    else {
                        is->src2rdy = rob[is->rsrc2].rdy;
                    }
                }

                /* write instruction to dispatch */
                dispatch[i] = regread[i];
                regread[i] = NULL;
            }
        }
    }
}

void ssp::Rename() {
    int i;
    int num = 0;
    int num_in_rr = 0;
    int FreeRob = 0;
    int RobTag;
    instr *is;

    /* update stats of the instructions in this register */
    for (i = 0; i < _w; i++) {
        if (rename[i] != NULL) {
            /* tag the cycle number if it hasnt been */
            if (!rename[i]->stats[RN].e) {
                rename[i]->stats[RN].e = true;
                rename[i]->stats[RN].stage_cycle = NumCycles;
            }
            rename[i]->stats[RN].stage_stat++; /* increment cycle counter */
        }
    }

    /* check capacity of RegRead and ROB */
    /* first count how many I have here and RegRead */
    for (i = 0; i < _w; i++) {
        if (rename[i] != NULL) {
            num++;
        }
        if (regread[i] != NULL) {
            num_in_rr++;
        }
    }

    FreeRob = RobFreeEntries();

    /* make sure RR is empty and the ROB has enough space for our instructions */
    if (num_in_rr == 0 && FreeRob >= num) {

        /* we have the space! go through and allocate to ROB and rename*/
        for (i = 0; i < _w; i++) {
            if (rename[i] != NULL) {
                /* here is our working instruction */
                is = rename[i]; //pull it out to make working with it easier

                /* Add destination register to ROB (even if it's -1 (doesnt exist)) */
                RobTag = AddEntryToRob(is->dst);
                //printf("RN: %i takes %i\n", is->num, RobTag);

                /* deal with sources
                 * rd, src1, src2 ---> rdst, rsrc1, rsrc2
                 * rsrc1 = rsrc2 = -1 initially and reassigned if the source is in the ROB */
                is->rsrc1 = -1;
                is->rsrc2 = -1;
                if (is->src1 >= 0) {
                    /* check the RMT if there is a rob tag */
                    if (rmt[is->src1].valid) {
                        is->rsrc1 = rmt[is->src1].rob_tag;
                    }
                }
                if (is->src2 >= 0) {
                    if (rmt[is->src2].valid) {
                        is->rsrc2 = rmt[is->src2].rob_tag;
                    }
                }

                /* Deal with destination */
                /* assign the rob tag to the RMT unless it has no real destination */
                if (is->dst >= 0) {
                    rmt[is->dst].rob_tag = RobTag;
                    rmt[is->dst].valid = true;
                }
                is->rdst = RobTag;

                /* add the instruction to regread to ensure readiness */
                regread[i] = rename[i];
                rename[i] = NULL;
            }
        }
    }
}

void ssp::Decode() {
    int i;
    int num = 0;

    /* update stats of the instructions in this register */
    for (i = 0; i < _w; i++) {
        if (decode[i] != NULL) {
            /* tag the cycle number if it hasnt been */
            if (!decode[i]->stats[DE].e) {
                decode[i]->stats[DE].e = true;
                decode[i]->stats[DE].stage_cycle = NumCycles;
            }
            decode[i]->stats[DE].stage_stat++; /* increment cycle counter */
        }
    }

    /* check if rename is empty */
    for (i = 0; i < _w; i++) {
        if (rename[i] != NULL) {
            num++;
        }
    }

    if (num == 0) {
        /* rename is empty, copy them over */
        for (i = 0; i < _w; i++) {
            if (decode[i] != NULL) {
                rename[i] = decode[i];
                decode[i] = NULL;
            }
        }
    }
}

void ssp::Fetch(FILE *FP) {
    instr *new_instr;
    int num = 0;
    int op_type, dest, src1, src2;  // Variables are read from trace file
    unsigned long int pc; // Variable holds the pc read from input file

    /* check if decode is empty */
    for (int i = 0; i < _w; i++) {
        if (decode[i] != NULL) {
            num++;
        }
    }

    if (num == 0) {
        /* Fetch is empty */
        for (int i = 0; i < _w; i++) {
            /* make sure we didnt hit end of file first... */
            if (fscanf(FP, "%lx %d %d %d %d", &pc, &op_type, &dest, &src1, &src2) != EOF) {
                /* got an instruction, create the structure for it and fill it out and attach to decode */
                new_instr = new instr;
                NumInstrInPipeline++;
                memset(new_instr, 0, sizeof(instr));

                new_instr->num = InstrNum++;
                new_instr->stats[FE].stage_cycle = NumCycles;
                new_instr->stats[FE].stage_stat++;
                new_instr->pc = pc;
                new_instr->dst = dest;
                new_instr->src1 = src1;
                new_instr->src2 = src2;
                new_instr->type = op_type;
                decode[i] = new_instr;
            }
        }
    }
}

ssp::ssp(int rob_size, int iq_size, int width) {
    int i;

    _rs = rob_size;
    _iqs = iq_size;
    _w = width;

    debug = 0;

    NumCycles = 0;
    InstrNum = 0;
    NumInstrInPipeline = 0;
    ex_num = 0;

    /* initialize all remap table entries as invalid */
    for (i = 0; i < N_IDA_ENTRIES; i++) {
        rmt[i].valid = false;
    }

    /* initialize all ARF entries as valid (and they always will be) */
    for (i = 0; i < N_IDA_ENTRIES; i++) {
        arf[i].valid = true;
    }

    /* create issue queue array and initialize to invalid */
    iq = new iq_entry[_iqs];
    for (i = 0; i < _iqs; i++) {
        iq[i].valid = false;
    }

    /* create ROB and initialize to not-ready */
    rob = new rob_entry[_rs];
    for (i = 0; i < _rs; i++) {
        rob[i].rdy = false;
        rob[i].instruction = NULL;
    }
    rob_head = 0;
    rob_tail = 0;
    this->rob_size = 0;

    execute_list = new ex_entry[_w * 5];
    for (i = 0; i < _w * 5; i++) {
        execute_list[i].valid = false;
    }

    wb = new wb_entry[_w * 5];
    for (i = 0; i < _w * 5; i++) {
        wb[i].valid = false;
    }

    fetch = new instr*[_w];
    for (i = 0; i < _w; i++) {
        fetch[i] = NULL;
    }
    decode = new instr*[_w];
    for (i = 0; i < _w; i++) {
        decode[i] = NULL;
    }
    rename = new instr*[_w];
    for (i = 0; i < _w; i++) {
        rename[i] = NULL;
    }
    regread = new instr*[_w];
    for (i = 0; i < _w; i++) {
        regread[i] = NULL;
    }
    dispatch = new instr*[_w];
    for (i = 0; i < _w; i++) {
        dispatch[i] = NULL;
    }
    issue = new instr*[_w];
    for (i = 0; i < _w; i++) {
        issue[i] = NULL;
    }
}

ssp::~ssp() {
    if (wb != NULL) {
        delete[] wb;
    }

    if (execute_list != NULL) {
        delete[] execute_list;
    }

    if (rob != NULL) {
        delete[] rob;
    }

    if (iq != NULL) {
        delete[] iq;
    }

    if (fetch != NULL) {
        delete[] fetch;
    }

    if (decode != NULL) {
        delete[] decode;
    }

    if (rename != NULL) {
        delete[] rename;
    }

    if (regread != NULL) {
        delete[] regread;
    }

    if (dispatch != NULL) {
        delete[] dispatch;
    }

    if (issue != NULL) {
        delete[] issue;
    }
}

void ssp::IncCycles() {
    NumCycles++;
}

bool ssp::Busy() {
    return NumInstrInPipeline > 0;
}

#if 0
void ssp::Debug() {
    volatile instr *is;
    int i;
    int find = 68;
    bool found = false;

    iq_entry iqe;

    for (i = 0; i < _w; i++) {
        if (decode[i] != NULL) {
            if (decode[i]->num == find) {
                found = true;
                is = decode[i];
            }
        }

        if (rename[i] != NULL) {
            if (rename[i]->num == find) {
                found = true;
                is = rename[i];
            }
        }

        if (regread[i] != NULL) {
            if (regread[i]->num == find) {
                found = true;
                is = regread[i];
            }
        }

        if (dispatch[i] != NULL) {
            if (dispatch[i]->num == find) {
                found = true;
                is = dispatch[i];
            }
        }
    }

    for (i = 0; i < _iqs; i++) {
        if (iq[i].valid) {
            if (iq[i].instruction->num == find)
            {
                found = true;
                is = iq[i].instruction;
                iqe = iq[i];
                iqe = iqe;
            }
        }
    }

    for (i = 0; i < _w * 5; i++) {
        if (execute_list[i].valid) {
            if (execute_list[i].instruction->num == find) {
                found = true;
                ex_entry ent = execute_list[i];
                is = execute_list[i].instruction;
            }
        }
    }

    for (i = 0; i < _w ^ 5; i++) {
        if (wb[i].valid) {
            if (wb[i].instruction->num == find) {
                found = true;
                is = wb[i].instruction;
            }
        }
    }

    if (found) {
        is = is;
    }

}
#endif
