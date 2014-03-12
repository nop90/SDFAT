/*
   Minimalist SD FAT filesystem IO by NOP90
   Uses 3DS SDIO Handling code from Kane49 adaption of the sdmmc module made for libnds    

   based on information from 
   - http://www.pjrc.com/tech/8051/ide/fat32.html

   limitations:
   - only 8.3 file name support
   - only 1 file opened at a time
   - no file or directory creation
   - no file resizing, can write only on already allocated sectors
*/   

#include <stdint.h>
#include "sdfat.h"
#include "sdmmc.h"
#include "minlib.h"

static uint8_t sect[SECTSIZE],filesect[SECTSIZE], fatsect[SECTSIZE];
static FATVolume fat;
static uint32_t rootdirsect, curdirsect, curfilesect, curfatsect;
static FATDirEntry curdirentry, curdir, curfile;
static uint8_t direntrycount,curdirsectorcluster;
static uint32_t curfilesectorcluster,filesectorcount;

static uint32_t lba_addr(uint32_t cluster_number){return cluster_begin_lba() + 32 * (fat.BPB_RootDirEnt / fat.BPB_BytsPerSec) + (cluster_number - 2) * fat.BPB_SecPerClus;}
static uint32_t cluster_begin_lba() {return fat.LBA_Begin + fat.BPB_RsvdSecCnt + (fat.BPB_NumFATs * fat.BPB_FATS);}
static uint32_t fat_begin_lba() {return fat.LBA_Begin + fat.BPB_RsvdSecCnt;}

int mount(){
  uint8_t i;
  uint32_t temp;
  int8_t partition;

  sdmmc_controller_init();
  temp = sdmmc_sdcard_init() ;
  if (sdmmc_sdcard_readsectors(0,1,(void*)&sect)) return FAT_ERR_SDREAD; // Error reading MBR
  else {
    temp=sect[510] + (sect[511]<<8);
    if(temp!=0xAA55) return FAT_ERR_MBR; // Check failed
    else {
      partition=-1;
      for(i=0;i<4;i++) { // Max four partition to check
        switch (sect[450+i*16]) { // Partition type
          case 0x06:  // FAT 16 CHS
            temp=sect[447+i*16]*63+sect[448+i*16]-1;
            if ((partition==-1) && temp) { 
              partition=i;
              fat.LBA_Begin=temp;
              fat.FAT_RecordLen=16;
            }
            break;
          case 0x0B: // FAT 32 CHS
            temp=sect[447+i*16]*63+sect[448+i*16]-1;
            if ((partition==-1) && temp) { 
              partition=i;
              fat.LBA_Begin=temp;
              fat.FAT_RecordLen=32;
            }
            break;
          case 0x0C: //FAT 32 LBA
            temp=sect[454+i*16]+(sect[455+i*16]<<8)+(sect[456+i*16]<<16)+(sect[457+i*16]<<24);
            if ((partition==-1) && temp) { 
              partition=i;
              fat.LBA_Begin=temp;
              fat.FAT_RecordLen=32;
            }
            break;
          case 0x0E: // FAT 16 LBA
            temp=sect[454+i*16]+(sect[455+i*16]<<8)+(sect[456+i*16]<<16)+(sect[457+i*16]<<24);
            if ((partition==-1) && temp) { 
              partition=i;
              fat.LBA_Begin=temp;
              fat.FAT_RecordLen=16;
            }
            break;
          default:
            break;
        }
        if (partition==-1) return FAT_ERR_PARTITION; // No valid partition found
        else {
          if (sdmmc_sdcard_readsectors(fat.LBA_Begin,1,(void*)&sect)) return FAT_ERR_SDREAD; // Error reading FAT ID
          else {
            temp=sect[510]+(sect[511]<<8);
            if(temp!=0xAA55) return FAT_ERR_FATID; // Check failed
            else {
              fat.BPB_BytsPerSec=sect[11]+(sect[12]<<8);
              fat.BPB_SecPerClus=sect[13];
              fat.BPB_RsvdSecCnt=sect[14]+(sect[15]<<8); 
              fat.BPB_NumFATs=sect[16];
              fat.BPB_RootDirEnt=sect[17] + (sect[18]<<8);
              if (fat.FAT_RecordLen==16){
                fat.BPB_RootDirEnt=sect[17] + (sect[18]<<8);
                fat.BPB_FATS=sect[22]+(sect[23]<<8);
                fat.BPB_RootClus=2;
              } else {
                fat.BPB_RootDirEnt=0;
                fat.BPB_FATS=sect[36]+(sect[37]<<8)+(sect[38]<<16)+(sect[39]<<24);
                fat.BPB_RootClus=sect[44]+(sect[45]<<8)+(sect[46]<<16)+(sect[47]<<24);
              }
              curfatsect=fat_begin_lba(fat);
              if (sdmmc_sdcard_readsectors(curfatsect,1,(void*)&fatsect)) return -1; 
              rootdirsect = lba_addr(fat.BPB_RootClus);
              return changedir("\\");
            }
          }
        }
      }
    }
  }
}

