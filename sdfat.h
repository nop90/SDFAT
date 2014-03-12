#include <stdint.h>

#ifndef SDFAT_H
#define SDFAT_H

#define SECTSIZE 512
#define ATT_READ_ONLY 1
#define ATT_HIDDEN 2
#define ATT_SYSTEM 4
#define ATT_VOLUME_ID 8
#define ATT_DIRECTORY 16

#define FAT_ERR_SDREAD -1
#define FAT_ERR_MBR -2
#define FAT_ERR_PARTITION -3
#define FAT_ERR_FATID -4


typedef struct{
  uint32_t FAT_RecordLen;		// FAT Record lenght: 32 for FAT32, 16 for FAT32
  uint32_t LBA_Begin;		// Partition LBA Begin from MBR
  uint16_t BPB_BytsPerSec;	// Bytes Per Sector
  uint16_t BPB_RootDirEnt;	// Number of Root Directory entries
  uint8_t  BPB_SecPerClus;	// Sectors Per Cluster
  uint16_t BPB_RsvdSecCnt; 	// Number of Reserved Sectors
  uint8_t  BPB_NumFATs;		// Number of FATs
  uint32_t BPB_FATS;		// Sectors Per FAT
  uint32_t BPB_RootClus;		// Root Directory First Cluster
} FATVolume;

typedef struct{
//  char  DIR_LongName[256];
  char   DIR_Name[11];
  uint8_t  DIR_Attr;
  uint32_t DIR_FstClus;
  uint32_t DIR_FileSize;
} FATDirEntry;

static uint32_t lba_addr(uint32_t cluster_number);
static uint32_t cluster_begin_lba();
static uint32_t fat_begin_lba();
int mount();
int movefirstdirentry();
int movenextdirentry();
int changedir(char* buf);
int openf(char* buf);
int readf();
int writef();
uint8_t getcharf(uint32_t pos);
void setcharf(uint32_t pos, uint8_t val); 
uint8_t endoffile();
uint32_t posoffile();
static int movedirnextsect();
static uint32_t getnextsect(uint32_t sect);
static int cmpname(char* name1, char* name2);
void getdirentryname(char* buf);
uint8_t getdirentryattr();
uint32_t getfilesize();
uint32_t getsect();
void entryname2str(char* buf,char* entryname);

#endif
