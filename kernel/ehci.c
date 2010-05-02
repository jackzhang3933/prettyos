/*
*  license and disclaimer for the use of this source code as per statement below
*  Lizenz und Haftungsausschluss f?r die Verwendung dieses Sourcecodes siehe unten
*/

#include "util.h"
#include "timer.h"
#include "pci.h"
#include "ehci.h"
#include "kheap.h"
#include "task.h"
#include "usb2.h"
#include "event_list.h"
#include "video.h"
#include "irq.h"
#include "video.h"

/// TEST
/// const uint8_t PORTRESET = 3; /// TEST: only one port is reset!!! PORTRESET+1 is the indicated port
#define PORTRESET j
/// TEST

struct ehci_CapRegs* pCapRegs; // = &CapRegs;
struct ehci_OpRegs*  pOpRegs;  // = &OpRegs;

bool      EHCIflag; // signals that one EHCI device was found /// TODO: manage more than one EHCI
bool      USBINTflag; // signals STS_USBINT reset by EHCI handler

uint8_t   numPorts;
uintptr_t eecp;
uint8_t*  inBuffer;
void*     DataQTD;
void*     SetupQTD;
uintptr_t DataQTDpage0;
uintptr_t MSDStatusQTDpage0;
uintptr_t SetupQTDpage0;

// pci devices list
extern pciDev_t pciDev_Array[PCIARRAYSIZE];
bool USBtransferFlag; // switch on/off tests for USB-Transfer
bool enabledPortFlag; // port enabled

// usb devices list
extern usb2_Device_t usbDevices[17]; // ports 1-16 // 0 not used

static void waitForKeyStroke()
{
   textColor(0x0D);
   printf("\n>>> Press key to go on with USB-Test. <<<");
   textColor(0x0F);
   while(!keyboard_getChar());
   printf("\n");
}

void ehci_install(uint32_t num, uint32_t i)
{
    uintptr_t bar_phys = pciDev_Array[num].bar[i].baseAddress & 0xFFFFFFF0;

    uintptr_t bar = (uintptr_t) paging_acquire_pcimem(bar_phys);
    uintptr_t offset = bar_phys%PAGESIZE;
    printf("\nEHCI_MMIO %X mapped to virt addr %X, offset: %x\n", bar_phys, bar, offset);

    if (!EHCIflag) // only the first EHCI is used
    {
        ODA.pciEHCInumber = num; /// TODO: implement for more than one EHCI
        EHCIflag = true; // only the first EHCI is used
        if(ODA.pciEHCInumber)
        {
            addEvent(&EHCI_INIT);
        }
        analyzeEHCI(bar,offset); // get data (capregs, opregs)
    }
}

void ehci_init()
{
    create_cthread(&startEHCI, "EHCI");
}
void ehci_portcheck()
{
    create_cthread(&portCheck, "EHCI Ports");
}

void createQH(void* address, uint32_t horizPtr, void* firstQTD, uint8_t H, uint32_t device, uint32_t endpoint, uint32_t packetSize)
{
    struct ehci_qhd* head = (struct ehci_qhd*)address;
    memset(address, 0, sizeof(struct ehci_qhd));

    head->horizontalPointer      =   horizPtr | 0x2; // bit 2:1   00b iTD,   01b QH,   10b siTD,   11b FSTN
    head->deviceAddress          =   device;         // The device address
    head->inactive               =   0;
    head->endpoint               =   endpoint;       // Endpoint 0 contains Device infos such as name
    head->endpointSpeed          =   2;              // 00b = full speed; 01b = low speed; 10b = high speed
    head->dataToggleControl      =   1;              // Get the Data Toggle bit out of the included qTD
    head->H                      =   H;
    head->maxPacketLength        =   packetSize;     // It's 64 bytes for a control transfer to a high speed device.
    head->controlEndpointFlag    =   0;              // Only used if Endpoint is a control endpoint and not high speed
    head->nakCountReload         =   0;              // This value is used by HC to reload the Nak Counter field.
    head->interruptScheduleMask  =   0;              // not used for async schedule
    head->splitCompletionMask    =   0;              // unused if (not low/full speed and in periodic schedule)
    head->hubAddr                =   0;              // unused if high speed (Split transfer)
    head->portNumber             =   0;              // unused if high speed (Split transfer)
    head->mult                   =   1;              // One transaction to be issued for this endpoint per micro-frame.
                                                     // Maybe unused for non interrupt queue head in async list
    if (firstQTD == NULL)
    {
        head->qtd.next = 0x1;
    }
    else
    {
        uint32_t physNext = paging_get_phys_addr(kernel_pd, firstQTD);
        head->qtd.next = physNext;
    }
}

void* createQTD_SETUP(uintptr_t next, bool toggle, uint32_t tokenBytes, uint32_t type, uint32_t req, uint32_t hiVal, uint32_t loVal, uint32_t index, uint32_t length)
{
    void* address = malloc(sizeof(struct ehci_qtd), 0x20); // 32 Byte alignment
    struct ehci_qtd* td = (struct ehci_qtd*)address;

    if (next != 0x1)
    {
        uint32_t phys = paging_get_phys_addr(kernel_pd, (void*)next);
        td->next = phys;
    }
    else
    {
        td->next = 0x1;
    }
    td->nextAlt = td->next; /// 0x1;     // No alternative next, so T-Bit is set to 1
    td->token.status       = 0x80;       // This will be filled by the Host Controller
    td->token.pid          = 0x2;        // SETUP = 2
    td->token.errorCounter = 0x3;        // Written by the Host Controller.
    td->token.currPage     = 0x0;        // Start with first page. After that it's written by Host Controller???
    td->token.interrupt    = 0x1;        // We want an interrupt after complete transfer
    td->token.bytes        = tokenBytes; // dependent on transfer
    td->token.dataToggle   = toggle;     // Should be toggled every list entry

    void* data = malloc(PAGESIZE, PAGESIZE); // Enough for a full page
    memset(data,0,PAGESIZE);

    SetupQTDpage0  = (uintptr_t)data;

    struct ehci_request* request = (struct ehci_request*)data;
    request->type    = type;    // 0x80;   // Device->Host
    request->request = req;     // 0x6;    // GET_DESCRIPTOR
    request->valueHi = hiVal;   // Type:1 (Device)
    request->valueLo = loVal;   // Index: 0, used only for String or Configuration descriptors
    request->index   = index;   // 0;    // Language ID: Default
    request->length  = length;  // 18;    // according to the the requested data bytes in IN qTD

    uint32_t dataPhysical = paging_get_phys_addr(kernel_pd, data);
    td->buffer0 = dataPhysical;
    td->buffer1 = 0x0;
    td->buffer2 = 0x0;
    td->buffer3 = 0x0;
    td->buffer4 = 0x0;
    td->extend0 = td->extend1 = td->extend2 = td->extend3 = td->extend4 = 0x0;

    return address;
}