int movefirstdirentry(){ 
  if ((curdirsect!=lba_addr(curdir.DIR_FstClus))|(curdirsectorcluster!=0)){
    if (sdmmc_sdcard_readsectors(lba_addr(curdir.DIR_FstClus),1,(void*)&sect)) return -1; 
    curdirsect=lba_addr(curdir.DIR_FstClus);
    curdirsectorcluster=0;
  }  
  direntrycount=0;
  return movenextdirentry();
}

int movenextdirentry(){ 
uint8_t j;
  if (sect[32*(direntrycount)]==0) return -1;
  if(++direntrycount==17){
    if(movedirnextsect()) return -1;
    return movenextdirentry();
  } else {
    if ((sect[32*(direntrycount-1)]==0xE5)||(sect[32*(direntrycount-1) + 11] & 14)) return movenextdirentry();
    else {
      for(j=0;j<11;j++) curdirentry.DIR_Name[j]=sect[32*(direntrycount-1) + j];
      curdirentry.DIR_Attr = (sect[32*(direntrycount-1) + 11] & 16);
      curdirentry.DIR_FstClus = (sect[32*(direntrycount-1) + 20]<<16) + (sect[32*(direntrycount-1) + 21]<<24) + sect[32*(direntrycount-1) + 26] + (sect[32*(direntrycount-1) + 27]<<8);
      curdirentry.DIR_FileSize = sect[32*(direntrycount-1) + 28] + (sect[32*(direntrycount-1) + 29]<<8) + (sect[32*(direntrycount-1) + 30]<<16) + (sect[32*(direntrycount-1) + 31]<<24);
      return 0;
    }
  }
}

int changedir(char* buf){ // Change current directory
uint8_t i;
  if ((buf[0]==0)) return -1;
  if ((buf[0]=='\\')&&(buf[1]==0)){
    if (sdmmc_sdcard_readsectors(lba_addr(fat.BPB_RootClus),1,(void*)&sect)) return FAT_ERR_SDREAD;
    curdirsect = lba_addr(fat.BPB_RootClus);
    curdirsectorcluster=0;
    curdir.DIR_Name[0]='\\';
    curdir.DIR_Name[1]=0x20;
    curdir.DIR_Name[2]=0x20;
    curdir.DIR_Name[3]=0x20;
    curdir.DIR_Name[4]=0x20;
    curdir.DIR_Name[5]=0x20;
    curdir.DIR_Name[6]=0x20;
    curdir.DIR_Name[7]=0x20;
    curdir.DIR_Name[8]=0x20;
    curdir.DIR_Name[9]=0x20;
    curdir.DIR_Name[10]=0x20;
    curdir.DIR_Attr = 17;
    curdir.DIR_FstClus = fat.BPB_RootClus;
    curdir.DIR_FileSize = 0;
    direntrycount=0;
    return 0;
  }
  if ((buf[0]=='.')&&(buf[1]==0)) return (direntrycount=0);
  
  // note: ".." can be searched like any other dir 

  if (movefirstdirentry()) return -1;
  while(cmpname(buf,curdirentry.DIR_Name)) if (movenextdirentry()) return -2;
  if (curdirentry.DIR_Attr & ATT_DIRECTORY) { 
    if (sdmmc_sdcard_readsectors(lba_addr(curdirentry.DIR_FstClus),1,(void*)&sect)) return FAT_ERR_SDREAD;
    curdirsect = lba_addr(curdirentry.DIR_FstClus);
    curdirsectorcluster=0;
    for (i=0; i<11;i++) curdir.DIR_Name[i]=curdirentry.DIR_Name[i];
    curdir.DIR_Attr = curdirentry.DIR_Attr;
    curdir.DIR_FstClus = curdirentry.DIR_FstClus;
    curdir.DIR_FileSize = curdirentry.DIR_FileSize;;
    return (direntrycount=0);
  }
  return -1;
}

