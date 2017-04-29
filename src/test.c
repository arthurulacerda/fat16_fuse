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

int main() {
  FILE *fd = fopen("fat16.img", "rb");
  BPB_BS bpb;
  int i;
  //sector_read(fd, 0, &BPB);
  fread(&bpb, 512, 1, fd);
  for (i = 0; i < 11; i++) {
    printf("%c", bpb.BS_VollLab[i]);
  }
  printf("\n");
  for (i = 0; i < 8; i++) {
    printf("%c", bpb.BS_FilSysType[i]);
  }
  printf("\n");
  printf("%s\n", bpb.BS_OEMName);
  printf("%d\n", bpb.BPB_BytsPerSec);
  printf("%d\n", bpb.BPB_SecPerClus);
  printf("%d\n", bpb.BPB_SecPerClus);
  printf("%x\n", bpb.BPB_NumFATS);
  printf("%d\n", bpb.BS_VollID);
  return 0;
}
