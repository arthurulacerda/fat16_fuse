#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "sector.h"
#include "sector.c"

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

typedef struct {
  BYTE BS_jmpBoot[3];
  BYTE BS_OEMName[8];
  WORD BPB_BytsPerSec;
  BYTE BPB_SecPerClus;
  WORD BPB_RsvdSecCnt;
  BYTE BPB_NumFATS;
  WORD BPB_RootEntCnt;
  WORD BPB_TotSec16;
  BYTE BPB_Media;
  WORD BPB_FatSz16;
  WORD BPB_SecPerTrk;
  WORD BPB_NumHeads;
  DWORD BPB_HiddSec;
  DWORD BPB_TotSec32;
  BYTE BS_DrvNum;
  BYTE BS_Reserved1;
  BYTE BS_BootSig;
  DWORD BS_VollID;
  BYTE BS_VollLab[11];
  BYTE BS_FilSysType[8];
  BYTE Hex1[448];
  WORD Signature_word;
} __attribute__ ((packed)) BPB_BS;

typedef struct {
  BYTE DIR_Name[11];
  BYTE DIR_Attr;
  BYTE DIR_NTRes;
  BYTE DIR_CrtTimeTenth;
  WORD DIR_CrtTime;
  WORD DIR_CrtDate;
  WORD DIR_LstAccDate;
  WORD DIR_FstClusHI;
  WORD DIR_WrtTime;
  WORD DIR_WrtDate;
  WORD DIR_FstClusLO;
  DWORD DIR_FileSize;
} __attribute__ ((packed)) DIR_ENTRY;

/** Reads the first 512 bytes of the image, that represents the BPB;
* @param Bpb: Pointer to the Bpb struct that will store all the information.
* @param fd: File descriptor to read the Bpb sector.
*/

void printBPB(BPB_BS *Bpb, FILE *out) {
  int i;
  for (i = 0; i < 11; i++) {
    fprintf(out, "%c", Bpb->BS_VollLab[i]);
  }
  fprintf(out, "\n");
  for (i = 0; i < 8; i++) {
    fprintf(out, "%c", Bpb->BS_FilSysType[i]);
  }
  fprintf(out, "\n");
  fprintf(out, "OEMNAME: %s\n", Bpb->BS_OEMName);
  fprintf(out, "BytsPersec: %d\n", Bpb->BPB_BytsPerSec);
  fprintf(out, "SecPerClus: %d\n", Bpb->BPB_SecPerClus);
  fprintf(out, "NumFATS: %x\n", Bpb->BPB_NumFATS);
  fprintf(out, "VollID: %d\n", Bpb->BS_VollID);
  fprintf(out, "RootEntCont: %d\n\n", Bpb->BPB_RootEntCnt);
}

void printDIR(DIR_ENTRY *Dir, FILE *out) {
  int i;
  fprintf(out, "Name: ");
  for (i = 0; i < 11; i++) {
    fprintf(out, "%c", Dir->DIR_Name[i]);
  }
  fprintf(out, "\n");
  fprintf(out, "Attr: %x\n", Dir->DIR_Attr);
  fprintf(out, "NTRes: %d\n", Dir->DIR_NTRes);
  fprintf(out, "CrtTimeTenth: %d\n", Dir->DIR_CrtTimeTenth);
  fprintf(out, "CrtTime: %d\n", Dir->DIR_CrtTime);
  fprintf(out, "CrtDate: %d\n", Dir->DIR_CrtDate);
  fprintf(out, "LstAccDate: %d\n", Dir->DIR_LstAccDate);
  fprintf(out, "FstClusHI: %d\n", Dir->DIR_FstClusHI);
  fprintf(out, "WrtTime: %d\n", Dir->DIR_WrtTime);
  fprintf(out, "WrtDate: %d\n", Dir->DIR_WrtDate);
  fprintf(out, "FstClusLO: %d\n", Dir->DIR_FstClusLO);
  fprintf(out, "FileSize: %d\n\n", Dir->DIR_FileSize);
}

/*void readFileFAT(FILE* fd, DIR *Dir, WORD fatPosition){
  int fatSector = fatPosition / 32; //512 has 32 slots of 16bytes.

}*/

int main(int argc, char **argv) {
  if (argv[1] == NULL) {
    printf("Missing FAT16 image file!\n");
    exit(0);
  }
  FILE *fd = fopen(argv[1], "rb");
  FILE *out = fopen("out", "w");
  BYTE buffer[BYTES_PER_SECTOR];

  /* BPB */
  BPB_BS Bpb;

  /* Reading BPB */
  sector_read(fd, 0, &Bpb);
  printBPB(&Bpb, out);

  /* Root directory */
  DIR_ENTRY *Root = calloc(Bpb.BPB_RootEntCnt, sizeof *Root);
  if (!Root) exit(1);

  /* First sector of the root directory */
  unsigned int FirstRootDirSecNum = Bpb.BPB_RsvdSecCnt + (Bpb.BPB_FatSz16 * Bpb.BPB_NumFATS);

  /* Reading root directory */
  int RootDirCnt = 1, i;
  sector_read(fd, FirstRootDirSecNum, &buffer);
  memcpy(&Root[0], buffer, 32);
  for (i = 1; i < Bpb.BPB_RootEntCnt && Root[i - 1].DIR_Name[0] != 0x00; i++) {
    printDIR(&Root[i - 1], out);
    memcpy(&Root[i], &buffer[i * 32], 32);

    /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries)
     * Read next sector */
    if (i % 16 == 0) {
      sector_read(fd, FirstRootDirSecNum + RootDirCnt, &buffer);
      RootDirCnt++;
    }
  }
  //readFileFAT(fd,&root,root.DIR_FstClusHI);

  /* First sector of the Data Region */
  WORD RootDirSectors = ((Bpb.BPB_RootEntCnt * 32) + (Bpb.BPB_BytsPerSec - 1)) / Bpb.BPB_BytsPerSec;

  /* Number of FAT entries */

  /* FAT */
  WORD *Fat = calloc(Bpb.BPB_FatSz16, sizeof *Fat);
  if (!Fat) exit(2);

  unsigned int ClusterN = Root[0].DIR_FstClusLO;
  unsigned int FATOffset = ClusterN * 2;
  unsigned int FatSecNum = Bpb.BPB_RsvdSecCnt + (FATOffset / BYTES_PER_SECTOR);
  unsigned int FatEntOffset = FATOffset % BYTES_PER_SECTOR;
  sector_read(fd, FatSecNum, &buffer);
  WORD FatClusEntryVal = *((WORD *) &buffer[FatEntOffset]);
  printf("%d\n", FatClusEntryVal);

  return 0;
}