void* createQTD_MSD(uintptr_t next, bool toggle, uint32_t tokenBytes, uint32_t type, uint32_t req, uint32_t hiVal, uint32_t loVal, uint32_t index, uint32_t length)
{
    void* address = malloc(sizeof(struct ehci_qtd), 0x20); // 32 Byte alignment
    struct ehci_qtd* td = (struct ehci_qtd*)address;

    if (next != 0x1)
    {
        uint32_t phys = paging_get_phys_addr(kernel_pd, (void*)next);
        td->next = phys;
    }
    else
    {
        td->next = 0x1;
    }
    td->nextAlt = td->next; /// 0x1;     // No alternative next, so T-Bit is set to 1
    td->token.status       = 0x80;       // This will be filled by the Host Controller
    td->token.pid          = 0x2;        // SETUP = 2
    td->token.errorCounter = 0x3;        // Written by the Host Controller.
    td->token.currPage     = 0x0;        // Start with first page. After that it's written by Host Controller???
    td->token.interrupt    = 0x1;        // We want an interrupt after complete transfer
    td->token.bytes        = tokenBytes; // dependent on transfer
    td->token.dataToggle   = toggle;     // Should be toggled every list entry

    void* data = malloc(PAGESIZE, PAGESIZE); // Enough for a full page
    memset(data,0,PAGESIZE);

    SetupQTDpage0  = (uintptr_t)data;

    struct ehci_request* request = (struct ehci_request*)data;
    request->type    = type;
    request->request = req;
    request->valueHi = hiVal;
    request->valueLo = loVal;
    request->index   = index;
    request->length  = length;

    uint32_t dataPhysical = paging_get_phys_addr(kernel_pd, data);
    td->buffer0 = dataPhysical;
    td->buffer1 = 0x0;
    td->buffer2 = 0x0;
    td->buffer3 = 0x0;
    td->buffer4 = 0x0;
    td->extend0 = td->extend1 = td->extend2 = td->extend3 = td->extend4 = 0x0;

    return address;
}

void* createQTD_IO(uintptr_t next, uint8_t direction, bool toggle, uint32_t tokenBytes)
{
    void* address = malloc(sizeof(struct ehci_qtd), 0x20); // 32 Byte alignment
    struct ehci_qtd* td = (struct ehci_qtd*)address;

    if (next != 0x1)
    {
        uint32_t phys = paging_get_phys_addr(kernel_pd, (void*)next);
        td->next = phys;
    }
    else
    {
        td->next = 0x1;
    }
    td->nextAlt = td->next; /// 0x1;     // No alternative next, so T-Bit is set to 1
    td->token.status       = 0x80;       // This will be filled by the Host Controller
    td->token.pid          = direction;  // OUT = 0, IN = 1
    td->token.errorCounter = 0x3;        // Written by the Host Controller.
    td->token.currPage     = 0x0;        // Start with first page. After that it's written by Host Controller???
    td->token.interrupt    = 0x1;        // We want an interrupt after complete transfer
    td->token.bytes        = tokenBytes; // dependent on transfer
    td->token.dataToggle   = toggle;     // Should be toggled every list entry

    void* data = malloc(PAGESIZE, PAGESIZE); // Enough for a full page
    memset(data,0,PAGESIZE);

    DataQTDpage0  = (uintptr_t)data;
    inBuffer = data;

    uint32_t dataPhysical = paging_get_phys_addr(kernel_pd, data);
    td->buffer0 = dataPhysical;
    td->buffer1 = 0x0;
    td->buffer2 = 0x0;
    td->buffer3 = 0x0;
    td->buffer4 = 0x0;
    td->extend0 = td->extend1 = td->extend2 = td->extend3 = td->extend4 = 0x0;

    return address;
}

void* createQTD_HS(uint8_t direction)
{
    uintptr_t next      = 0x1;
    uint32_t tokenBytes = 0;
    bool toggle         = 1;
    
    void* address = malloc(sizeof(struct ehci_qtd), 0x20); // 32 Byte alignment
    struct ehci_qtd* td = (struct ehci_qtd*)address;
 
    td->next = next;                     // No next, so T-Bit is set to 1
    td->nextAlt = td->next;              // No alternative next, so T-Bit is set to 1
    td->token.status       = 0x80;       // This will be filled by the Host Controller
    td->token.pid          = direction;  // OUT = 0, IN = 1
    td->token.errorCounter = 0x3;        // Written by the Host Controller.
    td->token.currPage     = 0x0;        // Start with first page. After that it's written by Host Controller???
    td->token.interrupt    = 0x1;        // We want an interrupt after complete transfer
    td->token.bytes        = tokenBytes; // 0 for handshake
    td->token.dataToggle   = toggle;     // 1 for handshake

    void* data = malloc(PAGESIZE, PAGESIZE); // Enough for a full page
    memset(data,0,PAGESIZE);
    
    uint32_t dataPhysical = paging_get_phys_addr(kernel_pd, data);
    
    td->buffer0 = dataPhysical;
    td->buffer1 = td->buffer2 = td->buffer3 = td->buffer4 = 0x0;
    td->extend0 = td->extend1 = td->extend2 = td->extend3 = td->extend4 = 0x0;

    return address;
}

