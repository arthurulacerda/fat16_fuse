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
  DWORD FirstRootDirSecNum;
  DWORD FirstDataSector;
  BYTE *Fat;
  BPB_BS Bpb;
} VOLUME;

void printBPB(BPB_BS Bpb) {
  int i;
  for (i = 0; i < 11; i++) {
    printf("%c", Bpb.BS_VollLab[i]);
  }
  printf("\n");
  for (i = 0; i < 8; i++) {
    printf("%c", Bpb.BS_FilSysType[i]);
  }
  printf("\n");
  printf("OEMNAME: %s\n", Bpb.BS_OEMName);
  printf("BytsPersec: %d\n", Bpb.BPB_BytsPerSec);
  printf("SecPerClus: %d\n", Bpb.BPB_SecPerClus);
  printf("NumFATS: %x\n", Bpb.BPB_NumFATS);
  printf("VollID: %d\n", Bpb.BS_VollID);
  printf("RootEntCont: %d\n\n", Bpb.BPB_RootEntCnt);
}

void printDIR(DIR_ENTRY Dir) {
  int i;
  printf("Name: ");
  for (i = 0; i < 11; i++) {
    printf("%c", Dir.DIR_Name[i]);
  }
  printf("\n");
  printf("Attr: %x\n", Dir.DIR_Attr);
  printf("NTRes: %d\n", Dir.DIR_NTRes);
  printf("CrtTimeTenth: %d\n", Dir.DIR_CrtTimeTenth);
  printf("CrtTime: %d\n", Dir.DIR_CrtTime);
  printf("CrtDate: %d\n", Dir.DIR_CrtDate);
  printf("LstAccDate: %d\n", Dir.DIR_LstAccDate);
  printf("FstClusHI: %d\n", Dir.DIR_FstClusHI);
  printf("WrtTime: %d\n", Dir.DIR_WrtTime);
  printf("WrtDate: %d\n", Dir.DIR_WrtDate);
  printf("FstClusLO: %d\n", Dir.DIR_FstClusLO);
  printf("FileSize: %d\n\n", Dir.DIR_FileSize);
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

  int pathsize = 1;
  int i,j;
  char letter = '0';

  // Counting number of files
  for(i = 0; path_entry[i] != '\0'; i++){
    if(path_entry[i] == '/'){
      if(path_entry[i+1] != '\0'){
      	pathsize++;
      }
    }
  }

  char** path = (char**) malloc (pathsize*sizeof(char*));

  // Dividing path names in separated file names
  const char token[2] = "/";
  char *slice;
  
  i = 0;

  slice = strtok(path_entry, token);
  while(i<pathsize){
    path[i++] = slice;
    //printf("\n\n%s\n\n",path[i-1]);
    slice = strtok(NULL,token);
  }
  
  char ** format_path = (char**) malloc (pathsize*sizeof(char*));
  for(i = 0; i < pathsize; i++){
    format_path[i] = (char*) malloc (11*sizeof(char));
  }


  int name_size = 0; // Size of name field
  int ext_size = 0; // Size of extension field
  int k;
  int dotflag = 0; // Verify if the dot character '.' has appeared.

  // Verifying if each directory is valid and formatting it for FAT
  for(i = 0; i < pathsize; i++){
  	for(j = 0, k = 0; ; j++, k++){
  		if(path[i][j] == '.'){
  			// Dir .
  			if(j == 0 && path[i][j+1] == '\0'){
  				format_path[i][0] = '.';
  				for(k = 1; k < 11; k++){
  					format_path[i][k] = ' ';
  				}
  				break;
  			}

  			// Dir ..
  			if(j == 0 && path[i][j+1] == '.' && path[i][j+2] == '\0'){
  				format_path[i][0] = '.';
  				format_path[i][1] = '.';
  				for(k = 2; k < 11; k++){
  					format_path[i][k] = ' ';
  				}
  				break;
  			}

  			// Check ocurrency of past dot character.
  			if(!dotflag){
  				if(path[i][j+1] == '\0'){
  					printf("Error: Empty extension after dot character (.) inf file %s\n",path[i]);
  					exit(1);
  				}

  				dotflag = 1;
  				for(;k < 8; k++){
  					format_path[i][k] = ' ';
  				}
  				k = 7;
  			} else{
  				printf("Error: More than one dot character (.) in file %s\n",path[i]);
  				exit(1);
  			}
  		}
  		// End of file name
  		else if(path[i][j] == '\0'){
  			for(;k<11;k++){
  				format_path[i][k] = ' ';
  			}

  			break;
  		}
  		// Lower case to Upper case
  		else if(path[i][j] >= 'a' && path[i][j] <='z'){
  			format_path[i][k] = path[i][j] - 32;

  			if(name_size > 8 || ext_size > 3){
  				printf("Error: Overfill of name or extension field in file %s\n",path[i]);
  				exit(1);
  			}
  		}
  		// Other character accepted
  		else if((path[i][j] >= 'A' && path[i][j] <='Z') || (path[i][j] >= '0' && path[i][j] <='9') || 
  			path[i][j] == '$' || path[i][j] == '%' || path[i][j] == '\'' || path[i][j] == '-' || path[i][j] == '_' ||
  			path[i][j] == '@' || path[i][j] == '~' || path[i][j] == '`' || path[i][j] == '!' || path[i][j] == '(' ||
  			path[i][j] == ')' || path[i][j] == '{' || path[i][j] == '}' || path[i][j] == '^' || path[i][j] == '#' ||
  			path[i][j] == '&'){
  			format_path[i][k] = path[i][j];
  			if(dotflag)
  				ext_size++;
  			else
  				name_size++;

  			if(name_size > 8 || ext_size > 3){
  				printf("Error: Overfill of name or extension field in file %s\n",path[i]);
  				exit(1);
  			}
  		}
  		else{
  			printf("Error: Character not accepted in file %s\n",path[i]);
  			exit(1);
  		}
    }
  }

  *pathsz = pathsize;
  free(path);
  return format_path;
}

