//#include <msp430.h>
#include "io430.h"
#include "PacketUtil.h"
#include "string.h"

 
long buf1[PACKET_BUFF_MAX_SIZE];
long buf2[PACKET_BUFF_MAX_SIZE];
uint packet_cntr = 0;
long* add_buf = &buf1[0];
long* packet_buf = &buf2[0];
uchar div[NUMBER_OF_CHANNELS] ={1,1,1,1,1,1,1,1,10,10,10,0}; // frequency dividers for each channel. div = 0 if channel is not active.
uchar offsetCounter[NUMBER_OF_CHANNELS] = {0,0,0,0,0,0,0,0,0,0,0,0}; // offsetCounter[i]_max = ( MAX_DIV / div[i] )  - number of "long" to buffer data from channel i
uchar sumCounter[NUMBER_OF_CHANNELS]= {0,0,0,0,0,0,0,0,0,0,0,0}; // sumCounter[i]_max = div[i] - how many times we sum input data from channel i to have average output data
uchar loffStatEnable = 0;//0 - disable, 1 - enable
uint loffStat;
//uint batteryVoltage = BATT_LOW_TH;

void packetUtilResetCounters(){
  memset(offsetCounter, 0, NUMBER_OF_CHANNELS); 
  memset(sumCounter, 0, NUMBER_OF_CHANNELS); 
  packet_cntr = 0;
}

void setAccelerometerMode(uchar mode){//0 - disable, 1 - enable;
  for(uchar i = 0; i<3; i++){
    if(mode){
      div[i+8] = 10;
    }else{
      div[i+8] = 0;
    }
  }
}

void measureBatteryVoltage(uchar mode){//0 - disable, 1 - enable;
    if(mode){
      div[11] = 10;
    }else{
      div[11] = 0;
    }
}

uchar packetAddNewData(long* newData) {
  uchar isAccumulatingFinished = 0;
  uchar j = 0;
  for (uchar i = 0; i < NUMBER_OF_CHANNELS; i++) {
    if (div[i] != 0) {
      if(sumCounter[i] == 0){
        add_buf[1 + j + offsetCounter[i]] = 0;
      }
      add_buf[1 + j + offsetCounter[i]] += newData[i];
      sumCounter[i]++;
      if (sumCounter[i] == div[i]) {
        offsetCounter[i]++;
        if ((sumCounter[i] * offsetCounter[i]) == MAX_DIV) {
          isAccumulatingFinished = 1;
          offsetCounter[i] = 0;
        }
        sumCounter[i] = 0;
      }
      j += (MAX_DIV / div[i]);      // j += offsetCounter[i]_max
    }
  }
  if(isAccumulatingFinished){
    //flip buffers
    long* pBuf = add_buf;
    add_buf = packet_buf;
    packet_buf = pBuf;
  }
  return isAccumulatingFinished;
} 

uchar assemblePacket(){
  uint* intBuff = (uint *) &packet_buf[0];
  intBuff[0] = 0xAAAA;
  intBuff[1] = packet_cntr++; 
  uchar* packetCharBuff = (uchar *) &packet_buf[1];
  uchar charIndex = 0;
  uchar longIndex = 0;
  //Add ADS1298 data
  for (uchar i = 0; i < 8; i++) {// 8 channels
    if (div[i] != 0) {
      uchar numberOfSamplesInChannel = (MAX_DIV/div[i]);
      for(uchar j = 0; j < numberOfSamplesInChannel; j++){
        packet_buf[longIndex + 1]/= div[i];
        packetCharBuff[charIndex++] = packetCharBuff[longIndex*4];
        packetCharBuff[charIndex++] = packetCharBuff[longIndex*4 + 1];
        packetCharBuff[charIndex++] = packetCharBuff[longIndex*4 + 2];
        longIndex++;
      }
    }
  }
  //Add accelerometer data
  for (uchar i = 8; i < 11; i++) {// 3 channels
    if (div[i] != 0) {
        packetCharBuff[charIndex++] = packetCharBuff[longIndex*4];
        packetCharBuff[charIndex++] = packetCharBuff[longIndex*4 + 1];
        longIndex++;
    }
  }
  //Add battery data
  if (div[11] != 0) {
        packetCharBuff[charIndex++] = packetCharBuff[longIndex*4];
        packetCharBuff[charIndex++] = packetCharBuff[longIndex*4 + 1];
        longIndex++;
  }
  //Add loff status if enabled
  if(loffStatEnable){
    packetCharBuff[charIndex++] = (uchar)loffStat;
    packetCharBuff[charIndex++] = (uchar)(loffStat>>8);
  }
  //Add footer value 0x55
  packetCharBuff[charIndex++] = 0x55;
  return 4 + charIndex ;
}














