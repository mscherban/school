/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#include <MKL25Z4.H>
#include <stdio.h>
#include "gpio_defs.h"
#include "UART.h"
#include "LEDs.h"

#define MAX_UART_BUFFER 32 /* must be power of 2 */

#define BIT(x) (1 << (x))

typedef enum {
	E_CLI = 0,
	E_AWAITING_SAMPLES,
	E_SAVED_ALL_SAMPLES,
	E_SENDING_DATA,
} eState;

uint16_t ValidChannelNums[] = { 0, 3, 6, 11, 14, 23, 26, 27 };

uint8_t UartBuffer[MAX_UART_BUFFER];
uint16_t g_Samples[6000];
uint16_t g_NumSamplesRemaining;
uint16_t g_ChannelNumber = 0;
uint32_t g_SamplingPerioduS = 10;
uint16_t g_SampleCount = 10;

void SetDebugSignal(int i)
{
	uint32_t tmp = PTB->PDOR;
	tmp &= ~0xf;
	tmp |= BIT(i);
	PTB->PDOR = tmp;
}

void Init_DebugSignals()
{
	SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
	
	PORTB->PCR[E_CLI] &= PORT_PCR_MUX_MASK;
	PORTB->PCR[E_CLI] |= PORT_PCR_MUX(1);
	PORTB->PCR[E_AWAITING_SAMPLES] &= PORT_PCR_MUX_MASK;
	PORTB->PCR[E_AWAITING_SAMPLES] |= PORT_PCR_MUX(1);
	PORTB->PCR[E_SAVED_ALL_SAMPLES] &= PORT_PCR_MUX_MASK;
	PORTB->PCR[E_SAVED_ALL_SAMPLES] |=PORT_PCR_MUX(1);
	PORTB->PCR[E_SENDING_DATA] &= PORT_PCR_MUX_MASK;
	PORTB->PCR[E_SENDING_DATA] |= PORT_PCR_MUX(1);
	
	/* Set ports as outputs */
	PTB->PDDR |= BIT(E_CLI) | BIT(E_AWAITING_SAMPLES) | BIT(E_SAVED_ALL_SAMPLES) | BIT(E_SENDING_DATA);
}

void Init_TPM()
{
	// Turn on clock to TPM
	SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
}

void Init_ADC()
{
	/* Enable ADC0 clock */
	SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
	/* Set TMP0 as the trigger for ADC0, and enable ADC0 alternate trigger */
	SIM->SOPT7 |= SIM_SOPT7_ADC0TRGSEL(8) | SIM_SOPT7_ADC0ALTTRGEN_MASK;
	
	/* Enable single ended 12 bit conversion mode */
	ADC0->CFG1 = ADC_CFG1_MODE(1);
	/* Enable HW trigger and DMA */
	ADC0->SC2 = ADC_SC2_ADTRG_MASK | ADC_SC2_DMAEN_MASK;

	/* Enable requirements for ADC channels */
	PMC_REGSC |= PMC_REGSC_BGBE_MASK;
	
	/* Set all ADC port pins */
	SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK |
									SIM_SCGC5_PORTE_MASK;
	PORTC->PCR[0] &= ~PORT_PCR_MUX_MASK;
	PORTC->PCR[2] &= ~PORT_PCR_MUX_MASK;
	PORTD->PCR[5] &= ~PORT_PCR_MUX_MASK;
	PORTE->PCR[20] &= ~PORT_PCR_MUX_MASK;
	PORTE->PCR[22] &= ~PORT_PCR_MUX_MASK;
	PORTE->PCR[30] &= ~PORT_PCR_MUX_MASK;
	
#if 0
	NVIC_SetPriority(ADC0_IRQn, 2); // 0, 1, 2, or 3
	NVIC_ClearPendingIRQ(ADC0_IRQn);
	NVIC_EnableIRQ(ADC0_IRQn);
}

int ADCcnt = 0;
void ADC0_IRQHandler()
{
	ADC0->SC1[0] = 0 | ADC_SC1_AIEN_MASK;
	ADCcnt++;
#endif	
}

