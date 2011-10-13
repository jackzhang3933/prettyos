#ifndef EHCI_H
#define EHCI_H

#include "pci.h"
#include "usb_hc.h"

#define EHCIMAX 4

/* ****** */
/* USBCMD */
/* ****** */

#define CMD_INTERRUPT_THRESHOLD        0x00FF0000 // valid values are:
#define CMD_1_MICROFRAME               BIT(16)
#define CMD_2_MICROFRAME               BIT(17)
#define CMD_4_MICROFRAME               BIT(18)
#define CMD_8_MICROFRAME               BIT(19) // 1ms
#define CMD_16_MICROFRAME              BIT(20)
#define CMD_32_MICROFRAME              BIT(21)
#define CMD_64_MICROFRAME              BIT(22)

#define CMD_PARK_MODE                  0x800
#define CMD_PARK_COUNT                 0x300
#define CMD_LIGHT_RESET                BIT(7)
#define CMD_ASYNCH_INT_DOORBELL        BIT(6)
#define CMD_ASYNCH_ENABLE              BIT(5)
#define CMD_PERIODIC_ENABLE            BIT(4)

#define CMD_FRAMELIST_SIZE             0xC     // valid values are:
#define CMD_FRAMELIST_1024             0x0
#define CMD_FRAMELIST_512              0x4
#define CMD_FRAMELIST_256              0x8

#define CMD_HCRESET                    BIT(1)  // reset
#define CMD_RUN_STOP                   BIT(0)  // run/stop


/* ************** */
/* USBSTS USBINTR */
/* ************** */

// only USBSTS
#define STS_ASYNC_ENABLED              BIT(15)
#define STS_PERIODIC_ENABLED           BIT(14)
#define STS_RECLAMATION                BIT(13)
#define STS_HCHALTED                   BIT(12)

// USBSTS (Interrupts)
#define STS_ASYNC_INT                  BIT(5)
#define STS_HOST_SYSTEM_ERROR          BIT(4)
#define STS_FRAMELIST_ROLLOVER         BIT(3)
#define STS_PORT_CHANGE                BIT(2)
#define STS_USBERRINT                  BIT(1)
#define STS_USBINT                     BIT(0)


/* *********/
/* FRINDEX */
/* *********/

#define FRI_MASK                       0x00001FFF


/* **************** */
/* PERIODICLISTBASE */
/* **************** */

#define PLB_ALIGNMENT                  0x00000FFF  // 4 KiB


/* ************* */
/* ASYNCLISTADDR */
/* ************* */

#define ALB_ALIGNMENT                  0x0000001F  // 32 Byte


/* ********** */
/* CONFIGFLAG */
/* ********** */

#define CF                             BIT(0)


/* *********** */
/* PORTSC[...] */
/* *********** */

#define PSTS_COMPANION_HC_OWNED        BIT(13) // rw
#define PSTS_POWERON                   BIT(12) // rw valid, if PPC == 1
#define PSTS_PORT_RESET                BIT(8)  // rw
#define PSTS_PORT_SUSPEND              BIT(7)  // rw
#define PSTS_PORT_RESUME               BIT(6)  // rw
#define PSTS_OVERCURRENT_CHANGE        BIT(5)  // rwc
#define PSTS_OVERCURRENT               BIT(4)  // ro
#define PSTS_ENABLED_CHANGE            BIT(3)  // rwc
#define PSTS_ENABLED                   BIT(2)  // rw
#define PSTS_CONNECTED_CHANGE          BIT(1)  // rwc
#define PSTS_CONNECTED                 BIT(0)  // ro

#define N_PORTS                        0xF     // number of ports (bits 3:0)
#define PORT_ROUTING_RULES             BIT(7)  // port routing to EHCI or cHC


typedef struct
{
    volatile uint8_t  CAPLENGTH;        // Core Capability Register Length
    volatile uint8_t  reserved;
    volatile uint16_t HCIVERSION;       // Core Interface Version Number
    volatile uint32_t HCSPARAMS;        // Core Structural Parameters //
    volatile uint32_t HCCPARAMS;        // Core Capability Parameters
    volatile uint32_t HCSPPORTROUTE_Hi; // Core Companion Port Route Description
    volatile uint32_t HCSPPORTROUTE_Lo; // Core Companion Port Route Description
} __attribute__((packed)) ehci_CapRegs_t;

