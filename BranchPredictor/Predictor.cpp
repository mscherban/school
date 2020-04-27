//
// Created by msche on 11/5/2018.
//

#include <stdio.h>
#include "Predictor.h"

/* calculate 2^x */
int pow2(int x) {
    int v = 1;
    for (int i = 0; i < x; i++) {
        v = v * 2;
    }
    return v;
}

Predictor::Predictor(unsigned long int m, unsigned long int n, char letter) {
    gbh = 0;
    Letter = letter;
    /* Set up all the masking to be used */
    _pc_m_mask = ((unsigned long int) 1 << m) - 1;
    _pc_n_mask = (((unsigned long int) 1 << n) - 1) << (m-n);
    _pc_n_shift = (m-n);
    _gbh_n_mask = ((unsigned long int) 1 << n) - 1;
    _gbh_n_n = n - 1;

    /* Set up the counter arrays and initialize to 2 */
    totalcnt = pow2(m);
    counters = new unsigned char[totalcnt];
    for (int i = 0; i < totalcnt; i++) {
        counters[i] = 2;
    }
}

Predictor::~Predictor() {
    if (counters != NULL) {
        delete [] counters;
    }
}

void Predictor::DumpContents() {
    for (int i = 0; i < totalcnt; i++) {
        printf(" %i\t%i\n", i, counters[i]);
    }
}

unsigned long int Predictor::GetIndex(unsigned long int pc) {
    pc = pc >> 2; /* get rid of bytes in between each opcode */
    pc &= _pc_m_mask; /* mask out entire M value */
    pc ^= gbh << _pc_n_shift; /* XOR upper N bits with GBH */

    return pc;
}

unsigned int Predictor::GetPrediction(unsigned long int index) {
    unsigned char Counter = counters[index];
    /* Get the prediction, basically BIT[1] */
    return (Counter >> 1) & (unsigned)1;
}

void Predictor::UpdateCounter(unsigned long int index, unsigned int outcome) {
    /* update the counter, but saturate it at 0 and 3 */
    if (outcome) {
        if (counters[index]++ == 3) {
            counters[index] = 3;
        }
    }
    else {
        if (counters[index]-- == 0) {
            counters[index] = 0;
        }
    }
}

void Predictor::UpdateGbh(unsigned int outcome) {
    //gbh = gbh << 1;
    //gbh |= outcome;
    //gbh &= _gbh_n_mask;

    /* shift the GBH once to the right */
    gbh = gbh >> 1;
    gbh |= outcome << _gbh_n_n; /* add the latest outcome */
    gbh &= _gbh_n_mask; /* mask it */
}

Controller::Controller(E_BP_TYPE type, bp_params params) {
    _type = type;
    bp[BIMODAL] = NULL;
    bp[GSHARE] = NULL;
    nchoosers = pow2(params.K);
    choosers = new unsigned char[nchoosers];
    for (unsigned int i = 0; i < nchoosers; i++) {
        choosers[i] = 1;
    }
    _chooser_mask = ((unsigned long int)1 << params.K) - 1;

    if (type == BIMODAL) {
        bp[BIMODAL] = new Predictor(params.M2, 0, 'B');
    }
    else if (type == GSHARE) {
        bp[GSHARE] = new Predictor(params.M1, params.N, 'G');
    }
    else if (type == HYBRID) {
        bp[BIMODAL] = new Predictor(params.M2, 0, 'B');
        bp[GSHARE] = new Predictor(params.M1, params.N, 'G');
    }
    else {

    }
}

Controller::~Controller() {
    /* cleanup the chooser list */
    delete [] choosers;
    for (int i = 0; i < 2; i++) {
        if (bp[i] != NULL) {
            delete bp[i];
        }
    }
}

unsigned long int Controller::GetIndex(unsigned long int pc) {
    /* get the chooser index */
    pc = pc >> 2;
    pc &= _chooser_mask;
    return pc;
}

unsigned long int Controller::GetPredictor(unsigned long int idx) {
    /* get the predictor, either bimodal or gshare, depending on BIT[1] */
    unsigned char Chooser = choosers[idx];
    return ((Chooser >> 1) & (unsigned)1);
}

void Controller::UpdateChooser(unsigned int idx, unsigned int outcome, unsigned int pb, unsigned int pg) {
    /* if they are equal they are both correct or incorrect, do nothing */
    if (pb != pg) {
        /* counters saturate at 0 and 3 */
        /* if bimodal was correct */
        if (pb == outcome) {
            if (choosers[idx]-- == 0) {
                choosers[idx] = 0;
            }
        } else { /* gshare was correct */
            if (choosers[idx]++ == 3) {
                choosers[idx] = 3;
            }
        }
    }
}

void Controller::DumpContents() {
    unsigned int i;
    if (_type == HYBRID) {
        printf("FINAL CHOOSER CONTENTS\n");
        for (i = 0; i < nchoosers; i++) {
            printf(" %i\t%i\n", i, choosers[i]);
        }
    }

    if (bp[GSHARE] != NULL) {
        printf("FINAL GSHARE CONTENTS\n");
        bp[GSHARE]->DumpContents();
    }

    if (bp[BIMODAL] != NULL) {
        printf("FINAL BIMODAL CONTENTS\n");
        bp[BIMODAL]->DumpContents();
    }
};

/* Make a prediction, the return is whether it is a mispredict or not to be added up in main */
bool Controller::Predict(unsigned long int addr, unsigned int outcome) {
    unsigned int idx[3];
    unsigned int prediction[3];
    bool mispredict;

    /* Get a prediction from both predictors, whichever exists */
    for (int i = 0; i < 2; i++) {
        if (bp[i] != NULL) {
            idx[i] = bp[i]->GetIndex(addr);
            prediction[i] = bp[i]->GetPrediction(idx[i]);
        }
    }

    /* hybrid */
    idx[HYBRID] = this->GetIndex(addr); /* index into the chooser */
    prediction[HYBRID] = this->GetPredictor(idx[HYBRID]); /* will hold 0 or 1 for bimodal or gshare */

    /* tread bimodal and gshare the same since bimodal just has n=0.
     * _type holds if we are bimodal or gshare */
    if (_type == BIMODAL || _type == GSHARE) {
        mispredict = outcome != prediction[_type]; /* was the prediction correct? 1 = no */
        bp[_type]->UpdateCounter(idx[_type], outcome); /* update the predictor counter */
        bp[_type]->UpdateGbh(outcome); /* update the GBH (does nothing if bimodal */
    } else { /* HYBRID */
        /* prediction[HYBRID] is an index to the real predictor */
        mispredict = outcome != prediction[prediction[HYBRID]]; /* was that predictor correct? */
        bp[prediction[HYBRID]]->UpdateCounter(idx[prediction[HYBRID]], outcome); /* update the predictor counter */
        bp[GSHARE]->UpdateGbh(outcome); /* always update GSHARE GBH */
        this->UpdateChooser(idx[HYBRID], outcome, prediction[BIMODAL], prediction[GSHARE]); /* update chooser */
    }

    return mispredict;
}


