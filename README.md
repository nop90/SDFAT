SDFAT
=====

Minimalist SD FAT filesystem IO

   Still in alpha state, but almost stable and usable

   Uses 3DS SDIO Handling code from Kane49 adaption of the sdmmc module made for libnds (sources included)   

   based on information from 
   - http://www.pjrc.com/tech/8051/ide/fat32.html

Abstract:
=========

I wrote this code for using on ARM9 Bare Metal 3DS Homebrew.

I started using PetifFS, but I'm very unluky with it since I have Sd and SDHC cards using CHS partitions addressing in MBT instead of LBA, and one of my card has the FAT partition not in the first MBR entry, and PetFS can't handle non of this cases.

Another problem was that on 3DS compiled file size must be under about 22K, so i decided to write my own library of the smallest compiled size (and number of functionalities of course) I was able.

I didn't use any of PetitFS code, but wrote everything from scratch using online informations, raw disk analysis tool (mostly winhex) and a lot of debugging. 

Limitations:
============
   - only FAT16 and FAT 32
   - only 8.3 file name support
   - only 1 file opened at a time
   - no file or directory creation
   - no file resizing, can write only on already allocated sectors

Know bugs:
============
   - wrong fiile/folder addressing on FAT 16 (problem identified, but in low periority to fix ...) 
   - reading a file can affect the directory listing if the dir has more than few files. Fixing in progress.
   - cluster change not fully tested and probably still buggy