void* createQTD_MSDStatus(uintptr_t next, bool toggle)
{
    void* address = malloc(sizeof(struct ehci_qtd), 0x20); // 32 Byte alignment
    struct ehci_qtd* td = (struct ehci_qtd*)address;

    if (next != 0x1)
    {
        uint32_t phys = paging_get_phys_addr(kernel_pd, (void*)next);
        td->next = phys;
    }
    else
    {
        td->next = 0x1;
    }
    td->nextAlt = td->next; /// 0x1;     // No alternative next, so T-Bit is set to 1
    td->token.status       = 0x80;       // This will be filled by the Host Controller
    td->token.pid          = IN;         // OUT = 0, IN = 1
    td->token.errorCounter = 0x3;        // Written by the Host Controller.
    td->token.currPage     = 0x0;        // Start with first page. After that it's written by Host Controller???
    td->token.interrupt    = 0x1;        // We want an interrupt after complete transfer
    td->token.bytes        = 13;         // dependent on transfer, here 13 byte status information
    td->token.dataToggle   = toggle;     // Should be toggled every list entry

    void* data = malloc(PAGESIZE, PAGESIZE); // Enough for a full page
    memset(data,0,PAGESIZE);

    MSDStatusQTDpage0  = (uintptr_t)data;
    (*(((uint32_t*)MSDStatusQTDpage0)+0)) = 0x53425355; // magic USBS
    (*(((uint32_t*)MSDStatusQTDpage0)+1)) = 0xAAAAAAAA; // CSWTag
    (*(((uint32_t*)MSDStatusQTDpage0)+2)) = 0xAAAAAAAA; //
    (*(((uint32_t*)MSDStatusQTDpage0)+3)) = 0xFFFFFFAA; //
    
    uint32_t dataPhysical = paging_get_phys_addr(kernel_pd, data);
    td->buffer0 = dataPhysical;
    td->buffer1 = 0x0;
    td->buffer2 = 0x0;
    td->buffer3 = 0x0;
    td->buffer4 = 0x0;
    td->extend0 = td->extend1 = td->extend2 = td->extend3 = td->extend4 = 0x0;

    return address;
}



void showPacket(uint32_t virtAddrBuf0, uint32_t size)
{
    #ifdef _USB_DIAGNOSIS_
    printf("virtAddrBuf0 %X : ", virtAddrBuf0);
    #endif
    for (uint32_t c=0; c<size; c++)
    {
        textColor(0x03);
        printf("%y ", *((uint8_t*)virtAddrBuf0+c));
        textColor(0x0F);
    }
    printf("\n");
}

void showPacketAlphaNumeric(uint32_t virtAddrBuf0, uint32_t size)
{
    #ifdef _USB_DIAGNOSIS_
    printf("virtAddrBuf0 %X : ", virtAddrBuf0);
    #endif
    for (uint32_t c=0; c<size; c++)
    {
        textColor(0x0F);
        if ( (*((uint8_t*)virtAddrBuf0+c)>=0x20) && (*((uint8_t*)virtAddrBuf0+c)<=0x7E) )
        {
            printf("%c", *((uint8_t*)virtAddrBuf0+c));
        }
    }
    printf("\n");
}

void showStatusbyteQTD(void* addressQTD)
{
    textColor(0x0F);
    uint8_t statusbyte = *((uint8_t*)addressQTD+8);
    printf("\nqTD Status: %y\t", statusbyte);

    // analyze status byte (cf. EHCI 1.0 spec, Table 3-16 Status in qTD Token)
    
    // Status not OK
    textColor(0x0E);
    if (statusbyte & (1<<7)) { printf("Active - HC transactions enabled"); }
    if (statusbyte & (1<<6)) { printf("Halted - serious error at the device/endpoint"); }
    if (statusbyte & (1<<5)) { printf("Data Buffer Error (overrun or underrun)"); }
    if (statusbyte & (1<<4)) { printf("Babble (fatal error leads to Halted)"); }
    if (statusbyte & (1<<3)) { printf("Transaction Error (XactErr)- host received no valid response device"); }
    if (statusbyte & (1<<2)) { printf("Missed Micro-Frame"); }
    if (statusbyte & (1<<1)) { printf("Do Complete Split"); }
    if (statusbyte & (1<<0)) { printf("Do Ping"); }
    textColor(0x0A);
    
    // Status OK
    if (statusbyte == 0)     { printf("OK (no bit set)"); }
    textColor(0x0F);
}

void ehci_handler(registers_t* r)
{
    if (!(pOpRegs->USBSTS & STS_FRAMELIST_ROLLOVER) && !(pOpRegs->USBSTS & STS_USBINT))
    {
      textColor(0x09);
      printf("\nehci_handler: ");
      textColor(0x0F);
    }

    textColor(0x0E);

    if (pOpRegs->USBSTS & STS_USBINT)
    {
        USBINTflag = true; // is asked by polling
        // printf("USB Interrupt");
        pOpRegs->USBSTS |= STS_USBINT;
    }

    if (pOpRegs->USBSTS & STS_USBERRINT)
    {
        printf("USB Error Interrupt");
        pOpRegs->USBSTS |= STS_USBERRINT;
    }

    if (pOpRegs->USBSTS & STS_PORT_CHANGE)
    {
        textColor(0x09);
        printf("Port Change");
        textColor(0x0F);

        pOpRegs->USBSTS |= STS_PORT_CHANGE;

        if (enabledPortFlag && ODA.pciEHCInumber)
        {
            addEvent(&EHCI_PORTCHECK);
        }
    }

    if (pOpRegs->USBSTS & STS_FRAMELIST_ROLLOVER)
    {
        //printf("Frame List Rollover Interrupt");
        pOpRegs->USBSTS |= STS_FRAMELIST_ROLLOVER;
    }

    if (pOpRegs->USBSTS & STS_HOST_SYSTEM_ERROR)
    {
        textColor(0x04);
        printf("Host System Error");
        textColor(0x0F);
        pOpRegs->USBCMD &= ~CMD_ASYNCH_ENABLE; // necessary?
        pOpRegs->USBSTS |= STS_HOST_SYSTEM_ERROR;
        analyzeHostSystemError(ODA.pciEHCInumber);
        textColor(0x0E);
        printf("\n>>> Init EHCI after fatal error:           <<<");
         printf("\n>>> Press key for EHCI (re)initialization. <<<");
        while(!keyboard_getChar());
        textColor(0x0F);
        addEvent(&EHCI_INIT);
    }

    if (pOpRegs->USBSTS & STS_ASYNC_INT)
    {
        printf("Interrupt on Async Advance");
        pOpRegs->USBSTS |= STS_ASYNC_INT;
    }
}

