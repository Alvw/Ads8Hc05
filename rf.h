#include "define.h"
extern uchar rf_tx_in_progress; 
extern uchar rf_data_received;
extern uchar rf_rx_buf_size;
extern uchar rf_rx_buf[51];//заменить на костанту
extern uchar rf_rx_data_ready_fg;

void rf_reset();
void rf_init();
void rf_send(uchar* cmd, uchar length);
void rf_send_after(uchar* cmd, uchar length);//отправляет данные после завершения отправки rf_send
void sendAtCommand(uchar* cmd);