void Init_DMA()
{
	// Gate clocks to DMA and DMAMUX
	SIM->SCGC7 |= SIM_SCGC7_DMA_MASK;
	SIM->SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
	
	// Disable DMA channel to allow configuration
	DMAMUX0->CHCFG[0] = 0;
	
	// Generate DMA interrupt when done
	// Increment destination, transfer words (16 bits)
	// Enable peripheral request, and cycle steal
	DMA0->DMA[0].DCR = DMA_DCR_EINT_MASK | DMA_DCR_DINC_MASK | 
											DMA_DCR_SSIZE(2) | DMA_DCR_DSIZE(2) |
											DMA_DCR_ERQ_MASK | DMA_DCR_CS_MASK;
	
	/* The source address is the ADC0 data address */
	DMA0->DMA[0].SAR = (uint32_t)&ADC0[0].R;
	
	// Configure NVIC for DMA ISR
	NVIC_SetPriority(DMA0_IRQn, 2);
	NVIC_ClearPendingIRQ(DMA0_IRQn); 
	NVIC_EnableIRQ(DMA0_IRQn);
	
	// Enable DMA MUX channel with ADC0 completion as trigger
	DMAMUX0->CHCFG[0] = DMAMUX_CHCFG_SOURCE(40) | DMAMUX_CHCFG_ENBL_MASK;
}

int Dmairqcnt;
void Stop_TPM();
void DMA0_IRQHandler(void)
{
	/* Stop the timer from initiating more ADC conversions*/
	Stop_TPM();
	Control_RGB_LEDs(1, 1, 0);
	SetDebugSignal((int)E_SAVED_ALL_SAMPLES);
	/* Complete DMA transfers */
	DMA0->DMA[0].DSR_BCR |= DMA_DSR_BCR_DONE_MASK;
	
	/* Tell main code that there are no samples remaining */
	g_NumSamplesRemaining = 0;
	Dmairqcnt++;
}

void Stop_TPM()
{
	/* Disable TPM */
	TPM0->SC &= ~TPM_SC_CMOD_MASK;

	
	DMAMUX0->CHCFG[0] &= ~DMAMUX_CHCFG_ENBL_MASK;
	
	/* I don't know why, but this field takes some time to disable,
	 if I don't wait for it, it bugs and keeps counting */
	while (TPM0->SC & TPM_SC_CMOD_MASK)
		;
	/* Clear counter */
	TPM0->CNT = 0;
	/* Ack any TOF */
	TPM0->SC |= TPM_SC_TOF_MASK;
}

void Start_TPM()
{
//#define TPM_DEBUG_1S
#ifndef TPM_DEBUG_1S
	uint32_t tmp;
	int found = 0;
	int ps = 4;
	int clk = 1;
	uint32_t mod = 0xFFFF;
	
	/* With the large range of period lengths and 16 bit counter, need to find the range
	the prescaler and clock source that fits */
	for (ps = 4; ps <= 7; ps++)
	{
		mod = ((float)(((float)DEFAULT_SYSTEM_CLOCK/1000000.0) / (float)BIT(ps)) * g_SamplingPerioduS) - 1;
		if (mod <= 0xFFFF && mod > 0)
		{
			found = 1;
			break;
		}
	}
	
	/* The mod value was too large for the register, need to switch clocks */
	if (found == 0)
	{
		/* use MCGIRCLK (32718) */
		clk = 3;
		ps = 0; /* prescaler = /1 */
		mod = ((float)(CPU_INT_SLOW_CLK_HZ/1000000.0) * g_SamplingPerioduS) - 1;
	}
	
	// Set clock source for tpm
	tmp = SIM->SOPT2;
	tmp &= ~SIM_SOPT2_TPMSRC_MASK;
	tmp |= SIM_SOPT2_TPMSRC(clk);
	SIM->SOPT2 = tmp;
	//SIM->SOPT2 |= (SIM_SOPT2_TPMSRC(clk) | SIM_SOPT2_PLLFLLSEL_MASK);
	
	/* Set CNT to 0 */
	TPM0->CNT = 0;
	// Load modulo
	TPM0->MOD = mod;
	// Set TPM to divide by 'ps' prescaler, enable counting (CMOD)
	TPM0->SC = TPM_SC_CMOD(1) | TPM_SC_PS(ps);
#else
	/* This is a timer config that triggers after 1s just to see what is going on */
	// Set clock source for tpm
	SIM->SOPT2 |= SIM_SOPT2_TPMSRC(2);
	// Load modulo
	TPM0->MOD = CPU_INT_SLOW_CLK_HZ;
	
		// Configure NVIC for DMA ISR
//	NVIC_SetPriority(TPM0_IRQn, 2);
//	NVIC_ClearPendingIRQ(TPM0_IRQn); 
//	NVIC_EnableIRQ(TPM0_IRQn);
	
	TPM0->SC = TPM_SC_CMOD(1)/* | TPM_SC_TOIE_MASK*/;

}