void analyzeEHCI(uintptr_t bar, uintptr_t offset)
{
    bar += offset;

    /// TEST
    uintptr_t bar_phys = (uintptr_t)paging_get_phys_addr(kernel_pd, (void*)bar);
    printf("EHCI bar get_phys_Addr: %X\n", bar_phys);
    /// TEST

    pCapRegs = (struct ehci_CapRegs*) bar;
    pOpRegs  = (struct ehci_OpRegs*) (bar + pCapRegs->CAPLENGTH);
    numPorts = (pCapRegs->HCSPARAMS & 0x000F);

    printf("HCIVERSION: %x ",  pCapRegs->HCIVERSION);               // Interface Version Number
    printf("HCSPARAMS: %X ",   pCapRegs->HCSPARAMS);                // Structural Parameters
    printf("Ports: %d ",       numPorts);                           // Number of Ports
    printf("\nHCCPARAMS: %X ", pCapRegs->HCCPARAMS);                // Capability Parameters
    if (BYTE2(pCapRegs->HCCPARAMS)==0) printf("No ext. capabil. "); // Extended Capabilities Pointer
    printf("\nOpRegs Address: %X ", pOpRegs);                       // Host Controller Operational Registers
}

void resetHostController()
{
    /*
    Intel Intel® 82801EB (ICH5), 82801ER (ICH5R), and 82801DB (ICH4)
    Enhanced Host Controller Interface (EHCI) Programmer’s Reference Manual (PRM) April 2003

    To initiate a host controller reset
    system software must:
    */

    // 1. Stop the host controller.
    //    System software must program the USB2CMD.Run/Stop bit to 0 to stop the host controller.
    pOpRegs->USBCMD &= ~CMD_RUN_STOP;            // set Run-Stop-Bit to 0

    /*
    2. Wait for the host controller to halt.
       To determine when the host controller has halted, system software must read the USB2STS.HCHalted bit;
       the host controller will set this bit to 1 as soon as
       it has successfully transitioned from a running state to a stopped state (halted).
       Attempting to reset an actively running host controller will result in undefined behavior.
    */
    while (!(pOpRegs->USBSTS & STS_HCHALTED))
    {
        sleepMilliSeconds(30); // wait at least 16 microframes (= 16*125 micro-sec = 2 ms)
    }

    // 3. Program the USB2CMD.HostControllerReset bit to a 1.
    //    This will cause the host controller to begin the host controller reset.
    pOpRegs->USBCMD |= CMD_HCRESET;              // set Reset-Bit to 1

    // 4. Wait until the host controller has completed its reset.
    // To determine when the reset is complete, system software must read the USB2CMD.HostControllerReset bit;
    // the host controller will set this bit to 0 upon completion of the reset.

    int32_t timeout=10;
    while ((pOpRegs->USBCMD & CMD_HCRESET) != 0) // Reset-Bit still set to 1
    {
        printf("waiting for HC reset\n");
        sleepMilliSeconds(20);
        timeout--;
        if (timeout<=0)
        {
            textColor(0x04);
            printf("Error: HC Reset-Bit still set to 1\n");
            textColor(0x0F);
            break;
        }
    }
}

void startHostController(uint32_t num)
{
    textColor(0x09);
    printf("\n>>> >>> function: startHostController (reset HC)");
    textColor(0x0F);

    resetHostController();

    /*
    Intel Intel® 82801EB (ICH5), 82801ER (ICH5R), and 82801DB (ICH4)
    Enhanced Host Controller Interface (EHCI) Programmer’s Reference Manual (PRM) April 2003:
    After the reset has completed, the system software must reinitialize the host controller so as to
    return the host controller to an operational state (See Section 4.3.3.3, Post-Operating System Initialization)

    ... software must complete the controller initialization by performing the following steps:
    */

    // 1. Claim/request ownership of the EHCI. This process is described in detail in Section 5 - EHCI Ownership.
    DeactivateLegacySupport(num);

    // 2. Program the CTRLDSSEGMENT register. This value must be programmed since the ICH4/5 only uses 64bit addressing
    //    (See Section 4.3.3.1.2-HCCPARAMS – Host Controller Capability Parameters).
    //    This register must be programmed before the periodic and asynchronous schedules are enabled.
    pOpRegs->CTRLDSSEGMENT = 0; // Program the CTRLDSSEGMENT register with 4-GiB-segment where all of the interface data structures are allocated.

    // 3. Determine which events should cause an interrupt. System software programs the USB2INTR register
    //    with the appropriate value. See Section 9 - Hardware Interrupt Routing - for additional details.
    // pOpRegs->USBINTR = STS_INTMASK; // all interrupts allowed
    pOpRegs->USBINTR = STS_ASYNC_INT|STS_HOST_SYSTEM_ERROR|STS_PORT_CHANGE|STS_USBERRINT|STS_USBINT/*|STS_FRAMELIST_ROLLOVER*/;

    // 4. Program the USB2CMD.InterruptThresholdControl bits to set the desired interrupt threshold
    pOpRegs->USBCMD |= CMD_8_MICROFRAME;

    //    and turn the host controller ON via setting the USB2CMD.Run/Stop bit. Setting the Run/Stop
    //    bit with both the periodic and asynchronous schedules disabled will still allow interrupts and
    //    enabled port events to be visible to software
    if (pOpRegs->USBSTS & STS_HCHALTED)
    {
        pOpRegs->USBCMD |= CMD_RUN_STOP; // set Run-Stop-Bit
    }

    // 5. Program the Configure Flag to a 1 to route all ports to the EHCI controller. Because setting
    //    this flag causes all ports to be unconditionally routed to the EHCI, all USB1.1 devices will
    //    cease to function until the bus is properly enumerated (i.e., each port is properly routed to its
    //    associated controller type: UHCI or EHCI)
    pOpRegs->CONFIGFLAG = CF; // Write a 1 to CONFIGFLAG register to route all ports to the EHCI controller

    sleepMilliSeconds(100); // do not delete
}

