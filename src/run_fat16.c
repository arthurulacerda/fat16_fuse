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
  WORD BPB_FATSz16;
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
  BYTE Reserved[448];
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

typedef struct {
  BPB_BS Bpb;
  DWORD FirstRootDirSecNum;
  DWORD FirstDataSector;
  BYTE *Fat;
} VOLUME;

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

void *emalloc(size_t size) {
  void *p = malloc(size);
  if (!p) {
    fprintf(stderr, "Out of memory!\n");
    exit(EXIT_FAILURE);
  }
  return p;
}

WORD fat_entry_by_cluster(FILE *fd, VOLUME *Vol, DIR_ENTRY Dir, WORD ClusterN) {
  WORD FatBuffer[BYTES_PER_SECTOR];
  WORD FATOffset = ClusterN * 2;
  WORD FatSecNum = Vol->Bpb.BPB_RsvdSecCnt + (FATOffset / Vol->Bpb.BPB_BytsPerSec);
  WORD FatEntOffset = FATOffset % Vol->Bpb.BPB_BytsPerSec;
  sector_read(fd, FatSecNum, &FatBuffer);
  return *((WORD *) &FatBuffer[FatEntOffset]);
}

char** path_treatment(char* path_entry, int* pathsz){

  int path_size = 1;
  int i,j;
  char letter = '0';

  // Counting number of files
  for(i = 0; path_entry[i] != '\0'; i++){
    if(path_entry[i] == '/')
      path_size++;
  }

  char** path = (char**) emalloc (path_size*sizeof(char*));

  // Dividing path names in separated file names
  const char token[2] = "/";
  char *slice;
  
  i = 0;

  slice = strtok(path_entry, token);
  while(slice != NULL){
    path[i++] = slice;
    //printf("\n\n%s\n\n",path[i-1]);
    slice = strtok(NULL,token);
  }
  
  // Verifying if each directory is valid and making it uppercase
  for(i = 0; i < path_size; i++){
    for(j = 0; path[i][j] != '\0'; j++){
      
      if((path[i][j] >= 'A' && path[i][j] <='Z')||(path[i][j] >= '0' && path[i][j] <='9'))
        continue;
      if(path[i][j] >= 'a' && path[i][j] <='z'){
        path[i][j] -= 32;
        continue;
      }
      if(path[i][j] == '.'){
        if(!(j == 0 || j==1 && path[i][0] == '.'))
          path[i][j] = ' ';
        continue;
      }
      if(path[i][j] == '$' || path[i][j] == '%' || path[i][j] == '\'' || path[i][j] == '-' || path[i][j] == '_' || path[i][j] == '@' || path[i][j] == '~' || path[i][j] == '`' || path[i][j] == '!' || path[i][j] == '(' || path[i][j] == ')' || path[i][j] == '{' || path[i][j] == '}' || path[i][j] == '^' || path[i][j] == '#' || path[i][j] == '&')
        continue;

      printf("Invalid Path: %s\n",path[i]);
      exit(1);
    }
    //printf("%s\n", path[i]);
  }

  *pathsz = path_size;

  return path;
}

VOLUME *fat16_init(FILE *fd, FILE *out) {
  VOLUME Fat16;
  VOLUME *Vol = &Fat16;
  Vol->Fat = NULL;

  /* BPB */
  sector_read(fd, 0, &Vol->Bpb);
  printBPB(&Vol->Bpb, out);

  /* First sector of the root directory */
  Vol->FirstRootDirSecNum = Vol->Bpb.BPB_RsvdSecCnt + (Vol->Bpb.BPB_FATSz16 *
      Vol->Bpb.BPB_NumFATS);

  /* Number of sectors in the root directory */
  DWORD RootDirSectors = ((Vol->Bpb.BPB_RootEntCnt * 32) +
      (Vol->Bpb.BPB_BytsPerSec - 1)) / Vol->Bpb.BPB_BytsPerSec;

  /* First sector of the data region (cluster #2) */
  Vol->FirstDataSector = Vol->Bpb.BPB_RsvdSecCnt + (Vol->Bpb.BPB_NumFATS *
      Vol->Bpb.BPB_FATSz16) + RootDirSectors;

  return Vol;
}