int testtpm;
void TPM0_IRQHandler(void)
{
	TPM0->SC |= TPM_SC_TOF_MASK;
	testtpm++;
#endif
}

void Start_Samples()
{
	/* Set the destination to the sample array */
	DMA0->DMA[0].DAR = (uint32_t)&g_Samples[0];
	/* Set the DMA byte count */
	DMA0->DMA[0].DSR_BCR |= DMA_DSR_BCR_BCR(g_SampleCount * sizeof(uint16_t));
	/* Set the channel of the ADC */
	ADC0->SC1[0] = g_ChannelNumber;
	/* Select 'a' channels */
	ADC0->CFG2 &= ~ADC_CFG2_MUXSEL_MASK;
	if (g_ChannelNumber == 6)
	{
		/* for channel 6 pick 'b' channels */
		ADC0->CFG2 |= ADC_CFG2_MUXSEL_MASK;
	}
	g_NumSamplesRemaining = g_SampleCount;
	
	DMAMUX0->CHCFG[0] |= DMAMUX_CHCFG_ENBL_MASK;
	Start_TPM();
}

/* Function to convert the UART received ASCII number to binary,
   returns the number, otherwise, returns a maxed uint32 (-1)*/
#define IS_ASCII_NUM(x)		((x) >= '0' && (x) <= '9')
#define ASCII_NUM2BINARY(x) ((x) - '0')
uint32_t GetNumber(uint8_t const *b)
{
	uint32_t Num = (uint32_t)-1;
	uint16_t len = 0, i;
	
	/* Get the string length */
	while (b[len] != '\r' && b[len] != '\n')
		len++;
	
	/* Go through string to make sure it is all ASCII numbers, and not symbols or letters */
	for (i = 0; i < len; i++)
	{
		if (!IS_ASCII_NUM(b[i]))
		{
			/* invalid ASCII found, set length to 0 so we don't go further */
			len = 0;
			break;
		}
	}
	
	if (len > 0)
	{
		/* Convert the ASCII string to a binary number to use */
		Num = 0;
		for (i = 0; i < len; i++)
		{
			Num = Num * 10;
			Num += ASCII_NUM2BINARY(b[i]);
		}
	}
	
	return Num;
}