int32_t initEHCIHostController()
{
    textColor(0x09);
    printf("\n>>> >>> function: initEHCIHostController");
    textColor(0x0F);

    // pci bus data
    uint32_t num = ODA.pciEHCInumber;
    uint8_t bus  = pciDev_Array[num].bus;
    uint8_t dev  = pciDev_Array[num].device;
    uint8_t func = pciDev_Array[num].func;
    uint8_t irq  = pciDev_Array[num].irq;
    // prepare PCI command register // offset 0x04
    // bit 9 (0x0200): Fast Back-to-Back Enable // not necessary
    // bit 2 (0x0004): Bus Master               // cf. http://forum.osdev.org/viewtopic.php?f=1&t=20255&start=0
    uint16_t pciCommandRegister = pci_config_read(bus, dev, func, 0x0204);
    printf("\nPCI Command Register before:          %x", pciCommandRegister);
    pci_config_write_dword(bus, dev, func, 0x04, pciCommandRegister /*already set*/ | 1<<2 /* bus master */); // resets status register, sets command register
    printf("\nPCI Command Register plus bus master: %x", pci_config_read(bus, dev, func, 0x0204));

    uint16_t pciCapabilitiesList = pci_config_read(bus, dev, func, 0x0234);
    printf("\nPCI Capabilities List: first Pointer: %x", pciCapabilitiesList);

    if (pciCapabilitiesList) // pointer != NULL
    {
        uint16_t nextCapability = pci_config_read(bus, dev, func, 0x0200 | pciCapabilitiesList);
        printf("\nPCI Capabilities List: ID: %y, next Pointer: %y",BYTE1(nextCapability),BYTE2(nextCapability));

        while (BYTE2(nextCapability)) // pointer != NULL
        {
            nextCapability = pci_config_read(bus, dev, func, 0x0200 | BYTE2(nextCapability));
            printf("\nPCI Capabilities List: ID: %y, next Pointer: %y",BYTE1(nextCapability),BYTE2(nextCapability));
        }
    }

    irq_install_handler(32 + irq,   ehci_handler);
    irq_install_handler(32 + irq-1, ehci_handler); /// work-around for VirtualBox Bug!

    USBtransferFlag = true;
    enabledPortFlag = false;
    startHostController(num);

    if (!(pOpRegs->USBSTS & STS_HCHALTED)) // TEST
    {
         // textColor(0x02);
         // printf("\nHCHalted bit set to 0 (OK), ports can be enabled now.");
         enablePorts();
         // textColor(0x0F);
    }
    else // not OK
    {
         textColor(0x04);
         printf("\nHCHalted bit set to 1 (Not OK!), fatal Error --> Ports cannot be enabled");
         showUSBSTS();
         textColor(0x0F);
         return -1;
    }
    return 0;
}

void enablePorts()
{
    textColor(0x09);
    printf("\n>>> >>> function: enablePorts");
    textColor(0x0F);

    for (uint8_t j=0; j<numPorts; j++)
    {
         // resetPort(j);
         resetPort(PORTRESET);

         //if ( pOpRegs->PORTSC[j] == (PSTS_POWERON | PSTS_ENABLED | PSTS_CONNECTED)  ) // high speed idle, enabled, SE0
         if ( pOpRegs->PORTSC[PORTRESET] == (PSTS_POWERON | PSTS_ENABLED | PSTS_CONNECTED) ) // high speed, enabled, device attached
         {
             textColor(0x0E);
             printf("Port %d: high speed enabled, device attached\n",j+1);
             textColor(0x0F);

             /*
             if (USBtransferFlag)
             {
                 // only port change activated for USB transfer
             }
             */
         }
    }
    enabledPortFlag = true;
}

void resetPort(uint8_t j)
{
    textColor(0x09);
    printf("\n>>> >>> function: resetPort %d  ",j+1);
    textColor(0x0F);

    pOpRegs->PORTSC[j] |=  PSTS_POWERON;

    /*
     http://www.intel.com/technology/usb/download/ehci-r10.pdf
     When software writes a one to this bit (from a zero),
     the bus reset sequence as defined in the USB Specification Revision 2.0 is started.
     Software writes a zero to this bit to terminate the bus reset sequence.
     Software must keep this bit at a one long enough to ensure the reset sequence,
     as specified in the USB Specification Revision 2.0, completes.
     Note: when software writes this bit to a one,
     it must also write a zero to the Port Enable bit.
    */
    pOpRegs->PORTSC[j] &= ~PSTS_ENABLED;

    /*
     The HCHalted bit in the USBSTS register should be a zero
     before software attempts to use this bit.
     The host controller may hold Port Reset asserted to a one
     when the HCHalted bit is a one.
    */
    if (pOpRegs->USBSTS & STS_HCHALTED) // TEST
    {
         textColor(0x04);
         printf("\nHCHalted set to 1 (Not OK!)");
         showUSBSTS();
         textColor(0x0F);
    }

    pOpRegs->USBINTR = 0;
    pOpRegs->PORTSC[j] |=  PSTS_PORT_RESET; // start reset sequence
    sleepMilliSeconds(250); // do not delete         // wait
    pOpRegs->PORTSC[j] &= ~PSTS_PORT_RESET; // stop reset sequence

    // wait and check, whether really zero
    uint32_t timeout=20;
    while ((pOpRegs->PORTSC[j] & PSTS_PORT_RESET) != 0)
    {
        sleepMilliSeconds(20);
        timeout--;
        if (timeout <= 0)
        {
            textColor(0x04);
            printf("\nerror: port %d did not reset! ",j+1);
            textColor(0x0F);
            printf("PortStatus: %X",pOpRegs->PORTSC[j]);
            break;
        }
    }
    pOpRegs->USBINTR = STS_INTMASK;
}

