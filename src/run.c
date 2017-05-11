#include <stdio.h>
#include <stdint.h>
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
} __attribute__ ((packed)) DIR;

/** Reads the first 512 bytes of the image, that represents the BPB;
* @param bpb: Pointer to the bpb struct that will store all the information.
* @param fd: File descriptor to read the bpb sector.
*/

void printBPB(BPB_BS *bpb){
  int i;
  for (i = 0; i < 11; i++) {
    printf("%c", bpb->BS_VollLab[i]);
  }
  printf("\n");
  for (i = 0; i < 8; i++) {
    printf("%c", bpb->BS_FilSysType[i]);
  }
  printf("\n");
  printf("OEMNAME: %s\n", bpb->BS_OEMName);
  printf("BytsPersec: %d\n", bpb->BPB_BytsPerSec);
  printf("SecPerClus: %d\n", bpb->BPB_SecPerClus);
  printf("NumFATS: %x\n", bpb->BPB_NumFATS);
  printf("VollID: %d\n\n", bpb->BS_VollID);
}

void printDIR(DIR *dir){
  int i;
  for (i = 0; i < 11; i++) {
    printf("%c", dir->DIR_Name[i]);
  }
  printf("\n");
  printf("Attr: %x\n", dir->DIR_Attr);
  printf("NTRes: %d\n", dir->DIR_NTRes);
  printf("CrtTimeTenth: %d\n", dir->DIR_CrtTimeTenth);
  printf("CrtTime: %d\n", dir->DIR_CrtTime);
  printf("CrtDate: %d\n", dir->DIR_CrtDate);
  printf("LstAccDate: %d\n", dir->DIR_LstAccDate);
  printf("FstClusHI: %d\n", dir->DIR_FstClusHI);
  printf("WrtTime: %d\n", dir->DIR_WrtTime);
  printf("WrtDate: %d\n", dir->DIR_WrtDate);
  printf("FstClusLO: %d\n", dir->DIR_FstClusLO);
  printf("FileSize: %d\n", dir->DIR_FileSize);
}

/*void readFileFAT(FILE* fd, DIR *dir, uint16_t fatPosition){
  int fatSector = fatPosition / 32; //512 has 32 slots of 16bytes.

}*/


int main() {
  FILE *fd = fopen("/home/arthur/fat16.img", "rb");
  
  BPB_BS bpb;
  DIR root;

  //Reading BPB
  sector_read(fd, 0, &bpb);
  printBPB(&bpb);

  /* Reading root directory
     Jumps both FATS and the BPB sector.  */
  int rootSector = bpb.BPB_RsvdSecCnt + bpb.BPB_FatSz16 * bpb.BPB_NumFATS;
  sector_read(fd, rootSector, &root);
  printDIR(&root);

  //readFileFAT(fd,&root,root.DIR_FstClusHI);

  return 0;
}
