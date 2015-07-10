//#include <msp430.h>
#include "io430.h"
#include "subroutine.h"
#include "rf.h"
#include "ads1292.h"
#include "ADC10.h"
#include "PacketUtil.h"
#include <string.h>

#define helloMsg "\xAA\xA5\x05\xA0\x55"
#define firmwareVersion "\xAA\xA5\x07\xA1\x01\x00\x55"
#define errMsg "\xAA\xA5\x07\xA2\x00\x00\x55"
#define lowBatteryMessage "\xAA\xA5\x07\xA3\x00\x01\x55"

void onRF_MessageReceived();
void onRF_MultiByteMessage();
void startRecording();
void stopRecording();
uchar packetDataReady = 0;
uchar lowBatteryMessageAlreadySent = 0;
uchar shutDownCntr = 0;
uchar pingCntr = 0; 
uchar rfResetCntr = 0;
uint powerUpCntr = 1;
uchar btnCntr = 0;

//������� �� ������������ RF ������ � ���������� ������ �������. 1 ���� ������� ~ 0.25 �������.
// 0 - ������������ ���������
uint resetTimeout = 0; 

int main(void)
{
  __disable_interrupt();
  sys_init();
  __delay_cycles(8000000);//������ �� �������� �������
  P1OUT |= BIT6; //����������� �������
  led(1);
  ADC10_Init();
  AFE_Init();
  rf_init();
  TACCR0 = 0xFFFF;// ������ �������
  __enable_interrupt();

 while (1)
 {
   if(rf_rx_data_ready_fg) {
     onRF_MessageReceived();
     rf_rx_data_ready_fg = 0;
   }
   if (packetDataReady){       
     uchar packetSize = assemblePacket();
     if(!rf_tx_in_progress){//�������� ���� ��� ������
       rf_send((uchar*)&packet_buf[0], packetSize);
     }
     packetDataReady = 0;      
   }
   if(rf_rx_data_ready_fg || packetDataReady){
  // ���� �� ����� �����
   }else{
   __bis_SR_register(CPUOFF + GIE); // ������ � ������ ����� 
   }
 }
} 
/*-----------------��������� ����������� � ���������� ���������--------------*/
void onRF_MessageReceived(){
    switch(rf_rx_buf[0]){
    case 0xFF: //stop recording command
      stopRecording();
      break;
    case 0xFE: //start recording command
      startRecording();
      break;
    case 0xFD: //hello command
//      memcpy (msgBuf, helloMsg, 5);
      rf_send(helloMsg,5);
      break;
    case 0xFC: //������ ��������
//      memcpy (msgBuf, firmwareVersion, 7);
      rf_send(firmwareVersion,7);
      break;
    case 0xFB: //ping command
      pingCntr = 1;
      break;
    default:
      if(rf_rx_buf[0] <= rf_rx_buf_size){//��������� ����� �������
        //��������� ��� ��������� ����� == ������ ����� ������
        if(((rf_rx_buf[rf_rx_buf[0]-1] == 0x55) && (rf_rx_buf[rf_rx_buf[0]-2] == 0x55))){
          onRF_MultiByteMessage();
        }else{
//          memcpy (msgBuf, errMsg, 7);
          rf_send(errMsg,7);
        }
      }
      break;
    }
}