void showPORTSC()
{
    for (uint8_t j=0; j<numPorts; j++)
    {
        //if (pOpRegs->PORTSC[j] & PSTS_CONNECTED_CHANGE)
        if (pOpRegs->PORTSC[PORTRESET] & PSTS_CONNECTED_CHANGE)
        {
            char PortStatus[20];

            // if (pOpRegs->PORTSC[j] & PSTS_CONNECTED)
            if (pOpRegs->PORTSC[PORTRESET] & PSTS_CONNECTED)
            {
                strcpy(PortStatus,"attached");

                /*
                resetPort(j);
                checkPortLineStatus(j);
                */
                resetPort(PORTRESET);
                checkPortLineStatus(PORTRESET);

            }
            else
            {
                strcpy(PortStatus,"not attached");
            }
            pOpRegs->PORTSC[j] |= PSTS_CONNECTED_CHANGE; // reset interrupt

            writeInfo(0, "Port: %i, device %s", j+1, PortStatus);

            // beep(1000,100);
        }
    }
}

void portCheck()
{
    showInfobar(true); // protect console against info area
    showPORTSC(); // with resetPort(j) and checkPortLineStatus(j), if PORTSC: 1005h
    textColor(0x0D);
    printf("\n>>> Press key to close this console. <<<");
    textColor(0x0F);
    while(!keyboard_getChar());
}

void startEHCI()
{
    initEHCIHostController();
    textColor(0x0D);
    printf("\n>>> Press key to close this console. <<<");
    textColor(0x0F);
    while(!keyboard_getChar());
}

void showUSBSTS()
{
    printf("\nUSB status: ");
    textColor(0x02);
    printf("%X",pOpRegs->USBSTS);
    textColor(0x0E);
    if (pOpRegs->USBSTS & STS_USBINT)             { printf("\nUSB Interrupt");                 pOpRegs->USBSTS |= STS_USBINT;              }
    if (pOpRegs->USBSTS & STS_USBERRINT)          { printf("\nUSB Error Interrupt");           pOpRegs->USBSTS |= STS_USBERRINT;           }
    if (pOpRegs->USBSTS & STS_PORT_CHANGE)        { printf("\nPort Change Detect");            pOpRegs->USBSTS |= STS_PORT_CHANGE;         }
    if (pOpRegs->USBSTS & STS_FRAMELIST_ROLLOVER) { printf("\nFrame List Rollover");           pOpRegs->USBSTS |= STS_FRAMELIST_ROLLOVER;  }
    if (pOpRegs->USBSTS & STS_HOST_SYSTEM_ERROR)  { printf("\nHost System Error");             pOpRegs->USBSTS |= STS_HOST_SYSTEM_ERROR;   }
    if (pOpRegs->USBSTS & STS_ASYNC_INT)          { printf("\nInterrupt on Async Advance");    pOpRegs->USBSTS |= STS_ASYNC_INT;           }
    if (pOpRegs->USBSTS & STS_HCHALTED)           { printf("\nHCHalted");                      pOpRegs->USBSTS |= STS_HCHALTED;            }
    if (pOpRegs->USBSTS & STS_RECLAMATION)        { printf("\nReclamation");                   pOpRegs->USBSTS |= STS_RECLAMATION;         }
    if (pOpRegs->USBSTS & STS_PERIODIC_ENABLED)   { printf("\nPeriodic Schedule Status");      pOpRegs->USBSTS |= STS_PERIODIC_ENABLED;    }
    if (pOpRegs->USBSTS & STS_ASYNC_ENABLED)      { printf("\nAsynchronous Schedule Status");  pOpRegs->USBSTS |= STS_ASYNC_ENABLED;       }
    textColor(0x0F);
}

