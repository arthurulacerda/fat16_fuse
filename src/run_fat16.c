#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "sector.h"
#include "sector.c"

typedef struct {
  uint8_t BS_jmpBoot[3];
  uint8_t BS_OEMName[8];
  uint16_t BPB_BytsPerSec;
  uint8_t BPB_SecPerClus;
  uint16_t BPB_RsvdSecCnt;
  uint8_t BPB_NumFATS;
  uint16_t BPB_RootEntCnt;
  uint16_t BPB_TotSec16;
  uint8_t BPB_Media;
  uint16_t BPB_FatSz16;
  uint16_t BPB_SecPerTrk;
  uint16_t BPB_NumHeads;
  uint32_t BPB_HiddSec;
  uint32_t BPB_TotSec32;
  uint8_t BS_DrvNum;
  uint8_t BS_Reserved1;
  uint8_t BS_BootSig;
  uint32_t BS_VollID;
  uint8_t BS_VollLab[11];
  uint8_t BS_FilSysType[8];
  uint8_t Hex1[448];
  uint16_t Signature_word;
} __attribute__ ((packed)) BPB_BS;

typedef struct {
  uint8_t DIR_Name[11];
  uint8_t DIR_Attr;
  uint8_t DIR_NTRes;
  uint8_t DIR_CrtTimeTenth;
  uint16_t DIR_CrtTime;
  uint16_t DIR_CrtDate;
  uint16_t DIR_LstAccDate;
  uint16_t DIR_FstClusHI;
  uint16_t DIR_WrtTime;
  uint16_t DIR_WrtDate;
  uint16_t DIR_FstClusLO;
  uint32_t DIR_FileSize;
} __attribute__ ((packed)) DIR_ENTRY;

/** Reads the first 512 bytes of the image, that represents the BPB;
* @param Bpb: Pointer to the Bpb struct that will store all the information.
* @param fd: File descriptor to read the Bpb sector.
*/

void printBPB(BPB_BS *Bpb) {
  int i;
  for (i = 0; i < 11; i++) {
    printf("%c", Bpb->BS_VollLab[i]);
  }
  printf("\n");
  for (i = 0; i < 8; i++) {
    printf("%c", Bpb->BS_FilSysType[i]);
  }
  printf("\n");
  printf("OEMNAME: %s\n", Bpb->BS_OEMName);
  printf("BytsPersec: %d\n", Bpb->BPB_BytsPerSec);
  printf("SecPerClus: %d\n", Bpb->BPB_SecPerClus);
  printf("NumFATS: %x\n", Bpb->BPB_NumFATS);
  printf("VollID: %d\n", Bpb->BS_VollID);
  printf("RootEntCont: %d\n\n", Bpb->BPB_RootEntCnt);
}

void printDIR(DIR_ENTRY *Dir) {
  int i;
  printf("Name: ");
  for (i = 0; i < 11; i++) {
    printf("%c", Dir->DIR_Name[i]);
  }
  printf("\n");
  printf("Attr: %x\n", Dir->DIR_Attr);
  printf("NTRes: %d\n", Dir->DIR_NTRes);
  printf("CrtTimeTenth: %d\n", Dir->DIR_CrtTimeTenth);
  printf("CrtTime: %d\n", Dir->DIR_CrtTime);
  printf("CrtDate: %d\n", Dir->DIR_CrtDate);
  printf("LstAccDate: %d\n", Dir->DIR_LstAccDate);
  printf("FstClusHI: %d\n", Dir->DIR_FstClusHI);
  printf("WrtTime: %d\n", Dir->DIR_WrtTime);
  printf("WrtDate: %d\n", Dir->DIR_WrtDate);
  printf("FstClusLO: %d\n", Dir->DIR_FstClusLO);
  printf("FileSize: %d\n", Dir->DIR_FileSize);
}

/*void readFileFAT(FILE* fd, DIR *Dir, uint16_t fatPosition){
  int fatSector = fatPosition / 32; //512 has 32 slots of 16bytes.

}*/

void searchRootDIR(DIR_ENTRY *Dir, uint8_t *buffer, uint16_t RootDirSectors) {
}

int main(int argc, char **argv) {
  if (argv[1] == NULL) {
    printf("Missing FAT16 image file!\n");
    exit(0);
  }
  FILE *fd = fopen(argv[1], "rb");
  uint8_t buffer[BYTES_PER_SECTOR];
  BPB_BS Bpb;

  /* Reading BPB */
  sector_read(fd, 0, &Bpb);
  printBPB(&Bpb);

  uint16_t RootEntCnt = Bpb.BPB_RootEntCnt;
  DIR_ENTRY Root[RootEntCnt];

  /* Reading Root Directory
   * Jumps both FAT and the BPB sector. */
  unsigned int FirstRootDirSecNum = Bpb.BPB_RsvdSecCnt + (Bpb.BPB_FatSz16 * Bpb.BPB_NumFATS);
  sector_read(fd, FirstRootDirSecNum, &buffer);
  uint16_t RootDirSectors = ((Bpb.BPB_RootEntCnt * 32) + (Bpb.BPB_BytsPerSec - 1)) / Bpb.BPB_BytsPerSec;
  printf("RootDirSectors: %d\n", RootDirSectors);

  /* Searching for file/directory in the Root Directory */
  int i, j, k;
  memcpy(&Root[0], &buffer, 32);
  if (Root[0].DIR_Name[0] != 0) {
    printDIR(&Root[0]);
    printf("\n");
  }
  for (k = 16; k <= RootDirSectors; k += 16) {
    for (j = 1; j <= 16; j++) {
      for (i = 0; i < 32; i++) {
        buffer[i] = buffer[i + (j * 32)];
      }
      memcpy(&Root[j], &buffer, 32);
      if (Root[j].DIR_Name[0] != 0) {
        printDIR(&Root[j]);
        printf("\n");
      }
    }
    /* Out of bytes in this sector (1 sector == 512 bytes == 16 dir entries)
     * Read one more sector */
    sector_read(fd, FirstRootDirSecNum, buffer);
  }

  //readFileFAT(fd,&root,root.DIR_FstClusHI);

  return 0;
}
