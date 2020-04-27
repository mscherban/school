//
// Created by msche on 11/5/2018.
//

#ifndef BRANCHPREDICTOR_PREDICTOR_H
#define BRANCHPREDICTOR_PREDICTOR_H
#include "sim_bp.h"

class Predictor {
public:
    unsigned long int GetIndex(unsigned long int pc);
    void UpdateCounter(unsigned long int index, unsigned int outcome);
    void UpdateGbh(unsigned int outcome);
    unsigned int GetPrediction(unsigned long int index);
    void DumpContents();
    Predictor(unsigned long int m, unsigned long int n, char letter);
    virtual ~Predictor();

private:

    char Letter;
    unsigned long int gbh;
    unsigned long int _pc_m_mask;
    unsigned long int _pc_n_mask;
    unsigned long int _pc_n_shift;
    unsigned long int _gbh_n_mask;
    unsigned long int _gbh_n_n;

    int totalcnt;
    unsigned char *counters;
};

class Controller {
public:
    Controller(E_BP_TYPE type, bp_params params);
    virtual ~Controller();
    bool Predict(unsigned long int addr, unsigned int outcome);
    unsigned long int GetIndex(unsigned long int pc);
    unsigned long int GetPredictor(unsigned long int idx);
    void UpdateChooser(unsigned int idx, unsigned int outcome, unsigned int pb, unsigned int pg);
    void DumpContents();

private:
    E_BP_TYPE _type;
    unsigned int nchoosers;
    unsigned int _chooser_mask;
    unsigned char *choosers;
    Predictor *bp[2];
};

#endif //BRANCHPREDICTOR_PREDICTOR_H
