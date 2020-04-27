#include "timers.h"
#include <MKL25Z4.h> 
#include "LEDs.h"

void Init_Timer_Output(uint16_t period){
	// Enable clock to port A
	SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;;
	
	// PTA12 connected to FTM1_CH0, Mux Alt 3
	// Set pin to FTM
	PORTA->PCR[12] &= ~PORT_PCR_MUX_MASK;          
	PORTA->PCR[12] |= PORT_PCR_MUX(3);          

	// Configure TPM
	SIM->SCGC6 |= SIM_SCGC6_TPM1_MASK;
	//set clock source for tpm: 48 MHz
	SIM->SOPT2 |= (SIM_SOPT2_TPMSRC(1) | SIM_SOPT2_PLLFLLSEL_MASK);
	//load the counter and mod
	TPM1->MOD = period-1;  
	//set TPM count direction to up with a divide by 2 prescaler 
	TPM1->SC =  TPM_SC_PS(1);
	// Continue operation in debug mode
	TPM1->CONF |= TPM_CONF_DBGMODE(3);
	// Set channel 0 to edge-aligned low-true PWM
	TPM1->CONTROLS[0].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK;
	// Set initial duty cycle
	TPM1->CONTROLS[0].CnV = period/2;
	// Start TPM
	TPM1->SC |= TPM_SC_CMOD(1);
}


// *******************************ARM University Program Copyright © ARM Ltd 2013*************************************   
