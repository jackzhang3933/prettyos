/*
*  license and disclaimer for the use of this source code as per statement below
*  Lizenz und Haftungsausschluss f�r die Verwendung dieses Sourcecodes siehe unten
*/

#include "devicemanager.h"
#include "console.h"
#include "util.h"
#include "fat12.h"

disk_t* disks[DISKARRAYSIZE];
port_t* ports[PORTARRAYSIZE];
partition_t* systemPartition;

extern port_t portFloppy1, portFloppy2;
extern disk_t floppy1, floppy2;

void deviceManager_install(/*partition_t* system*/)
{
    memset(disks, 0, DISKARRAYSIZE*sizeof(disks));
    memset(ports, 0, PORTARRAYSIZE*sizeof(ports));
    //systemPartition = system;
}

void attachPort(port_t* port)
{
    for(uint8_t i=0; i<PORTARRAYSIZE; i++)
    {
        if(ports[i] == NULL)
        {
            ports[i] = port;
            return;
        }
    }
}

void attachDisk(disk_t* disk)
{
    // Later: Searching correct ID in device-File
    for(uint8_t i=0; i<DISKARRAYSIZE; i++)
    {
        if(disks[i] == NULL)
        {
            disks[i] = disk;
            return;
        }
    }
}

void removeDisk(disk_t* disk)
{
    for(uint8_t i=0; i<DISKARRAYSIZE; i++)
    {
        if(disks[i] == disk)
        {
            disks[i] = NULL;
            return;
        }
    }
}

void showPortList()
{
    printf("\n\nAvailable Ports:");
    textColor(0x07);
    printf("\n\nType    \tnumber\tMedia");
    printf("\n----------------------------------------------------------------------");
    textColor(0x0F);

    for (uint8_t i = 0; i < PORTARRAYSIZE; i++)
    {
        if (ports[i] != NULL)
        {
            switch (ports[i]->type) // Type
            {
                case FDD:
                    printf("\nFDD     \t%c", i+'A');
                    char volumeName[12];
                    flpydsk_get_volumeName(volumeName);

                    // if (ports[i]->insertedDisk != NULL)
                    if (volumeName[0]!=0x20)
                    {
                        // TODO: attach floppy disk to FDD ///////////////////
                        if (ports[i] == &portFloppy1) 
                        {
                            ports[i]->insertedDisk = &floppy1; // ???
                        }
                        if (ports[i] == &portFloppy2) 
                        {
                            ports[i]->insertedDisk = &floppy2; // ???
                        }
                        //////////////////////////////////////////////////////

                        printf("\t%s", volumeName);
                    }
                    else
                    {
                        printf("\tNo floppy disk inserted");
                    }
                    break;
                case RAM:
                    //printf("\nRAMdisk \t---");
                    //if (ports[i]->insertedDisk != NULL)
                    //{
                    //    printf("\tactive"); // only possibility
                    //}
                    break;
                case USB:
                    printf("\nUSB Port\t%c", i+'A');
                    if (ports[i]->insertedDisk != NULL)
                    {
                        printf("\tMSD attached");
                    }
                    else
                    {
                       printf("\tNo MSD attached");
                    }
                    break;
            }
        }
    }
    textColor(0x07);
    printf("\n----------------------------------------------------------------------\n");
    textColor(0x0F);
}

void showDiskList()
{
    printf("\n\nAttached Disks:");
    textColor(0x07);
    printf("\n\nType\tNumber\tSerial\tPart.\tSerial");
    printf("\n----------------------------------------------------------------------");
    textColor(0x0F);

    for (uint8_t i=0; i<DISKARRAYSIZE; i++)
    {
        if (disks[i] != NULL)
        {
            switch (disks[i]->type) // Type
            {
                case FLOPPYDISK:
                    printf("\nFloppy");
                    break;
                case RAMDISK:
                    printf("\nRAMdisk");
                    break;
                case USB_MSD:
                    printf("\nUSB MSD");
                    break;
            }

            textColor(0x0E); 
            printf("\t%u", i+1); // Number
            textColor(0x0F);

            printf("\t%u", disks[i]->serial); // Serial of disk

            for (uint8_t j = 0; j < PARTITIONARRAYSIZE; j++)
            {
                if (disks[i]->partition[j] == NULL) continue; // Empty

                if (j!=0) printf("\n\t\t\t"); // Not first, indent

                printf("\t%u", j); // Partition number

                switch(disks[i]->type)
                {
                    case FLOPPYDISK: // TODO: floppy disk device: use the current serials of the floppy disks
                        printf("\t%s", disks[i]->partition[j]->serialNumber);
                        break;
                    case RAMDISK:
                        printf("\t%s", disks[i]->partition[j]->serialNumber);
                        break;
                    case USB_MSD:                      
                        printf("\t%s", ((usb2_Device_t*)disks[i]->data)->serialNumber); // serial of device
                        break;
                }
            }
        }
    }
    textColor(0x07);
    printf("\n----------------------------------------------------------------------\n");
    textColor(0x0F);
}

partition_t* getPartition(const char* path)
{
    size_t length = strlen(path);
    char Buffer[10];

    int16_t PortID = -1;
    int16_t DiskID = -1;
    uint8_t PartitionID = 0;
    for(size_t i = 0; i < length; i++)
    {
        if(path[i] == ':' || path[i] == '-')
        {
            strncpy(Buffer, path, i);
            Buffer[i] = 0;
            if(isalpha(Buffer[0]))
            {
                PortID = toUpper(Buffer[0]) - 'A';
            }
            else
            {
                DiskID = atoi(Buffer);
            }
            for(size_t j = i+1; j < length; j++)
            {
                if(path[j] == ':' || path[j] == '-')
                {
                    strncpy(Buffer, path+i+1, j-i-1);
                    Buffer[j-i-1] = 0;
                    PartitionID = atoi(Buffer);
                    break;
                }
                if(!isdigit(path[j]) || path[j] == '/' || path[j] == '|' || path[j] == '\\')
                {
                    break;
                }
            }
            break;
        }
        if(!isalnum(path[i]))
        {
            return(0);
        }
    }
    if(PortID != -1)
    {
        return(ports[PortID]->insertedDisk->partition[PartitionID]);
    }
    else
    {
        if(DiskID == 0)
        {
            return(systemPartition);
        }
        else
        {
            return(disks[DiskID]->partition[PartitionID-1]);
        }
    }
}

/*
* Copyright (c) 2010 The PrettyOS Project. All rights reserved.
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