void checkPortLineStatus(uint8_t j)
{
    textColor(0x0E);
    if (j<numPorts)
    // if (j==PORTRESET) // ??
    {
      //check line status
      textColor(0x0B);
      printf("\nport %d: %X, line: %y  ",j+1,pOpRegs->PORTSC[j],(pOpRegs->PORTSC[j]>>10)&3);
      textColor(0x0E);
      if (((pOpRegs->PORTSC[j]>>10)&3) == 0) // SE0
      {
        textColor(0x0B);
        printf("SE0");
        if ((pOpRegs->PORTSC[j] & PSTS_POWERON) && (pOpRegs->PORTSC[j] & PSTS_ENABLED) && (pOpRegs->PORTSC[j] & ~PSTS_COMPANION_HC_OWNED))
        {
             textColor(0x0E);
             printf(",power on, enabled, EHCI owned");
             textColor(0x0F);

             if (USBtransferFlag && enabledPortFlag && (pOpRegs->PORTSC[j] & (PSTS_POWERON | PSTS_ENABLED | PSTS_CONNECTED)))
             {
                 uint8_t devAddr = usbTransferEnumerate(j);

                 #ifdef _USB_DIAGNOSIS_
                 printf("\nSETUP: "); showStatusbyteQTD(SetupQTD); waitForKeyStroke();
                 #endif

                 usbTransferDevice(devAddr); // device address, endpoint=0

                 #ifdef _USB_DIAGNOSIS_
                 printf("\nsetup packet: "); showPacket(SetupQTDpage0,8); printf("\nSETUP: "); showStatusbyteQTD(SetupQTD);
                 printf("\nIO:    "); showStatusbyteQTD(DataQTD); waitForKeyStroke();
                 #endif

                 usbTransferConfig(devAddr); // device address, endpoint 0

                 #ifdef _USB_DIAGNOSIS_
                 printf("\nsetup packet: "); showPacket(SetupQTDpage0,8); printf("\nSETUP: "); showStatusbyteQTD(SetupQTD);
                 printf("\nIO   : "); showStatusbyteQTD(DataQTD); waitForKeyStroke();
                 #endif

                 usbTransferString(devAddr); // device address, endpoint 0

                 #ifdef _USB_DIAGNOSIS_
                 printf("\nsetup packet: "); showPacket(SetupQTDpage0,8); printf("\nSETUP: "); showStatusbyteQTD(SetupQTD);
                 printf("\nIO   : "); showStatusbyteQTD(DataQTD);
                 #endif

                 for(int k=1; k<4;k++) // fetch 3 strings
                 {
                     #ifdef _USB_DIAGNOSIS_
                     waitForKeyStroke();
                     #endif

                     usbTransferStringUnicode(devAddr,k);

                     #ifdef _USB_DIAGNOSIS_
                     printf("\nsetup packet: "); showPacket(SetupQTDpage0,8); printf("\nSETUP: "); showStatusbyteQTD(SetupQTD);
                     printf("\nIO   : "); showStatusbyteQTD(DataQTD);
                     #endif
                 }

                 usbTransferSetConfiguration(devAddr, 1); // set first configuration
                 #ifdef _USB_DIAGNOSIS_
                 printf("\nSETUP: "); showStatusbyteQTD(SetupQTD);
                 #endif

                 printf(" %d",usbTransferGetConfiguration(devAddr)); // check configuration

                 #ifdef _USB_DIAGNOSIS_
                 printf("\nsetup packet: "); showPacket(SetupQTDpage0,8); printf("\nSETUP: "); showStatusbyteQTD(SetupQTD);
                 printf("\ndata packet: ");  showPacket(DataQTDpage0, 1); printf("\nIO:    "); showStatusbyteQTD(DataQTD);
                 waitForKeyStroke();
                 #endif

                 /// device, interface, endpoints, and maxLUN (0 for USB-sticks)
                 usbDevices[devAddr].maxLUN = 0;
                 // usbDevices[devAddr].maxLUN = usbTransferBulkOnlyGetMaxLUN(devAddr, usbDevices[devAddr].numInterfaceMSD);
                 
                 printf("\ndev: %d interface: %d endpOUT: %d  endpIN: %d",devAddr, usbDevices[devAddr].numInterfaceMSD, 
                                             usbDevices[devAddr].numEndpointOutMSD,usbDevices[devAddr].numEndpointInMSD);
                 // printf("\nMax. Logical Unit Numbers (LUN): %d",usbDevices[devAddr].maxLUN);

                 usbTransferBulkOnlyMassStorageReset(devAddr, usbDevices[devAddr].numInterfaceMSD); // Reset Interface

               ///////// Test Suite 1: send SCSI comamnd "test unit ready(6)"
                 
                 int32_t timeout = 5; 
                 uint8_t statusByte;
                 do
                 {        
                     textColor(0x09); printf("\n>>> SCSI: test unit ready"); textColor(0x0F);
                     usbSendSCSIcmd(devAddr, usbDevices[devAddr].numEndpointOutMSD, usbDevices[devAddr].numEndpointInMSD, 0x00, 0, 0, true); // dev, endp, cmd, LBA, transfer length, MSDStatus
                     statusByte = BYTE1(*(((uint32_t*)MSDStatusQTDpage0)+3));

                     timeout--;
                     if (timeout <= 0) 
                     {
                         break;
                     }
                     waitForKeyStroke();
                 }
                 while (statusByte != 0x00); // 0x00 means: Command Passed ("good status")
                                  
                 if (statusByte != 0x00)
                 {
                     textColor(0x0C);
                     printf("\n\nCommand Block Status Values not in \"good status\"\n");
                     textColor(0x0F);
                 }
                 else                 
                 {

                   ///////// Test Suite 2: send SCSI comamnd "read capacity(10)"
                     
                     //usbTransferBulkOnlyMassStorageReset(devAddr, usbDevices[devAddr].numInterfaceMSD); // Reset Interface

                 
                     textColor(0x09); printf("\n>>> SCSI: read capacity"); textColor(0x0F);
                     usbSendSCSIcmd(devAddr, usbDevices[devAddr].numEndpointOutMSD, usbDevices[devAddr].numEndpointInMSD, 0x25, 0, 8, true); // dev, endp, cmd, LBA, transfer length, MSDStatus
                     uint32_t lastLBA    = (*((uint8_t*)DataQTDpage0+0)) * 16777216 + (*((uint8_t*)DataQTDpage0+1)) * 65536 + (*((uint8_t*)DataQTDpage0+2)) * 256 + (*((uint8_t*)DataQTDpage0+3));
                     uint32_t blocksize  = (*((uint8_t*)DataQTDpage0+4)) * 16777216 + (*((uint8_t*)DataQTDpage0+5)) * 65536 + (*((uint8_t*)DataQTDpage0+6)) * 256 + (*((uint8_t*)DataQTDpage0+7));
                     uint32_t capacityMB = ((lastLBA+1)/1000000) * blocksize;

                     textColor(0x0E);
                     printf("\nCapacity: %d MB, Last LBA: %d, block size %d\n", capacityMB, lastLBA, blocksize);
                     textColor(0x0F);
                     waitForKeyStroke();
                  

                   ///////// Test Suite 3: send SCSI comamnd "read(10)", read 512 byte from LBA 0, and get Status

                     //usbTransferBulkOnlyMassStorageReset(devAddr, usbDevices[devAddr].numInterfaceMSD); // Reset Interface
                     
                     uint32_t length = 512; // number of byte to be read
                     
                     for(uint32_t sector=1055; sector < 1060; sector++)
                     {
                         textColor(0x09); printf("\n>>> SCSI: read(10)"); textColor(0x0F);
                         usbSendSCSIcmd(devAddr, usbDevices[devAddr].numEndpointOutMSD, usbDevices[devAddr].numEndpointInMSD, 
                                        0x28, sector, length, false); // dev, endp, cmd, LBA, transfer length, MSDStatus
                         waitForKeyStroke();                     
                     }

                   ///////// Test Suite 4: ...
                 }

             }
        }
      }

      if (((pOpRegs->PORTSC[j]>>10)&3) == 1) // K_STATE
      {
        textColor(0x0E);
        printf("K-State");
      }

      if (((pOpRegs->PORTSC[j]>>10)&3) == 2) // J_STATE
      {
        textColor(0x0E);
        printf("J-state");
      }

      if (((pOpRegs->PORTSC[j]>>10)&3) == 3) // undefined
      {
        textColor(0x0C);
        printf("undefined");
      }
    }
    textColor(0x0F);
}