void onRF_MultiByteMessage(){
  uchar msgOffset = 1;
  while (msgOffset < (rf_rx_buf[0]-2)){
    if(rf_rx_buf[msgOffset] == 0xF0){//������� ��� ads1292
      AFE_Cmd(rf_rx_buf[msgOffset+1]);
      msgOffset+=2;
    }else if(rf_rx_buf[msgOffset] == 0xF1){//������ ��������� ads1298
      AFE_Write_Reg(rf_rx_buf[msgOffset+1], rf_rx_buf[msgOffset+2], &rf_rx_buf[msgOffset+3]);
      msgOffset+=rf_rx_buf[msgOffset+2]+3;
    }else if(rf_rx_buf[msgOffset] == 0xF2){//�������� ������� ��� ads1298 8 ��������
      for(int i = 0; i<8; i++){
        if((rf_rx_buf[msgOffset+1+i] == 0) || (rf_rx_buf[msgOffset+1+i] == 1) || 
           (rf_rx_buf[msgOffset+1+i] == 2) || (rf_rx_buf[msgOffset+1+i] == 5) || (rf_rx_buf[msgOffset+1+i] == 10)){
          div[i] = rf_rx_buf[msgOffset+1+i]; 
        }
      }
      msgOffset+=9;
    }else if(rf_rx_buf[msgOffset] == 0xF3){//����� ������ �������������
      setAccelerometerMode(rf_rx_buf[msgOffset+1]);
      msgOffset+=2;
    }else if(rf_rx_buf[msgOffset] == 0xF4){//�������� ������ ���������
      measureBatteryVoltage(rf_rx_buf[msgOffset+1]);
      msgOffset+=2;
    }else if(rf_rx_buf[msgOffset] == 0xF5){//�������� ������ loff ������� 
      loffStatEnable = rf_rx_buf[msgOffset+1];
      msgOffset+=2;
    }else if(rf_rx_buf[msgOffset] == 0xF6){//RF reset timeout ��� ���������� Ping ������� � ����������. 
      resetTimeout = rf_rx_buf[msgOffset+1] * 4;
      msgOffset+=2;
    }else if(rf_rx_buf[msgOffset] == 0xFF){//stop recording command 
       stopRecording();
       msgOffset+=1;
    }else if(rf_rx_buf[msgOffset] == 0xFE){//start recording command 
       startRecording();
       msgOffset+=1;
    }else{
//      memcpy (msgBuf, errMsg, 7);
      rf_send(errMsg,7);
      return;
    }
  }
}

void startRecording(){
       powerUpCntr = 0;
       packetUtilResetCounters();
       lowBatteryMessageAlreadySent = 0;
       shutDownCntr = 0;
       pingCntr = 1;
       AFE_StartRecording();
       led(0);
}

void stopRecording(){
  powerUpCntr = 1;
  pingCntr = 0;
  AFE_StopRecording();
}

/* ------------------------ ���������� �� P1 ----------------------- */

#pragma vector = PORT1_VECTOR
__interrupt void Port1_ISR(void)
{
  if (P1IFG & AFE_DRDY_PIN) { 
    P1IFG &= ~AFE_DRDY_PIN;      // Clear DRDY flag
    long new_data[12];
    AFE_Read_Data(&new_data[0]);
    loffStat = AFE_getLoffStatus();
    ADC10_Read_Data(&new_data[8]);
    ADC10_Measure();
    if(packetAddNewData(new_data)){
      packetDataReady = 1;
      __bic_SR_register_on_exit(CPUOFF); // �� ������������ � ��� ��� ������
    }
  }
  if (P1IFG & BIT0) { 
    P1IFG &= ~BIT0;      // Clear BT connection status flag
  }
}
/* -------------------------------------------------------------------------- */
/* ------------------------- ���������� �� ������� -------------------------- */
/* -------------------------------------------------------------------------- */
#pragma vector = TIMERA1_VECTOR
__interrupt void TimerA_ISR(void)
{ 
  TACTL &= ~TAIFG;
  if(rfResetCntr == 0x01){
    P3OUT |= BIT7;//BT reset pin hi
    rfResetCntr = 0;
  }
  if(pingCntr && resetTimeout){
    pingCntr++;
    if(pingCntr > resetTimeout){//no signal from host for ~ resetTimeout * 4 seconds
      P3OUT &= ~BIT7; //BT reset pin lo
      rfResetCntr = 0x01;
      pingCntr = 1;
    }
  }
  if(!(BIT5 & P2IN)){// if power button pressed
      btnCntr++;
  }else{
      btnCntr = 0;
  }
  if(btnCntr >= 4){
        led(1);
        P1OUT &= ~BIT6; //power hold pin
        while(1){} //���� ���������� ������
  }
  
  if(!lowBatteryMessageAlreadySent){
      if(batteryVoltage < BATT_LOW_TH){
        lowBatteryMessageAlreadySent = 1;
        rf_send_after((uchar*)&lowBatteryMessage[0],7);
        shutDownCntr = 1;
      }
    }
  if(shutDownCntr){
    shutDownCntr++;
    if(shutDownCntr == 4){//wait 1 second before shut down
      P1OUT &= ~BIT6; //power hold pin
    }
  }
  if(powerUpCntr){
    powerUpCntr++;
    if(powerUpCntr >= 2400){//������ ��������� ������� (�� �������� ������ � �������� ~10 �����)
      P1OUT &= ~BIT6; //��������� �������
    }
    if(powerUpCntr%2){
      led(1);
    }else{
      led(0);
    }
  
  }
}
/* -------------------------------------------------------------------------- */ 

