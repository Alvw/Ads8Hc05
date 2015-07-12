//#include <msp430.h>
#include "io430.h"
#include "rf.h"
#include "string.h"
#include "subroutine.h"

uchar getStrSize(uchar* str);
//extern
uchar rf_tx_in_progress; 
uchar rf_rx_data_ready_fg;
uchar rf_rx_buf[rf_rx_buf_size];
uchar rf_rx_data_size;

uchar* rf_tx_buf;
uchar rf_tx_buf_size;
uchar* rf_tx_buf_1;
uchar rf_tx_buf_1_size = 0;

uchar rf_tx_cntr = 0;
uchar rf_rx_cntr = 0;

uchar rf_incoming_message_timeout_cntr = 0;

void rf_reset(){
  P3OUT &= ~BIT7;
  __delay_cycles(1600000);
  P3OUT |= BIT7;
}

void rf_init(){
  //Reset pin p3.7 and Programming mode pin p3.6
  P3DIR |= BIT6 + BIT7;
  P3OUT &= ~BIT6;
  rf_reset();
  
  //configure UART 460800
    P3SEL |= 0x30;                            // P3.4,5 = USCI_A0 TXD/RXD
    //UCA0CTL1 |= UCSSEL_2;                    // SMCLC
    UCA0CTL1 |= UCSSEL_1;                    // ACLC
    UCA0BR0 = 34;                            // 16,000 MHz  460800
    UCA0BR1 = 0;                             
    UCA0MCTL = UCBRS2 + UCBRS1 + UCBRS0;               	 // Modulation UCBRSx = 7
    UCA0CTL1 &= ~UCSWRST;                    // **Initialize USCI state machine**
    IE2 |= UCA0RXIE;                         // Enable USCI_A0 RX interrupt
 }

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void) {
  rf_rx_buf[rf_rx_cntr++] = UCA0RXBUF;
  switch(rf_rx_cntr){
    case 1:
      if(rf_rx_buf[0] > rf_rx_buf_size){//one byte message
        rf_rx_cntr = 0;
        rf_rx_data_ready_fg = 1;
        __bic_SR_register_on_exit(CPUOFF);
      }else{
        rf_rx_data_size = rf_rx_buf[0];
        rf_incoming_message_timeout_cntr = 1;
      }
      break;
    default:
      if(rf_rx_cntr == rf_rx_data_size){
        rf_rx_cntr = 0;
        rf_rx_data_ready_fg = 1;
        rf_incoming_message_timeout_cntr = 0;
        __bic_SR_register_on_exit(CPUOFF);
      }
      break;
  }
}

void startRFSending() {
  rf_tx_cntr = 0;
  while (!(IFG2 & UCA0TXIFG));
  IFG2 &= ~UCA0TXIFG;                     //tx flag reset!!!!!!!!!
  IE2 |= UCA0TXIE;                        // Enable USCI_A0 TX interrupt
  UCA0TXBUF = rf_tx_buf[rf_tx_cntr++];	
} 


#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void) {
  UCA0TXBUF = rf_tx_buf[rf_tx_cntr++];
  if (rf_tx_cntr > (rf_tx_buf_size - 1)) { // TX over?
    if(!rf_tx_buf_1_size){                   //nothing to send in rf_tx_buf_1
      IE2 &= ~UCA0TXIE;                     // Disable USCI_A0 TX interrupt
      rf_tx_in_progress = 0;
    }else{                                  //start sending buffered packet
      rf_send(rf_tx_buf_1,rf_tx_buf_1_size);
      rf_tx_buf_1_size = 0;
    }
  }
}

void rf_send(uchar* cmd, uchar length){
  if(rf_tx_in_progress){
    return;
  }
  rf_tx_in_progress = 1;
  rf_tx_buf = cmd;
  rf_tx_buf_size = length;
  startRFSending();
}

void rf_send_after(uchar* cmd, uchar length){
  rf_tx_buf_1 = cmd;
  rf_tx_buf_1_size = length;
}

uchar rf_delete_unfinished_incoming_messages(){
  if(rf_incoming_message_timeout_cntr){
    rf_incoming_message_timeout_cntr++;
    if(rf_incoming_message_timeout_cntr == 4){
      rf_rx_cntr = 0;
      return 1;
    }
  }
  return 0;
}
