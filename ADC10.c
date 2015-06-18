//#include <msp430.h>
#include "io430.h"
#include "ADC10.h"

uint ADC10_DMA_Data[4];

void ADC10_Init(){
  
  P2SEL |= BIT0 + BIT1 + BIT2 + BIT3; // ADC
  ADC10CTL0 = SREF_1;      // VREF = VREF+ - VSS
  ADC10CTL0 |= ADC10SHT_3; // S&H time = 64 CLKs
  ADC10CTL0 |= ADC10SR;    // 50 ksps max
  ADC10CTL0 |= REFBURST;   // REF Buffer On only while sample and conversion
  ADC10CTL0 |= REF2_5V;    // VREF = 2.5V
  ADC10CTL0 |= REFON;      // REF Generator On
  ADC10CTL0 |= MSC;         //Multiply sample and conversion
  ADC10CTL0 |= ADC10ON;     //ADC10 On
  
  ADC10CTL1 = INCH_3 + CONSEQ_1;        // A3/A2/A1/A0, single sequence
  ADC10DTC1 = 0x04;                         // 4 conversions
  ADC10AE0 |= 0x0F;                         // P2.3, 2.2, 2.1, 2.0 ADC10 option select
}

/* --------------------- Измерение АЦП по 4-м каналам -------------------- */
void ADC10_Measure()
{  
  ADC10CTL0 &= ~ENC;
  while (ADC10CTL1 & BUSY);               // Wait if ADC10 core is active
  ADC10SA = (int)ADC10_DMA_Data;                        // Data buffer start
  //ADC10CTL0 &= ~ADC10IFG;
  ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start 
}
/* -------------------------------------------------------------------------- */

void ADC10_Read_Data(long* result){
  for(int i = 0; i < 4; i++){
    result[i] = ADC10_DMA_Data[3-i];
  }
}