int openf(char* buf){ // Load first cluster of a file in buffer
  if (movefirstdirentry()) return -1;
  while(cmpname(buf,curdirentry.DIR_Name)) if (movenextdirentry()) return -2;
  if (!(curdirentry.DIR_Attr & ATT_DIRECTORY)) { 
    if (sdmmc_sdcard_readsectors(lba_addr(curdirentry.DIR_FstClus),1,(void*)&filesect)) return FAT_ERR_SDREAD;
    curfilesect = lba_addr(curdirentry.DIR_FstClus);
    curfile.DIR_Attr = curdirentry.DIR_Attr;
    curfile.DIR_FstClus = curdirentry.DIR_FstClus;
    curfile.DIR_FileSize = curdirentry.DIR_FileSize;;
    return (filesectorcount=curfilesectorcluster=0);
  }
  return -1;
}

uint8_t getcharf(uint32_t pos){
  return filesect[pos % SECTSIZE]; 
}

void setcharf(uint32_t pos, uint8_t val){
  filesect[pos % SECTSIZE] = val; 
}

uint32_t posoffile(){ //only for debug
  return (filesectorcount * SECTSIZE); 
}

uint8_t endoffile(){
  return ((filesectorcount * SECTSIZE)>= curfile.DIR_FileSize); 
}

int readf(){ // Read the next cluster of a file
  uint32_t nextsect;
  if (endoffile()) return -1; 
  filesectorcount++;
  if (++curfilesectorcluster == fat.BPB_SecPerClus){
    if ((nextsect = getnextsect(curfilesect))==0) return -1;  
    curfilesect=nextsect;
    curfilesectorcluster=0;
  }
  if (sdmmc_sdcard_readsectors(curfilesect+curfilesectorcluster,1,(void*)&filesect)) return -1; 
  else return 0;
}

int writef(){ // Flush changes on current buffer to the file (can modify only positions allocated on disk)
  return sdmmc_sdcard_writesector(curfilesect + curfilesectorcluster, (void*)& filesect);
}

static int movedirnextsect(){ 
  uint32_t nextsect;
  if (++curdirsectorcluster == fat.BPB_SecPerClus){
    nextsect = getnextsect(curdirsect);
    if (nextsect==0) return -1;  
    curdirsect=nextsect;
    curdirsectorcluster=0;
  }
  if (sdmmc_sdcard_readsectors(curdirsect+curdirsectorcluster,1,(void*)&sect)) return -1; 
  else return (direntrycount=0);
}

static uint32_t getnextsect(uint32_t sect){  // not tested yet
  uint32_t cluster, fatnumsect, fatentry;

  cluster = (sect - cluster_begin_lba()- 32 * (fat.BPB_RootDirEnt / fat.BPB_BytsPerSec)) / fat.BPB_SecPerClus + 2;
  if (fat.FAT_RecordLen==32){
    fatnumsect= lba_addr(cluster>>7);
    fatentry = cluster|127;
  } else {
    fatnumsect= lba_addr(cluster>>8);
    fatentry = cluster|255;
  }  
  if (curfatsect!=fatnumsect){
      if (sdmmc_sdcard_readsectors(fatnumsect,1,(void*)&fatsect)) return -1; 
      curfatsect=fatnumsect;
  }  
  if (fat.FAT_RecordLen==32) return fatsect[fatentry*4] + (fatsect[fatentry*4 +1]<<8) + (fatsect[fatentry*4 +2]<<16) + (fatsect[fatentry*4 +3]<<24);
  else return fatsect[fatentry*2] + (fatsect[fatentry*2 +1]<<8) ;
}

static int cmpname(char* name1, char* name2){
uint8_t i;
  for (i=0; (i<11) & !(((name1[i]==0x20)|(name1[i]==0))&(name2[i]==0x20)); i++) if (name1[i]!=name2[i]) return -1;
  return 0;
}

void getdirentryname(char* buf){
uint8_t i;
  for (i=0; i<11;i++) buf[i]=curdirentry.DIR_Name[i];
}

uint8_t getdirentryattr(){
  return curdirentry.DIR_Attr;
}

uint32_t getfilesize() {return curfile.DIR_FileSize;}
uint32_t getsect() {return curdirsect+curdirsectorcluster;}

void entryname2str(char* buf,char* entryname){
int j,i;
  for(i=j=0;j<8;j++) if (entryname[j]!=0x20) buf[i++]=entryname[j];
  if (entryname[8]==0x20) buf[i++]=0;
  else {
    buf[i++]=0x2E;
    for(j=8;j<11;j++) if (entryname[j]!=0x20) buf[i++]=entryname[j];
    buf[i]=0;
  }
}