/*
HCSP-PORTROUTE - Companion Port Route Description:

This optional field is valid only if Port Routing Rules field in the HCSPARAMS register is set to a one.

This field is a 15-element nibble array (each 4 bits is one array element).
Each array location corresponds one-to-one with a physical port provided by the HC
(e.g. PORTROUTE[0] corresponds to the first PORTSC port, PORTROUTE[1] to the second PORTSC port, etc.).
The value of each element indicates to which of the cHC this port is routed.
Only the first N_PORTS elements have valid information.
A value of zero indicates that the port is routed to the lowest numbered function cHC.
A value of one indicates that the port is routed to the next lowest numbered function cHC, and so on.
*/


typedef struct
{
    volatile uint32_t USBCMD;           // USB Command                     Core  // +00h
    volatile uint32_t USBSTS;           // USB Status                      Core  // +04h
    volatile uint32_t USBINTR;          // USB Interrupt Enable            Core  // +08h
    volatile uint32_t FRINDEX;          // USB Frame Index                 Core  // +0Ch
    volatile uint32_t CTRLDSSEGMENT;    // 4G Segment Selector             Core  // +10h
    volatile uint32_t PERIODICLISTBASE; // Frame List Base Address         Core  // +14h
    volatile uint32_t ASYNCLISTADDR;    // Next Asynchronous List Address  Core  // +18h
    volatile uint32_t reserved1;                                                 // +1Ch
    volatile uint32_t reserved2;                                                 // +20h
    volatile uint32_t reserved3;                                                 // +24h
    volatile uint32_t reserved4;                                                 // +28h
    volatile uint32_t reserved5;                                                 // +2Ch
    volatile uint32_t reserved6;                                                 // +30h
    volatile uint32_t reserved7;                                                 // +34h
    volatile uint32_t reserved8;                                                 // +38h
    volatile uint32_t reserved9;                                                 // +3Ch
    volatile uint32_t CONFIGFLAG;       // Configure Flag Register         Aux   // +40h
    volatile uint32_t PORTSC[16];       // Port Status/Control             Aux   // +44h
} __attribute__((packed))  ehci_OpRegs_t;

/*
Configure Flag (CF) - R/W. Default: 0. Host software sets this bit as the last action in
its process of configuring the HC. This bit controls the default port-routing control logic.
Bit values and side-effects are listed below.
0: routes each port to an implementation dependent classic HC.
1: routes all ports to the EHCI.
*/


struct ehci;

typedef struct
{
    uint8_t      num;
    port_t       port;
    struct ehci* ehci;
} ehci_port_t;

typedef struct ehci
{
    pciDev_t*        PCIdevice;         // PCI device
    uint32_t         bar;               // base address register
    ehci_OpRegs_t*   OpRegs;
    ehci_CapRegs_t*  CapRegs;
    uint8_t          num;               // number of the EHCI
    uint8_t          numPorts;          // number of ports of the EHCI
    bool             enabledPortFlag;
    bool             USBINTflag;
    bool             USBasyncIntFlag;
    struct ehci_qhd* idleQH;
    struct ehci_qhd* tailQH;
    ehci_port_t*     ports[16];
} ehci_t;


void ehci_install(pciDev_t* PCIdev, uintptr_t bar_phys);
void ehci_start();
void ehci_initHC(ehci_t* e);
void ehci_startHC(ehci_t* e);
void ehci_resetHC(ehci_t* e);
void ehci_enablePorts(ehci_t* e);
void ehci_resetPort(ehci_t* e, uint8_t port);
void ehci_portCheck();

void ehci_setupUSBDevice(ehci_t* e, uint8_t portNumber);
void ehci_showUSBSTS(ehci_t* e);


void ehci_setupTransfer(usb_transfer_t* transfer);
void ehci_setupTransaction(usb_transfer_t* transfer, usb_transaction_t* transaction, bool toggle, uint32_t tokenBytes, uint32_t type, uint32_t req, uint32_t hiVal, uint32_t loVal, uint32_t index, uint32_t length);
void ehci_inTransaction(usb_transfer_t* transfer, usb_transaction_t* transaction, bool toggle, void* buffer, size_t length);
void ehci_outTransaction(usb_transfer_t* transfer, usb_transaction_t* transaction, bool toggle, void* buffer, size_t length);
void ehci_issueTransfer(usb_transfer_t* transfer);


#endif
