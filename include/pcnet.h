#pragma once
#include "types.h"

bool pcnet_init(void);
bool pcnet_present(void);
void pcnet_get_mac(u8 *mac);
int  pcnet_send(const void *data, u32 len);
int  pcnet_receive(void *buf, u32 max);
u32  pcnet_rx_packets(void);
u32  pcnet_tx_packets(void);
u32  pcnet_rx_bytes(void);
u32  pcnet_tx_bytes(void);
u16  pcnet_io_base(void);
u8   pcnet_irq(void);
void pcnet_debug(void);