VOLUME *fat16_init(FILE *fd) {
  VOLUME Fat16;
  VOLUME *Vol = &Fat16;
  Vol->Fat = NULL;

  /* BPB */
  sector_read(fd, 0, &Vol->Bpb);
  printBPB(Vol->Bpb);

  /* First sector of the root directory */
  Vol->FirstRootDirSecNum = Vol->Bpb.BPB_RsvdSecCnt + (Vol->Bpb.BPB_FATSz16 * Vol->Bpb.BPB_NumFATS);

  /* Number of sectors in the root directory */
  DWORD RootDirSectors = ((Vol->Bpb.BPB_RootEntCnt * 32) +
      (Vol->Bpb.BPB_BytsPerSec - 1)) / Vol->Bpb.BPB_BytsPerSec;

  /* First sector of the data region (cluster #2) */
  Vol->FirstDataSector = Vol->Bpb.BPB_RsvdSecCnt + (Vol->Bpb.BPB_NumFATS *
      Vol->Bpb.BPB_FATSz16) + RootDirSectors;

  return Vol;
}

void find_subdir(FILE *fd, VOLUME *Vol, DIR_ENTRY Dir, char **path, int pathsize) {
  int pathdepth, i, j, DirSecCnt = 1, cmpstring;
  BYTE buffer[BYTES_PER_SECTOR];

  // Search the other files from path
  for (pathdepth = 1; pathdepth < pathsize; pathdepth+1) {
    WORD ClusterN = Dir.DIR_FstClusLO;
    WORD FatClusEntryVal = fat_entry_by_cluster(fd, Vol, Dir, ClusterN);

    /* First sector of any valid cluster */
    WORD FirstSectorofCluster = ((ClusterN - 2) * Vol->Bpb.BPB_SecPerClus) + Vol->FirstDataSector;
    printDIR(Dir);

    sector_read(fd, FirstSectorofCluster, &buffer);
    for (i = 1; Dir.DIR_Name[0] != 0x00; i++) {
      memcpy(&Dir, &buffer[((i - 1) * 32) % BYTES_PER_SECTOR], 32);

      /* Comparing strings */
      cmpstring = 1;
      for (j = 0; j < 11; j++) {
        if (Dir.DIR_Name[j] != path[pathdepth][j]) {
          cmpstring = 0;
          break;
        }
      }

      /* If the path is only one file (ATTR_ARCHIVE) and it is located in the
       * root directory, stop searching */
      if (cmpstring && Dir.DIR_Attr == 0x20) {
        printf("Found the file %s in the %s directory!\n", Dir.DIR_Name,
            path[pathdepth - 1]);
        printDIR(Dir);
        exit(0);
      }

      if (cmpstring && Dir.DIR_Attr == 0x10) {
        printf("Found the file %s in the %s directory!\n", Dir.DIR_Name,
            path[pathdepth - 1]);
        printDIR(Dir);
        exit(0);
      }

      if (i % 16 == 0) {
        if (DirSecCnt < Vol->Bpb.BPB_SecPerClus) {
          sector_read(fd, Vol->FirstRootDirSecNum + DirSecCnt, &buffer);
          DirSecCnt++;
        } else {
          // End of cluster
          // Search for the next cluster
          if (FatClusEntryVal == 0xffff) {
            printf("%s: file not found\n", path[pathdepth]);
            exit(0);
          }
        }
      }
    }
    if (pathdepth == 1) break;
  }

}

