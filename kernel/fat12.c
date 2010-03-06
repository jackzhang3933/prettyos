#include "flpydsk.h"
#include "fat12.h"
#include "task.h" // for log_task_list()

/*
Links:
http://www.win.tue.nl/~aeb/linux/fs/fat/fat-1.html
*/


#define MAX_ATTEMPTS_FLOPPY_DMA_BUFFER 60
#define SECTOR 0
#define TRACK  1

// cache memory for tracks 0 and 1
uint8_t cache0[9216];
uint8_t cache1[9216];

// long term necessary?
uint8_t track0[9216];
uint8_t track1[9216];

// how to handle memory for the file?
uint8_t file[51200];
int32_t fat_entry[FATMAXINDEX];

int32_t initCache()
{
    int32_t retVal0, retVal1;

    retVal0 = flpydsk_read_ia(0,cache0,TRACK);
    retVal1 = flpydsk_read_ia(1,cache1,TRACK);

    if((!retVal0) && (!retVal1))
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int32_t flpydsk_load(const char* name, const char* ext)
{
    int32_t retVal;
    struct file f;
    uint32_t firstCluster = 0;

    flpydsk_control_motor(true);
    retVal = initCache();
    if(retVal)
    {
        settextcolor(12,0);
        printformat("track0 & track1 read error.\n");
        settextcolor(2,0);
    }

    printformat("Load and execute "); settextcolor(14,0); printformat("-->%s.%s<--",name,ext);
    settextcolor(2,0); printformat(" from floppy disk\n");

    firstCluster = search_file_first_cluster(name,ext,&f); // now working with cache
    if(firstCluster==0)
    {
        printformat("file not found in root directory\n");
        return -1;
    }
    printformat("FileSize: %d Byte, 1st Cluster: %d\n",f.size, f.firstCluster);

    printformat("\nFAT1 parsed 12-bit-wise: ab cd ef --> dab efc\n");

    /*retVal = flpydsk_read_ia(0,track0, TRACK);*/

    memcpy((void*)track0, (void*)cache0, 0x2400); // cache0 --> track0 (necessary?)

    ///TODO: read only entries which are necessary for file_ia
    ///      perhaps reading FAT entry and data sector it can be combined

    for(uint32_t i=0;i<FATMAXINDEX;i++)
    {
        read_fat(&fat_entry[i], i, FAT1_SEC, track0);
    }
    retVal = file_ia(fat_entry,firstCluster,file); // read sectors of file
    ///

    #ifdef _DIAGNOSIS_
        printformat("\nFile content (start of first 5 clusters): ");
        printformat("\n1st sector:\n"); for(uint16_t i=   0;i<  20;i++) {printformat("%y ",file[i]);}
        printformat("\n2nd sector:\n"); for(uint16_t i= 512;i< 532;i++) {printformat("%y ",file[i]);}
        printformat("\n3rd sector:\n"); for(uint16_t i=1024;i<1044;i++) {printformat("%y ",file[i]);}
        printformat("\n4th sector:\n"); for(uint16_t i=1536;i<1556;i++) {printformat("%y ",file[i]);}
        printformat("\n5th sector:\n"); for(uint16_t i=2048;i<2068;i++) {printformat("%y ",file[i]);}
        printformat("\n\n");
    #endif

    if(!retVal)
    {
        /// START TASK AND INCREASE TASKCOUNTER
        if( elf_exec( file, f.size ) ) // execute loaded file
        {
            userTaskCounter++;         // an additional user-program has been started
        }
    }
    else if(retVal==-1)
    {
        printformat("file was not executed due to FAT error.");
    }
    printformat("\n\n");
    flpydsk_control_motor(false);
    return 0;
}


int32_t flpydsk_write_ia( int32_t i, void* a, int8_t option)
{
    int32_t val=0;

    if(option == SECTOR)
    {
        memcpy((void*)DMA_BUFFER, a  , 0x200);
        val = i;
    }
    else if(option == TRACK)
    {
        memcpy((void*)DMA_BUFFER, a, 0x2400);
        val = i*18;
    }

    uint32_t timeout = 2; // limit
    int32_t  retVal  = 0;

    while( flpydsk_write_sector_wo_motor(val) != 0 ) // without motor on/off
    {
        retVal = -1;
        timeout--;
        printformat("write error: attempts left: %d\n",timeout);
	    if(timeout<=0)
	    {
	        printformat("timeout\n");
	        break;
	    }
    }
    if(retVal==0)
    {
        // printformat("success write_sector.\n");
    }
    return retVal;
}


int32_t flpydsk_read_ia( int32_t i, void* a, int8_t option)
{
    /// TEST: change DMA before write/read
    /// printformat("DMA manipulation\n");

    int32_t val=0;

    if(option == SECTOR)
    {
        memset((void*)DMA_BUFFER, 0x41, 0x200); // 0x41 is in ASCII the 'A'
        val = i;
    }
    else if(option == TRACK)
    {
        memset((void*)DMA_BUFFER, 0x41, 0x2400); // 0x41 is in ASCII the 'A'
        val = i*18;
    }

    //flpydsk_initialize_dma(); // important, if you do not use the unreliable autoinit bit of DMA
    flpydsk_control_motor(true);

    int32_t retVal;
    for(uint8_t n=0;n<MAX_ATTEMPTS_FLOPPY_DMA_BUFFER;n++)
    {
        retVal = flpydsk_read_sector(val,0);
        if(retVal!=0)
        {
            printformat("\nread error: %d\n",retVal);
        }
        if( (*(uint8_t*)(DMA_BUFFER+ 0)==0x41) && (*(uint8_t*)(DMA_BUFFER+ 1)==0x41) &&
            (*(uint8_t*)(DMA_BUFFER+ 2)==0x41) && (*(uint8_t*)(DMA_BUFFER+ 3)==0x41) &&
            (*(uint8_t*)(DMA_BUFFER+ 4)==0x41) && (*(uint8_t*)(DMA_BUFFER+ 5)==0x41) &&
            (*(uint8_t*)(DMA_BUFFER+ 6)==0x41) && (*(uint8_t*)(DMA_BUFFER+ 7)==0x41) &&
            (*(uint8_t*)(DMA_BUFFER+ 8)==0x41) && (*(uint8_t*)(DMA_BUFFER+ 9)==0x41) &&
            (*(uint8_t*)(DMA_BUFFER+10)==0x41) && (*(uint8_t*)(DMA_BUFFER+11)==0x41)
          )
          {memset((void*)DMA_BUFFER, 0x41, 0x2400); // 0x41 is in ASCII the 'A'
              settextcolor(4,0);
              printformat("Floppy ---> DMA attempt no. %d failed.\n",n+1);
              if(n>=MAX_ATTEMPTS_FLOPPY_DMA_BUFFER-1)
              {
                  printformat("Floppy ---> DMA error.\n");
              }
              settextcolor(2,0);
              continue;
          }
          else
          {
              settextcolor(3,0);
              printformat("Floppy ---> DMA success.\n");
              settextcolor(2,0);
              break;
          }
    }

    if(option == SECTOR)
    {
        memcpy( (void*)a, (void*)DMA_BUFFER, 0x200);
    }
    else if(option == TRACK)
    {
        memcpy( (void*)a, (void*)DMA_BUFFER, 0x2400);
    }
    return retVal;
}


int32_t file_ia(int32_t* fatEntry, uint32_t firstCluster, void* fileData)
{
    uint8_t a[512];
    uint32_t sectornumber;
    uint32_t i, pos;  // i for FAT-index, pos for data position
    const uint32_t ADD = 31;

    // copy first cluster
    sectornumber = firstCluster+ADD;
    printformat("\n\n1st sector: %d\n",sectornumber);

    uint32_t timeout = 2; // limit
    int32_t  retVal  = 0;
    while( flpydsk_read_ia(sectornumber,a,SECTOR) != 0 )
    {
        retVal = -1;
        timeout--;
        printformat("error read_sector. attempts left: %d\n",timeout);
	    if(timeout<=0)
	    {
	        printformat("timeout\n");
	        break;
	    }
    }
    if(retVal==0)
    {
        /// printformat("success read_sector.\n");
    }

    memcpy( (void*)fileData, (void*)a, 512);

    // // find second cluster and chain in fat
    pos=0;
    i = firstCluster;
    while(fatEntry[i]!=0xFFF)
    {
        printformat("\ni: %d FAT-entry: %x\t",i,fatEntry[i]);
        if( (fatEntry[i]<3) || (fatEntry[i]>MAX_BLOCK))
        {
            printformat("FAT-error.\n");
            return -1;
        }

        // copy data from chain
        pos++;
        sectornumber = fatEntry[i]+ADD;
        printformat("sector: %d\t",sectornumber);

        timeout = 2; // limit
        retVal  = 0;
        while( flpydsk_read_ia(sectornumber,a,SECTOR) != 0 )
        {
            retVal = -1;
            timeout--;
            printformat("error read_sector. attempts left: %d\n",timeout);
	        if(timeout<=0)
	        {
	            printformat("timeout\n");
	            break;
	        }
        }
        if(retVal==0)
        {
            /// printformat("success read_sector.\n");
        }

        memcpy( (void*)(fileData+pos*512), (void*)a, 512);

        // search next cluster of the fileData
        i = fatEntry[i];
    }
    printformat("\n");
    return 0;
}



int32_t read_fat(int32_t* fat_entrypoint, int32_t index, int32_t st_sec, uint8_t* buffer)
{
    // example: //TODO: only necessary FAT entries and combine these tow steps:
                //parse FAT & load file data
    // for(i=0;i<FATMAXINDEX;i++)
    //    read_fat(&fat_entrypoint[i], i, FAT1_SEC);
    // file_ia(fat_entrypoint,firstCluster,file);


    int32_t fat_index;
    int32_t fat1, fat2;

    fat_index = (index*3)/2; // 0 -> 0, 1 -> 1
                       // 100 -> 150, 101 -> 151, 102 -> 153, 103 -> 154, 104 -> 156, ...
                       // 511 -> 766, 512 -> 768

    fat1 = buffer[st_sec*512+fat_index]   & 0xFF;
    fat2 = buffer[st_sec*512+fat_index+1] & 0xFF;

    parse_fat(fat_entrypoint,fat1,fat2,index);

    return 0;
}

int32_t flpydsk_read_directory() /// TODO: check whether Floppy ---> DMA really works !
{
    int32_t error = -1; // return value


	memset((void*)DMA_BUFFER, 0x0, 0x2400); // 18 sectors: 18 * 512 = 9216 = 0x2400

    //flpydsk_initialize_dma(); // important, if you do not use the unreliable autoinit bit of DMA

	/// TODO: change to read_ia(...)!
	int32_t retVal = flpydsk_read_sector(19,1); // start at 0x2600: root directory (14 sectors)
	if(retVal != 0)
	{
	    printformat("\nread error: %d\n",retVal);
	}
	printformat("<Floppy Disc Directory>\n");

	for(uint8_t i=0;i<224;++i)       // 224 Entries * 32 Byte
	{
        if(
			(( *((uint8_t*)(DMA_BUFFER + i*32)) )      != 0x00 ) && /* free from here on           */
			(( *((uint8_t*)(DMA_BUFFER + i*32)) )      != 0xE5 ) && /* 0xE5 deleted = free         */
			(( *((uint8_t*)(DMA_BUFFER + i*32 + 11)) ) != 0x0F )    /* 0x0F part of long file name */
		  )
		  {
		    error = 0;
			int32_t start = DMA_BUFFER + i*32; // name
			int32_t count = 8;
			int8_t* end = (int8_t*)(start+count);
			for(; count != 0; --count)
			{
			    if( *(end-count) != 0x20 ) /* empty space in file name */
				    printformat("%c",*(end-count));
			}

            start = DMA_BUFFER + i*32 + 8; // extension

			if(((( *((uint8_t*)(DMA_BUFFER + i*32 + 11)) ) & 0x08 ) == 0x08) ||  // volume label
			     ( ( *((uint8_t*) (start))   == 0x20) &&
			       ( *((uint8_t*) (start+1)) == 0x20) &&
			       ( *((uint8_t*) (start+2)) == 0x20) ))                          // extension == three 'space'
			{
			    // do nothing
			}
			else
			{
			    printformat("."); // usual separator between file name and file extension
			}

			count = 3;
			end = (int8_t*)(start+count);
			for(; count!=0; --count)
				printformat("%c",*(end-count));

			// filesize
			printformat("\t%d byte", *((uint32_t*)(DMA_BUFFER + i*32 + 28)));

            // attributes
			printformat("\t");
			if(*((uint32_t*)(DMA_BUFFER + i*32 + 28))<100)                   printformat("\t");
			if((( *((uint8_t*)(DMA_BUFFER + i*32 + 11)) ) & 0x08 ) == 0x08 ) printformat(" (vol)");
			if((( *((uint8_t*)(DMA_BUFFER + i*32 + 11)) ) & 0x10 ) == 0x10 ) printformat(" (dir)");
			if((( *((uint8_t*)(DMA_BUFFER + i*32 + 11)) ) & 0x01 ) == 0x01 ) printformat(" (r/o)");
			if((( *((uint8_t*)(DMA_BUFFER + i*32 + 11)) ) & 0x02 ) == 0x02 ) printformat(" (hid)");
			if((( *((uint8_t*)(DMA_BUFFER + i*32 + 11)) ) & 0x04 ) == 0x04 ) printformat(" (sys)");
			if((( *((uint8_t*)(DMA_BUFFER + i*32 + 11)) ) & 0x20 ) == 0x20 ) printformat(" (arc)");

			// 1st cluster: physical sector number  =  33  +  FAT entry number  -  2  =  FAT entry number  +  31
            printformat("  1st sector: %d", *((uint16_t*)(DMA_BUFFER + i*32 + 26))+31);
            printformat("\n"); // next root directory entry
		  }//if
	}//for
    printformat("\n");
    return error;
}


/*****************************************************************************
  The following functions are derived from source code of the dynacube team.
  which was published in the year 2004 at http://www.dynacube.net

  This functions are free software and can be redistributed/modified
  under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License,
  or any later version.
*****************************************************************************/

int32_t flpydsk_prepare_boot_sector(struct boot_sector *bs)
{
    int32_t i;
    uint8_t a[512];

    int32_t retVal = flpydsk_read_ia(BOOT_SEC,a,SECTOR);
    if(retVal!=0)
    {
        printformat("\nread error: %d\n",retVal);
    }

    i=0;
    for(uint8_t j=0;j<3;j++)
    {
        a[j]=bs->jumpBoot[j];
    }

    i+= 3;
    for(uint8_t j=0;j<8;j++,i++)
    {
        a[i]= bs->SysName[j];
    }

    a[i]   = BYTE1(bs->charsPerSector);
    a[i+1] = BYTE2(bs->charsPerSector);
    i+=2;

    a[i++] = bs->SectorsPerCluster;

    a[i]   = BYTE1(bs->ReservedSectors);
    a[i+1] = BYTE2(bs->ReservedSectors);
    i+=2;

    a[i++] = bs->FATcount;

    a[i]   = BYTE1(bs->MaxRootEntries);
    a[i+1] = BYTE2(bs->MaxRootEntries);
    i+=2;

    a[i]   = BYTE1(bs->TotalSectors1);
    a[i+1] = BYTE2(bs->TotalSectors1);
    i+=2;

    a[i++] = bs->MediaDescriptor;

    a[i]   = BYTE1(bs->SectorsPerFAT);
    a[i+1] = BYTE2(bs->SectorsPerFAT);
    i+=2;

    a[i]   = BYTE1(bs->SectorsPerTrack);
    a[i+1] = BYTE2(bs->SectorsPerTrack);
    i+=2;

    a[i]   = BYTE1(bs->HeadCount);
    a[i+1] = BYTE2(bs->HeadCount);
    i+=2;

    a[i]   = BYTE1(bs->HiddenSectors);
    a[i+1] = BYTE2(bs->HiddenSectors);
    a[i+2] = BYTE3(bs->HiddenSectors);
    a[i+3] = BYTE4(bs->HiddenSectors);
    i+=4;

    a[i]   = BYTE1(bs->TotalSectors2);
    a[i+1] = BYTE2(bs->TotalSectors2);
    a[i+2] = BYTE3(bs->TotalSectors2);
    a[i+3] = BYTE4(bs->TotalSectors2);
    i+=4;

    a[i++] = bs->DriveNumber;
    a[i++] = bs->Reserved1;
    a[i++] = bs->ExtBootSignature;

    a[i]   = BYTE1(bs->VolumeSerial);
    a[i+1] = BYTE2(bs->VolumeSerial);
    a[i+2] = BYTE3(bs->VolumeSerial);
    a[i+3] = BYTE4(bs->VolumeSerial);
    i+=4;

    for(uint8_t j=0;j<11;j++,i++)
    {
        a[i] = bs->VolumeLabel[j];
    }

    for(uint8_t j=0;j<8;j++,i++)
    {
        a[i] = bs->Reserved2[j];
    }

    // boot signature
    a[510]= 0x55; a[511]= 0xAA;


    // flpydsk_control_motor(true); printformat("write_boot_sector.motor_on\n");
    // return flpydsk_write_sector_ia( BOOT_SEC, a );
    /// prepare sector 0 of track 0
    for(uint16_t k=0;k<511;k++)
    {
        track0[k] = a[k];
    }
    return 0;
}



int32_t flpydsk_format(char* vlab) // VolumeLabel
{
    struct boot_sector b;
    uint8_t a[512];
    uint8_t i;

    // int32_t dt, tm; // for VolumeSerial

    flpydsk_control_motor(true);
    printformat("\n\nFormat process started.\n");

    for(i=0;i<11;i++)
    {
        if(vlab[i] == '\0')
        {
            break;
        }
    }

    for(uint8_t j=i;j<11;j++)
    {
        vlab[j] = ' ';
    }

    b.jumpBoot[0] = 0xeb;
    b.jumpBoot[1] = 0x3c;
    b.jumpBoot[2] = 0x90;

    b.SysName[0] = 'M';
    b.SysName[1] = 'S';
    b.SysName[2] = 'W';
    b.SysName[3] = 'I';
    b.SysName[4] = 'N';
    b.SysName[5] = '4';
    b.SysName[6] = '.';
    b.SysName[7] = '1';

    b.charsPerSector    =  512;
    b.SectorsPerCluster =    1;
    b.ReservedSectors   =    1;
    b.FATcount          =    2;
    b.MaxRootEntries    =  224;
    b.TotalSectors1     = 2880;
    b.MediaDescriptor   = 0xF0;
    b.SectorsPerFAT     =    9;
    b.SectorsPerTrack   =   18;
    b.HeadCount         =    2;
    b.HiddenSectors     =    0;
    b.TotalSectors2     =    0;
    b.DriveNumber       =    0;
    b.Reserved1         =    0;
    b.ExtBootSignature  = 0x29;

      /*
      dt = form_date();
      tm = form_time();
      dt = ((dt & 0xff) << 8) | ((dt & 0xFF00) >> 8);
      tm = ((tm & 0xff) << 8) | ((tm & 0xFF00) >> 8);
      */
    b.VolumeSerial = 0x12345678;
    /* b.VolumeSerial = tm << 16 + dt; */

    for(uint8_t j=0;j<11;j++)
    {
        b.VolumeLabel[j] = vlab[j];
    }
    b.Reserved2[0] = 'F';
    b.Reserved2[1] = 'A';
    b.Reserved2[2] = 'T';
    b.Reserved2[3] = '1';
    b.Reserved2[4] = '2';
    b.Reserved2[5] = ' ';
    b.Reserved2[6] = ' ';
    b.Reserved2[7] = ' ';


    /// bootsector
    flpydsk_prepare_boot_sector(&b);

    /// prepare FATs
    for(uint16_t j=512;j<9216;j++)
    {
        track0[j] = 0;
    }
    for(uint16_t j=0;j<9216;j++)
    {
        track1[j] = 0;
    }

    a[0]=0xF0; a[1]=0xFF; a[2]=0xFF;
    for(uint8_t j=0;j<3;j++)
    {
        track0[FAT1_SEC*512+j]=a[j]; // FAT1 starts at 0x200  (sector  1)
        track0[FAT2_SEC*512+j]=a[j]; // FAT2 starts at 0x1400 (sector 10)
    }

    /// prepare first root directory entry (volume label)
    for(uint8_t j=0;j<11;j++)
    {
        track1[512+j] = vlab[j];
    }
    track1[512+11] = ATTR_VOLUME_ID | ATTR_ARCHIVE;

    for(uint16_t j=7680;j<9216;j++)
    {
        track1[j] = 0xF6; // format ID of MS Windows
    }

    /// write track 0 & track 1
    flpydsk_control_motor(true); printformat("writing tracks 1 & 2\n");
    flpydsk_write_ia(0,track0,TRACK);
    flpydsk_write_ia(1,track1,TRACK);
    printformat("Quickformat complete.\n\n");

    ///TEST
    printformat("Content of Disc:\n");
    struct dir_entry entry;
    for(uint8_t j=0;j<224;j++)
    {
        read_dir(&entry, j, 19, false);
        if(strcmp((&entry)->Filename,"")==0)
        {
            break;
        }
    }
    printformat("\n");
    ///TEST

    return 0;
}


void parse_dir(uint8_t* a, int32_t in, struct dir_entry* rs)
{
   int32_t i = (in %DIR_ENTRIES) * 32;

   for(int32_t j=0;j<8;j++,i++)
   {
       rs->Filename[j] = a[i];
   }

   for(int32_t j=0;j<3;j++,i++)
   {
       rs->Extension[j] = a[i];
   }

   rs->Attributes   = a[i++];
   rs->NTRes        = a[i++];
   rs->CrtTimeTenth = a[i++];
   rs->CrtTime      = FORM_SHORT(a[i],a[i+1]);                  i+=2;
   rs->CrtDate      = FORM_SHORT(a[i],a[i+1]);                  i+=2;
   rs->LstAccDate   = FORM_SHORT(a[i],a[i+1]);                  i+=2;
   rs->FstClusHI    = FORM_SHORT(a[i],a[i+1]);                  i+=2;
   rs->WrtTime      = FORM_SHORT(a[i],a[i+1]);                  i+=2;
   rs->WrtDate      = FORM_SHORT(a[i],a[i+1]);                  i+=2;
   rs->FstClusLO    = FORM_SHORT(a[i],a[i+1]);                  i+=2;
   rs->FileSize     = FORM_LONG(a[i],a[i+1],a[i+2],a[i+3]);     i+=4;
}

void print_dir(struct dir_entry* rs)
{
    if(strcmp(rs->Filename,"")!=0)
    {
        printformat("File Name : ");
        for(int32_t j=0;j<8;j++)
        {
            printformat("%c",rs->Filename[j]);
        }
        printformat("\n");
        printformat("Extension : ");
        for(int32_t j=0;j<3;j++)
        {
            printformat("%c",rs->Extension[j]);
        }
        printformat("\n");
        printformat("Attributes   = %d\t %x\n",rs->Attributes,   rs->Attributes      );
        printformat("NTRes        = %d\t %x\n",rs->NTRes,        rs->NTRes           );
        printformat("CrtTimeTenth = %d\t %x\n",rs->CrtTimeTenth, rs->CrtTimeTenth    );
        printformat("CrtTime      = %d\t %x\n",rs->CrtTime,      rs->CrtTime         );
        printformat("CrtDate      = %d\t %x\n",rs->CrtDate,      rs->CrtDate         );
        printformat("LstAccDate   = %d\t %x\n",rs->LstAccDate,   rs->LstAccDate      );
        printformat("FstClusHI    = %d\t %x\n",rs->FstClusHI,    rs->FstClusHI       );
        printformat("WrtTime      = %d\t %x\n",rs->WrtTime,      rs->WrtTime         );
        printformat("WrtDate      = %d\t %x\n",rs->WrtDate,      rs->WrtDate         );
        printformat("FstClusLO    = %d\t %x\n",rs->FstClusLO,    rs->FstClusLO       );
        printformat("FileSize     = %d\t %x\n",rs->FileSize,     rs->FileSize        );
        printformat("\n");
    }
}

int32_t read_dir(struct dir_entry* rs, int32_t in, int32_t st_sec, bool flag)
{
   uint8_t a[512];
   st_sec = st_sec + in/DIR_ENTRIES;

   /*
   if(flpydsk_read_ia(st_sec,a,SECTOR) != 0) // <--- bullshit
   {
       return E_DISK;
   }
   */
   memcpy((void*)a,(void*)(cache1+st_sec*512-9216),0x200); //copy data from cache to a[...]

   parse_dir(a,in,rs);
   if(flag==true)
   {
       print_dir(rs);
   }
   return 0;
}

uint32_t search_file_first_cluster(const char* name, const char* ext, struct file* f)
{
   struct dir_entry entry;
   char buf1[10], buf2[5];

   for(uint8_t i=0;i<224;i++)
   {
       read_dir(&entry, i, 19, false);
       if ((&entry)->Filename[0] == 0)
       {
           break; // filter empty entry, no further entries expected
       }
       settextcolor(14,0);
       printformat("root dir entry: %c%c%c%c%c%c%c%c.%c%c%c\n",
                   (&entry)->Filename[0],(&entry)->Filename[1],(&entry)->Filename[2],(&entry)->Filename[3],
                   (&entry)->Filename[4],(&entry)->Filename[5],(&entry)->Filename[6],(&entry)->Filename[7],
                   (&entry)->Extension[0],(&entry)->Extension[1],(&entry)->Extension[2]);
       settextcolor(2,0);

       for(uint8_t j=0;j<3;j++)
       {
           buf1[j] = (&entry)->Filename[j];
           buf2[j] = (&entry)->Extension[j];
       }
       for(uint8_t j=3;j<8;j++)
       {
           buf1[j] = (&entry)->Filename[j];
       }
       buf1[8]=0; //string termination Filename
       buf2[3]=0; //string termination Extension

       if((strcmp(buf1,name)==0) && (strcmp(buf2,ext)==0))
       {
           break;
       }
    }
    settextcolor(14,0);
    printformat("rootdir search finished.\n\n");
    settextcolor(2,0);

    f->size = (&entry)->FileSize;
    f->firstCluster = FORM_SHORT((&entry)->FstClusLO,(&entry)->FstClusHI);

    return f->firstCluster;
}


// combine two FAT-entries fat1 and fat2 to a 12-bit-value fat_entry
void parse_fat(int32_t* fat_entrypoint, int32_t fat1, int32_t fat2, int32_t in)
{
    int32_t fat;
    if(in%2 == 0)
    {
        fat = ((fat2 & 0x0F) << 8) | fat1;
    }
    else
    {
        fat = (fat2 << 4) | ((fat1 &0x0F0) >> 4);
    }
    fat = fat & 0xFFF;
    *fat_entrypoint = fat;
    ///printformat("%x ", fat);
}

