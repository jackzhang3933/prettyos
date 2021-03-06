#ifndef RTL8168_H
#define RTL8168_H

#include "network.h"


// RTL8168 register definitions
#define RTL8168_IDR0                0x00        // Mac address
//#define RTL8139_MAR0                0x08        // Multicast filter
//#define RTL8139_TXSTATUS0           0x10        // Transmit status (4 32bit regs)
#define RTL8168_TXADDR0             0x20        // Tx descriptors (also 4 32bit)
#define RTL8168_RXADDR0             0xE4        // Tx descriptors (also 4 32bit)
//#define RTL8139_RXBUF               0x30        // Receive buffer start address
//#define RTL8139_RXEARLYCNT          0x34        // Early Rx byte count
//#define RTL8139_RXEARLYSTATUS       0x36        // Early Rx status
#define RTL8168_CHIPCMD             0x37        // Command register
//#define RTL8139_RXBUFTAIL           0x38        // Current address of packet read (queue tail)
//#define RTL8139_RXBUFHEAD           0x3A        // Current buffer address (queue head)
#define RTL8168_INTRMASK            0x3C        // Interrupt mask
#define RTL8168_INTRSTATUS          0x3E        // Interrupt status
#define RTL8168_TXCONFIG            0x40        // Tx config
#define RTL8168_RXCONFIG            0x44        // Rx config
//#define RTL8139_TIMER               0x48        // A general purpose counter
//#define RTL8139_RXMISSED            0x4C        // 24 bits valid, write clears
#define RTL8168_CFG9346             0x50        // 93C46 command register
//#define RTL8139_CONFIG0             0x51        // Configuration reg 0
//#define RTL8139_CONFIG1             0x52        // Configuration reg 1
//#define RTL8139_TIMERINT            0x54        // Timer interrupt register (32 bits)
//#define RTL8139_MEDIASTATUS         0x58        // Media status register
//#define RTL8139_CONFIG3             0x59        // Config register 3
//#define RTL8139_CONFIG4             0x5A        // Config register 4
//#define RTL8139_MULTIINTR           0x5C        // Multiple interrupt select
//#define RTL8139_MII_TSAD            0x60        // Transmit status of all descriptors (16 bits)
//#define RTL8139_MII_BMCR            0x62        // Basic Mode Control Register (16 bits)
//#define RTL8139_MII_BMSR            0x64        // Basic Mode Status Register (16 bits)
//#define RTL8139_AS_ADVERT           0x66        // Auto-negotiation advertisement reg (16 bits)
//#define RTL8139_AS_LPAR             0x68        // Auto-negotiation link partner reg (16 bits)
//#define RTL8139_AS_EXPANSION        0x6A        // Auto-negotiation expansion reg (16 bits)

// RTL8168 command bits
#define RTL8168_CMD_RESET           0x10
#define RTL8168_CMD_RX_ENABLE       0x08
#define RTL8168_CMD_TX_ENABLE       0x04

// RTL8168 interrupt status bits
#define RTL8168_INT_TIMEOUT         0x4000
#define RTL8168_INT_RX_FIFO_EMPTY   0x0200
#define RTL8168_INT_SOFTWARE_INT    0x0100
#define RTL8168_INT_TXDESC_UNAVAIL  0x0080
#define RTL8168_INT_RXFIFO_OVERFLOW 0x0040
#define RTL8168_INT_LINK_CHANGE     0x0020
#define RTL8168_INT_RXDESC_UNAVAIL  0x0010
#define RTL8168_INT_TX_ERR          0x0008
#define RTL8168_INT_TX_OK           0x0004
#define RTL8168_INT_RX_ERR          0x0002
#define RTL8168_INT_RX_OK           0x0001

//// RTL8139C transmit status bits
//#define RTL8139_TX_CARRIER_LOST     0x80000000    // Carrier sense lost
//#define RTL8139_TX_ABORTED          0x40000000    // Transmission aborted
//#define RTL8139_TX_OUT_OF_WINDOW    0x20000000    // Out of window collision
//#define RTL8139_TX_STATUS_OK        0x00008000    // Status ok: a good packet was transmitted
//#define RTL8139_TX_UNDERRUN         0x00004000    // Transmit FIFO underrun
//#define RTL8139_TX_HOST_OWNS        0x00002000    // Set to 1 when DMA operation is completed
//#define RTL8139_TX_SIZE_MASK        0x00001FFF    // Descriptor size mask

// RTL8139C receive status bits
//#define RTL8139_RX_MULTICAST        0x00008000    // Multicast packet
//#define RTL8139_RX_PAM              0x00004000    // Physical address matched
//#define RTL8139_RX_BROADCAST        0x00002000    // Broadcast address matched
//#define RTL8139_RX_BAD_SYMBOL       0x00000020    // Invalid symbol in 100TX packet
//#define RTL8139_RX_RUNT             0x00000010    // Packet size is <64 bytes
//#define RTL8139_RX_TOO_LONG         0x00000008    // Packet size is >4K bytes
//#define RTL8139_RX_CRC_ERR          0x00000004    // CRC error
//#define RTL8139_RX_FRAME_ALIGN      0x00000002    // Frame alignment error
//#define RTL8139_RX_STATUS_OK        0x00000001    // Status ok: a good packet was received


typedef struct
{
    uint32_t command;  // command/status dword
    uint32_t vlan;     // currently unused
    uint32_t low_buf;  // low 32-bits of physical buffer address
    uint32_t high_buf; // high 32-bits of physical buffer address
} __attribute__((packed)) RTL8168_Desc;

typedef struct
{
    network_adapter_t* device;
    RTL8168_Desc*      Rx_Descriptors;
    RTL8168_Desc*      Tx_Descriptors;
    uint8_t*           RxBuffer;
    void*              MMIO_base;
} RTL8168_networkAdapter_t;


void rtl8168_install(network_adapter_t* device);
void rtl8168_handler(registers_t* data, pciDev_t* device);


#endif