int main(int argc, char **argv) {
  if (argv[1] == NULL || argv[2] == NULL) {
    printf("Usage: ./run_fat16 <FAT16 image> <path name>\n");
    exit(0);
  }

  /* Open FAT16 image file */
  FILE *fd = fopen(argv[1], "rb");
  if (!fd) {
    printf("%s: file not found\n", argv[1]);
    exit(0);
  }

  int pathsize;
  char **path = path_treatment(argv[2], &pathsize);

  /* Initializing a FAT16 volume */
  VOLUME *Vol = fat16_init(fd);

  /* Buffer to store bytes from sector_read */
  BYTE buffer[BYTES_PER_SECTOR];

  /* Root directory */
  DIR_ENTRY Root;

  /* Searching in the root directory first */
  int RootDirCnt = 1, i, j, cmpstring = 1;
  sector_read(fd, Vol->FirstRootDirSecNum, &buffer);

  for (i = 1; i <= Vol->Bpb.BPB_RootEntCnt; i++) {
    memcpy(&Root, &buffer[((i - 1) * 32) % BYTES_PER_SECTOR], 32);

    /* If the directory entry is free, all the next directory entries are also
     * free. So this file/directory could not be found */
    if (Root.DIR_Name[0] == 0x00) {
      exit(0);
    }

    // Comparing strings
    cmpstring = 1;
    for (j = 0; j < 11; j++) {
      if (Root.DIR_Name[j] != path[0][j]) {
        cmpstring = 0;
        break;
      }
    }

    /* If the path is only one file (ATTR_ARCHIVE) and it is located in the
     * root directory, stop searching */
    if (cmpstring && Root.DIR_Attr == 0x20) {
    	printf("Found the file %s in the root directory!\n", Root.DIR_Name);
      printDIR(Root);
    	exit(0);
    }

    /* If the first level of the path is a directory, continue searching
     * in the root's sub-directories */
    if (cmpstring == 1) {
      find_subdir(fd, Vol, Root, path, pathsize);
    }

    /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries)
     * Read next sector */
    if (i % 16 == 0 && i != Vol->Bpb.BPB_RootEntCnt) {
      sector_read(fd, Vol->FirstRootDirSecNum + RootDirCnt, &buffer);
      RootDirCnt++;
    }
  }
  fclose(fd);
  return 0;
}