int main(int argc, char **argv) {
  if (argv[1] == NULL) {
    printf("Missing FAT16 image file!\n");
    exit(0);
  }
  if (argv[2] == NULL) {
    printf("Missing PATH!\n");
    exit(0);
  }

  /* Open FAT16 image file */
  FILE *fd = fopen(argv[1], "rb");

  /* Open output file */
  FILE *out = fopen("out", "w");

  /* Initializing a FAT16 volume */
  VOLUME *Vol = fat16_init(fd, out);

  /* Buffer to store bytes from sector_read */
  BYTE buffer[BYTES_PER_SECTOR];

  int path_size;
  char **path = path_treatment(argv[2], &path_size);

  /* Represents the root directory */
  DIR_ENTRY Root;

  /* Searching in the root directory first */
  int RootDirCnt = 1, i = 1, j, flag = 1, firstPathFile;
  sector_read(fd, Vol->FirstRootDirSecNum, &buffer);
  for (i = 1; i <= Vol->Bpb.BPB_RootEntCnt; i++) {
    memcpy(&Root, &buffer[((i - 1) * 32) % BYTES_PER_SECTOR], 32);

    /* If the directory entry is free, all the next directory entries are also
     * free. So this file/directory could not be found */
    if (Root.DIR_Name[0] == 0x00) {
      fprintf(stdout, "%s: No such file or directory\n", path[0]);
      exit(0);
    }
    printDIR(&Root, out);

    // Comparing strings
    flag = 1;
    for (j = 0; Root.DIR_Name[j] != '\0'; j++) {
      if (Root.DIR_Name[j] != path[0][j]) {
        flag = 0;
        break;
      }
    }

    /* If the path is only one file (ATTR_ARCHIVE) and it is located in the
     * root directory, stop the searching */
    if (flag == 1 && Root.DIR_Attr == 0x20) {
      printf("Found the file %s in the root directory!\n", Root.DIR_Name);
      exit(0);
    }

    // Verifying if the name ended
    if (flag == 1 && !(Root.DIR_Name[j] >= 33 && Root.DIR_Name[j] <= 126)) {
      firstPathFile = i;
      // TODO: call a function to keep searching in sub-directories of the root
    }

    /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries)
     * Read next sector */
    if (i % 16 == 0 && i != Vol->Bpb.BPB_RootEntCnt) {
      sector_read(fd, Vol->FirstRootDirSecNum + RootDirCnt, &buffer);
      RootDirCnt++;
    }
  }
  //readFileFAT(fd,&root,root.DIR_FstClusHI);

  DIR_ENTRY currentDir;
  currentDir = Root;
  int pathDepth;
  int currentPathFile = firstPathFile;
  int currentEntry = 0;

  // Search the other files from path
  for(pathDepth = 1; pathDepth < path_size; pathDepth+1){
    WORD ClusterN = currentDir.DIR_FstClusLO;
    WORD FatClusEntryVal = fat_entry_by_cluster(fd, Vol, currentDir, ClusterN);

    /* Number of sectors in the root directory */
    WORD currentDirSectors = ((Vol->Bpb.BPB_RootEntCnt * 32) +
        (Vol->Bpb.BPB_BytsPerSec - 1)) / Vol->Bpb.BPB_BytsPerSec;

    /* First sector of any valid cluster */
    WORD FirstSectorofCluster = ((ClusterN - 2) * Vol->Bpb.BPB_SecPerClus) + Vol->FirstDataSector;

    DIR_ENTRY Dir;
    sector_read(fd, FirstSectorofCluster, &buffer);
    memcpy(&Dir, &buffer[currentEntry*32], 32);
    printDIR(&Dir, out);

    if(pathDepth == 1)
      break;
  }

  /* Number of FAT entries */

  /* FAT */
  //WORD *Fat = calloc(Bpb.BPB_FATSz16, sizeof *Fat);
  //if (!Fat) exit(2);
  
  /* Determination of FAT entry for a cluster */
  

  


  fclose(fd);
  fclose(out);

  //sector_read(fd, 4, &FatBuffer);
  //printf("%d\n", FatBuffer[2]);
  return 0;
}