/* Function to process a UART command */
eState UartProcessCommand()
{
	eState NextState = E_CLI; /* The next state returned from this function */
	uint16_t i = 0, run = 1;
	uint8_t c;
	uint32_t Num;
	
	while (run)
	{
		/* Get the next uart character and add it to ring buffer */
		c = UART0_Receive_Poll();
		UartBuffer[i++ & (MAX_UART_BUFFER-1)] = c;
	
		/* if backspace is used remove the backspace */
		if (c == '\b' | c == 0x7F)
		{
			i--;
			/* if there is a previous character, remove that too */
			if (i != 0)
			{
				i--;
				/* Move the cursor back once */
				UART0_Transmit_Poll('\b');
				/* Transmit a space to clear the previous character */
				UART0_Transmit_Poll(' ');
				/* Move the cursor back once */
				UART0_Transmit_Poll('\b');
			}
		}
		else
		{
			UART0_Transmit_Poll(c);
		}

		/* If the command is terminated with enter (carriage return or new line) */
		if (c == '\r' || c == '\n')
		{
			/* Drain the uart buffer so there is nothing else in there
   			 (we could get either \r or \n, or both) */
			while ((UART0->S1 & UART0_S1_RDRF_MASK))
			{
				c = UART0->D;
			}
			
			/* Break out of this loop because we got a command */
			run = 0;
		}
	}
	
	/* Scan the command, get the command character */
	c = UartBuffer[0];
	switch (c)
	{
		case 'R': /* Start Recording */
			/* Next char should be a terminator! */
			c = UartBuffer[1];
			if (c == '\r' || c == '\n')
			{
				printf("\r\nStarting Recording.\r\n");
			}
			NextState = E_AWAITING_SAMPLES;
			break;
		case 'S': /* Send Data */
			/* Next char should be a terminator! */
			c = UartBuffer[1];
			if (c == '\r' || c == '\n')
			{
				printf("\r\nSending data (%i samples)\r\n", g_SampleCount);
			}
			NextState = E_SENDING_DATA;
			break;
		case 'C': /* Select Input Channel */
			Num = GetNumber(&UartBuffer[1]);
		  /* Make sure it's a valid channel number */
			for (i = 0; i < (sizeof(ValidChannelNums) / sizeof(ValidChannelNums[0])); i++)
			{
				if (Num == ValidChannelNums[i])
				{
					g_ChannelNumber = Num;
					printf("\r\nChannel %i selected.\r\n", g_ChannelNumber);
					break;
				}
			}
			break;
		case 'P': /* Set Sampling Period */
			Num = GetNumber(&UartBuffer[1]);
			/* Make sure it's a valid sampling period */
			if (Num >= 2 && Num <= 1000000)
			{
				g_SamplingPerioduS = Num;
				printf("\r\nSampling period set to %i us.\r\n", g_SamplingPerioduS);
			}
			break;
		case 'N': /* Set Sample Count */
			Num = GetNumber(&UartBuffer[1]);
			/* Make sure it's a valid sample count */
			if (Num >= 1 && Num <= 6000)
			{
				g_SampleCount = Num;
				printf("\r\nSample count set to %i.\r\n", g_SampleCount);
			}
			break;
		default:
			break;
	}
	
	return NextState;
}

#define VSTEP		(float)(3.3/4096.0)
#define SAMPLE2VOLTAGE(x)		((x) * VSTEP)
void SendData()
{
	int i;
	//printf("\r\nSending data(%i samples).\r\n", g_SampleCount);
	for (i = 0; i < g_SampleCount; i++)
	{
		printf("%i: %f V\r\n", i + 1, (float)SAMPLE2VOLTAGE(g_Samples[i]));
	}
}

/*----------------------------------------------------------------------------
  MAIN function
	
	Change definition of USE_UART_INTERRUPTS in UART.h to select polled or 
	interrupt-driven communication.
	 *----------------------------------------------------------------------------*/
int main (void) {
	eState State = E_CLI;

	Init_UART0(115200);
	Init_RGB_LEDs();
	Init_DebugSignals();
	Init_TPM();
	Init_ADC();
	Init_DMA();
	
	//Start_Samples();

 	//printf("\n\rGood morning!\n\r");
	
	__enable_irq();
	while (1) {
		SetDebugSignal((int)State);
		switch (State) {
			case E_CLI:
				Control_RGB_LEDs(0, 0, 1);
				State = UartProcessCommand();
				break;
			case E_SENDING_DATA:
				Control_RGB_LEDs(1, 0, 1);
				SendData();
				State = E_CLI;
				break;
			case E_AWAITING_SAMPLES:
				Control_RGB_LEDs(0, 1, 0);
				Start_Samples();
				while (g_NumSamplesRemaining != 0)
					;
				State = E_CLI;
				break;
			case E_SAVED_ALL_SAMPLES: /* This state is  in the DMA ISR! */
				Control_RGB_LEDs(1, 1, 0);
				State = E_CLI;
				break;
			default:
				State = E_CLI;
				break;
		}
	}
	
#if 0
	while (1) {
		c = UART0_Receive_Poll();
		Control_RGB_LEDs(0, 0, 1);
		UART0_Transmit_Poll(c+1);
		Control_RGB_LEDs(0, 0, 0);
	}
#endif

}

// *******************************ARM University Program Copyright © ARM Ltd 2013*************************************   
