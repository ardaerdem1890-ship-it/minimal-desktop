#pragma once
#include "types.h"

/* Intel 8254x (PRO/1000) - the NIC VirtualBox offers as
 * "Intel PRO/1000 MT Desktop" (8086:100E), "MT Server" (8086:100F) and
 * "T Server" (8086:10D3). Also QEMU's -device e1000 / e1000e. */
bool e1000_init(void);
bool e1000_present(void);
void e1000_get_mac(u8 *mac);
int  e1000_send(const void *data, u32 len);
int  e1000_receive(void *buf, u32 max);
u32  e1000_rx_packets(void);
u32  e1000_tx_packets(void);
u32  e1000_rx_bytes(void);
u32  e1000_tx_bytes(void);
u32  e1000_mmio_base(void);
u8   e1000_irq(void);