void DeactivateLegacySupport(uint32_t num)
{
    bool failed = false;

    // pci bus data
    uint8_t bus  = pciDev_Array[num].bus;
    uint8_t dev  = pciDev_Array[num].device;
    uint8_t func = pciDev_Array[num].func;

    eecp = BYTE2(pCapRegs->HCCPARAMS);
    printf("\nDeactivateLegacySupport: eecp = %x\n",eecp);
    /*
    cf. EHCI 1.0 spec, 2.2.4 HCCPARAMS - Capability Parameters, Bit 15:8 (BYTE2)
    EHCI Extended Capabilities Pointer (EECP). Default = Implementation Dependent.
    This optional field indicates the existence of a capabilities list.
    A value of 00h indicates no extended capabilities are implemented.
    A non-zero value in this register indicates the offset in PCI configuration space
    of the first EHCI extended capability. The pointer value must be 40h or greater
    if implemented to maintain the consistency of the PCI header defined for this class of device.
    */
    // cf. http://wiki.osdev.org/PCI#PCI_Device_Structure

    //               eecp       // RO  - This field identifies the extended capability.
                                //       01h identifies the capability as Legacy Support.
    if (eecp >= 0x40)
    {
        int32_t eecp_id=0;

        while (eecp)
        {
            uint32_t NextEHCIExtCapPtr; // RO  - 00h indicates end of the ext. cap. list.

            printf("eecp = %x, ",eecp);
            eecp_id = pci_config_read(bus, dev, func, 0x0100/*length 1 byte*/ | (eecp + 0));
            printf("eecp_id = %x\n",eecp_id);
            if (eecp_id == 1)
                 break;
            NextEHCIExtCapPtr = eecp + 1;
            eecp = pci_config_read(bus, dev, func, 0x0100 | NextEHCIExtCapPtr);
        }
        uint32_t BIOSownedSemaphore = eecp + 2; // R/W - only Bit 16 (Bit 23:17 Reserved, must be set to zero)
        uint32_t OSownedSemaphore   = eecp + 3; // R/W - only Bit 24 (Bit 31:25 Reserved, must be set to zero)
        uint32_t USBLEGCTLSTS       = eecp + 4; // USB Legacy Support Control/Status (DWORD, cf. EHCI 1.0 spec, 2.1.8)

        // Legacy-Support-EC found? BIOS-Semaphore set?
        if ((eecp_id == 1) && (pci_config_read(bus, dev, func, 0x0100 | BIOSownedSemaphore) & 0x01))
        {
            printf("set OS-Semaphore.\n");
            pci_config_write_byte(bus, dev, func, OSownedSemaphore, 0x01);
            failed = true;

            int32_t timeout=200;
            // Wait for BIOS-Semaphore being not set
            while ((pci_config_read(bus, dev, func, 0x0100 | BIOSownedSemaphore) & 0x01) && (timeout>0))
            {
                printf(".");
                timeout--;
                sleepMilliSeconds(20);
            }
            if (!(pci_config_read(bus, dev, func, 0x0100 | BIOSownedSemaphore) & 0x01)) // not set
            {
                printf("BIOS-Semaphore being not set.\n");
                timeout=200;
                while (!(pci_config_read(bus, dev, func, 0x0100 | OSownedSemaphore) & 0x01) && (timeout>0))
                {
                    printf(".");
                    timeout--;
                    sleepMilliSeconds(20);
                }
            }
            if (pci_config_read(bus, dev, func, 0x0100 | OSownedSemaphore) & 0x01)
            {
                failed = false;
                printf("OS-Semaphore being set.\n");
            }

            printf("Check: BIOSownedSemaphore: %d OSownedSemaphore: %d\n",
                pci_config_read(bus, dev, func, 0x0100 | BIOSownedSemaphore),
                pci_config_read(bus, dev, func, 0x0100 | OSownedSemaphore));


            // USB SMI Enable R/W. 0=Default.
            // The OS tries to set SMI to disabled in case that BIOS bit satys at one.
            pci_config_write_dword(bus, dev, func, USBLEGCTLSTS, 0x0); // USB SMI disabled
        }
        else
        {
                textColor(0x0A);
                printf("\nBIOS did not own the EHCI. No action needed.\n");
                textColor(0x0F);
        }
    }
    else
    {
        printf("No valid eecp found.\n");
    }
}

/*
* Copyright (c) 2009-2010 The PrettyOS Project. All rights reserved.
*
* http://www.c-plusplus.de/forum/viewforum-var-f-is-62.html
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
