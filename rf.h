#include "define.h"

#define rf_rx_buf_size 51
extern uchar rf_data_received;
extern uchar rf_rx_buf[rf_rx_buf_size];
extern uchar rf_rx_data_ready_fg;
extern uchar rf_tx_fail_flag;
extern uchar rfConStat;

void rf_reset();
void rf_init();
void rf_send(uchar* cmd, uchar length);
uchar rf_delete_unfinished_incoming_messages();
