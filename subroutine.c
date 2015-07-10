//#include <msp430.h>
#include "io430.h"
#include "subroutine.h"

//Инициализация микроконтроллера
void sys_init(){
  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;
  
 // CLOCK
  BCSCTL1 = CALBC1_16MHZ;                    
  DCOCTL = CALDCO_16MHZ;
  BCSCTL2 |= DIVS_3;                        // SMCLK / 8;
  
  //SMCLC output pin
  P1DIR |= BIT4; //P1.4 = output direction
  P1SEL |= BIT4; //P1.4 = SMCLK output function
  
  BCSCTL1 |= XTS;                           // ACLK = LFXT1 = HF XTAL
  BCSCTL3 |= LFXT1S1;                       // 3 – 16MHz crystal or resonator
  IE1 |= OFIE;                              // Enable osc fault interrupt 
  
  //LED
  P1DIR |= BIT7;
  P1OUT &= ~BIT7;
  
  //initialize BT_CON_STAT pin (P1.0)
  P1REN |= BIT0;         // Pull-UP/DOWN Resistors Enabled
  P1IES &= ~BIT0;       // Interrupt on rising edge
  P1IFG &= ~BIT0;       // Clear flag
  P1IE |= BIT0;         // Enable interrupt on DRDY
  
  //initialize power hold pin
  P1DIR |= BIT6;
  P1OUT &= ~BIT6; //Отключаем питание при ненажатой кнопке
  
  //RF reset pin
  P3DIR |= BIT7;
  P3OUT &= ~BIT7;
  
// Неиспользуемые выводы
  P1DIR |= BIT1 + BIT3 + BIT5;
  P1OUT &= ~(BIT1 + BIT3 + BIT5);
  
//  P2DIR |= BIT4 + BIT5;
//  P2OUT &= ~(BIT4 + BIT5);
  
  P3DIR |= BIT0;
  P3OUT &= ~BIT0;
  
  P4DIR |= BIT0 + BIT1 + BIT2 + BIT3 + BIT7;
  P4OUT &= ~(BIT0 + BIT1 + BIT2 + BIT3 + BIT7); 
  
  // Таймер  
  TACTL_bit.TACLR  = 1; // Reset TAR, divider and count dir
  TACTL = TASSEL_2;     // SMCLK
  TACTL |= ID_2 + ID_1; // 1:8  
  TACCR0 = 0x00;
  TACTL_bit.TAIE = 1;   // INT enable 
  TACTL &= ~TAIFG;      // Сброс прерывания
  TACTL |= MC_1;
}

#pragma bis_nmi_ie1=OFIE                    // Re-enable osc fault interrupt
#pragma vector=NMI_VECTOR
__interrupt void NMI_ISR(void)
{
  volatile unsigned int i;
  BCSCTL2 &= ~SELS;                       // Ensure SMCLK runs from DCO 
  do {
    IFG1 &= ~OFIFG;                         // Clear OSCFault flag
    for (i = 0xFF; i > 0; i--);             // Time for flag to set
  } while (IFG1 & OFIFG);                     // OSCFault flag still set?
  BCSCTL2 |= SELS;                        // SMCLK = LFXT1 / 8;
} 

void led(uchar state){
  if(state){
    P1OUT |= BIT7;
  }else{
    P1OUT &=~BIT7;
  }
}

/* --------------------- Индикатор "питание" -------------------- */
void Pwr_Indication()
{
  P1OUT &=~BIT7;
  for (unsigned char cntr = 0; cntr < 6; cntr++) // Мигаем 3 раза
     {
       P1OUT ^= BIT7;
       __delay_cycles(3200000);
     } 
}
/* -------------------------------------------------------------------------- */