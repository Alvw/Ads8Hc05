//#include <msp430.h>
#include "io430.h"
#include "subroutine.h"
#include "rf.h"
#include "ads1292.h"
#include "ADC10.h"
#include "PacketUtil.h"

#define helloMsg "\xAA\xA5\x05\xA0\x55"
#define firmwareVersion "\xAA\xA5\x07\xA1\x01\x00\x55"
#define unknownMultiByteCmdError "\xAA\xA5\x07\xA2\x00\x00\x55"
#define unfinishedIncomingMsgError "\xAA\xA5\x07\xA2\x00\x01\x55"
#define unknownOneByteCmdError     "\xAA\xA5\x07\xA2\x00\x02\x55" 
#define noStopMarkerError          "\xAA\xA5\x07\xA2\x00\x03\x55"
#define txFailError          "\xAA\xA5\x07\xA2\x00\x04\x55"
#define lowBatteryMessage "\xAA\xA5\x07\xA3\x00\x01\x55"
#define hardwareConfigMessage "\xAA\xA5\x07\xA4\x08\x01\x55"//reserved, power button, 8ADS channels, 1 accelerometer
#define stopRecordingResponceMessage "\xAA\xA5\x05\xA5\x55"//reserved, power button, 8ADS channels, 1 accelerometer
#define chargeShutDownMessage "\xAA\xA5\x05\xA6\x55"

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
uchar isRecording = 0;
uint batteryVoltage = 800;
uint sumBatteryVoltage = 8000;
uchar batteryCntr = 0;

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
     rf_send((uchar*)&packet_buf[0], packetSize);
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
      rf_send(stopRecordingResponceMessage,5);
      break;
    case 0xFE: //start recording command
      startRecording();
      break;
    case 0xFD: //hello command
      rf_send(helloMsg,5);
      break;
    case 0xFC: //������ ��������
      rf_send(firmwareVersion,7);
      break;
    case 0xFB: //ping command
      pingCntr = 0;
      break;
    case 0xFA: //hardware config request
     rf_send(hardwareConfigMessage,7);
      break;
    default:
      if((rf_rx_buf_size < rf_rx_buf[0]) && (rf_rx_buf[0] < 0xFA)){//��������� ����� ������������ �������
        rf_send(unknownOneByteCmdError,7);
      }
      //��������� ��� ��������� ����� == ������ ����� ������
      if(((rf_rx_buf[rf_rx_buf[0]-1] == 0x55) && (rf_rx_buf[rf_rx_buf[0]-2] == 0x55))){
        onRF_MultiByteMessage();
      }else{
        rf_send(noStopMarkerError,7);
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
    }else if(rf_rx_buf[msgOffset] == 0xFE){//start recording command 
       startRecording();
       msgOffset+=1;
    }else{
      rf_send(unknownMultiByteCmdError,7);
      return;
    }
  }
}

void startRecording(){
      isRecording = 1;
       powerUpCntr = 0;
       packetUtilResetCounters();
       lowBatteryMessageAlreadySent = 0;
       shutDownCntr = 0;
       pingCntr = 0;
       AFE_StartRecording();
       led(0);
}

void stopRecording(){
  isRecording = 0; 
  powerUpCntr = 1;
  pingCntr = 0;
  AFE_StopRecording();
}

void addBatteryData(uint battValue){
  batteryVoltage +=battValue;
  batteryCntr++;
  if(batteryCntr == 10){
      sumBatteryVoltage = batteryVoltage;
      batteryVoltage = 0;
      batteryCntr = 0;
  }
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
    addBatteryData(new_data[11]);
    ADC10_Measure();
    if(packetAddNewData(new_data)){
      packetDataReady = 1;
      __bic_SR_register_on_exit(CPUOFF); // �� ������������ � ��� ��� ������
    }
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
  if(isRecording){
    if(resetTimeout){
      pingCntr++;
      if(pingCntr > resetTimeout){//no signal from host for ~ resetTimeout * 4 seconds
        P3OUT &= ~BIT7; //BT reset pin lo
        if (rfResetCntr == 0)rfResetCntr = 1;
        pingCntr = 0;
      }
    }
  }else{//if not recording
    long new_data[4];
    ADC10_Read_Data(&new_data[0]);
    addBatteryData(new_data[3]);
    ADC10_Measure();
  }
  if(!(BIT5 & P2IN)){// if power button pressed
      btnCntr++;
  }else{
      btnCntr = 0;
  }
  if(BIT0 & P1IN){// if rf connected
      rfConStat = 1;
  }else{
      rfConStat = 0;
  }
  if(!(BIT4 & P2IN)){// if charge plug connected
    if(shutDownCntr == 0){
      shutDownCntr = 1;
      rf_send(chargeShutDownMessage,5);
    }
  }
  if(btnCntr >= (4)){ //1 ��� �������� ����� �����������
        led(1);
        P1OUT &= ~BIT6; //power hold pin
        while(1){} //���� ���������� ������
  }
  
  if(!lowBatteryMessageAlreadySent){    
      if(sumBatteryVoltage < BATT_LOW_TH){
        lowBatteryMessageAlreadySent = 1;
        rf_send(lowBatteryMessage,7);
        if (shutDownCntr == 0) shutDownCntr = 1;
      }
  }
  if(shutDownCntr){
    shutDownCntr++;
    if(shutDownCntr > 4){//wait 1 second before shut down
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
  if(rf_delete_unfinished_incoming_messages()){
    rf_send(unfinishedIncomingMsgError,7);
  }
  if(rf_tx_fail_flag){//debug information maybe delete
    rf_tx_fail_flag = 0;
    rf_send(txFailError,7);
  }
}
/* -------------------------------------------------------------------------- */ 

