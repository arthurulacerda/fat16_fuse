#include <stdint.h>
#include <string.h>

#include "sector.h"
#include "fat16.h"

/**
 * Prints BPB Attributes
 * ==================================================================================
 * Return
 * There is no return in this funcion.
 * ==================================================================================
 * Parameters
 * @Bpb: Structe that holds informations about BPB.
**/
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

/**
 * Prints Directory Attributes
 * ==================================================================================
 * Return
 * There is no return in this funcion.
 * ==================================================================================
 * Parameters
 * @Dir: Directory entry that attibutes will be read.
**/
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

/**
 * Given an cluster N, the function gets the FAT entry.
 * ==================================================================================
 * Return
 * There is no return in this funcion.
 * ==================================================================================
 * Parameters
 * @fd: File descriptor.
 * @Vol: Structure that contains essential data about the File System (BPB, first
 * sector of data and root section).
 * @CusterN: the Nth cluster of the data section.
**/
WORD fat_entry_by_cluster(FILE *fd, VOLUME *Vol, WORD ClusterN) {
  BYTE FatBuffer[BYTES_PER_SECTOR];
  WORD FATOffset = ClusterN * 2;
  WORD FatSecNum = Vol -> Bpb.BPB_RsvdSecCnt + (FATOffset / Vol -> Bpb.BPB_BytsPerSec);
  WORD FatEntOffset = FATOffset % Vol -> Bpb.BPB_BytsPerSec;
  sector_read(fd, FatSecNum, & FatBuffer);
  return *((WORD *) & FatBuffer[FatEntOffset]);
}

/**
 * This function recieves the string given by user input and divides it into an array
 * of strings, with the format of FAT16.
 * ==================================================================================
 * Return
 * @pathFormatted: array os string, each strings references a file of the path, in a
 * given depth, it also presents the format of FAT16 file names.
 * ==================================================================================
 * Parameters
 * @pathInput: User input string, the path of files to go trough.
 * @pathSz: Address of the variable that will keep the number of files in the path.
**/
char ** path_treatment(char *pathInput, int *pathSz) {

  int pathSize = 1;
  int i, j;
  char letter = '0';

  /* Counting number of files */
  for (i = 0; pathInput[i] != '\0'; i++) {
    if (pathInput[i] == '/') {
      if (pathInput[i + 1] != '\0') {
        pathSize++;
      }
    }
  }

  char ** path = (char ** ) malloc(pathSize * sizeof(char *));

  const char token[2] = "/";
  char *slice;

  i = 0;

  /* Dividing path into separated strings of file names */
  slice = strtok(pathInput, token);
  while (i < pathSize) {
    path[i++] = slice;
    slice = strtok(NULL, token);
  }

  char ** pathFormatted = (char ** ) malloc(pathSize * sizeof(char *));
  for (i = 0; i < pathSize; i++) {
    pathFormatted[i] = (char *) malloc(11 * sizeof(char));
  }

  int nameSize = 0;
  int extensionSize = 0;
  int k;
  int dotFlag = 0;

  /* Verifyies if each file of the path is valid input, and formats it for FAT16 */
  for (i = 0; i < pathSize; i++) {
    for (j = 0, k = 0;; j++, k++) {

      /* When an '.' character is analysed */
      if (path[i][j] == '.') {

        /* Verifies if it's . file */
        if (j == 0 && path[i][j + 1] == '\0') {
          pathFormatted[i][0] = '.';
          for (k = 1; k < 11; k++) {
            pathFormatted[i][k] = ' ';
          }
          break;
        }

        /* Verifies if it's .. file */
        if (j == 0 && path[i][j + 1] == '.' && path[i][j + 2] == '\0') {
          pathFormatted[i][0] = '.';
          pathFormatted[i][1] = '.';
          for (k = 2; k < 11; k++) {
            pathFormatted[i][k] = ' ';
          }
          break;
        }

        /* Check if there wasn't any other past occurrence of '.' character */
        if (!dotFlag) {
          /* If there is nothing after the dot character */
          if (path[i][j + 1] == '\0') {
            printf("Error: Empty extension after dot character (.) inf file %s\n", path[i]);
            exit(1);
          }

          /* Marks the occurrence of the '.' character' */
          dotFlag = 1;

          /* Fills with space ' ' character the name field leftover */
          for (; k < 8; k++) {
            pathFormatted[i][k] = ' ';
          }
          k = 7;

          /* If there are two '.' characters found in the file name */
        } else {
          printf("Error: More than one dot character (.) in file %s\n", path[i]);
          exit(1);
        }
      }

      /* End of the file name, fills with ' ' character the rest of the file
       * name and the file extension fields */
      else if (path[i][j] == '\0') {
        for (; k < 11; k++) {
          pathFormatted[i][k] = ' ';
        }

        break;
      }

      /* Turns lower case characters into upper case characters */
      else if (path[i][j] >= 'a' && path[i][j] <= 'z') {
        pathFormatted[i][k] = path[i][j] - 32;

        if (dotFlag)
          extensionSize++;
        else
          nameSize++;

        /* Checks for fields overflow */
        if (nameSize > 8 || extensionSize > 3) {
          printf("Error: Overfill of name or extension field in file %s\n", path[i]);
          exit(1);
        }
      }
      /* Other character accepted in the file name*/
      else if ((path[i][j] >= 'A' && path[i][j] <= 'Z') || (path[i][j] >= '0' && path[i][j] <= '9') ||
        path[i][j] == '$' || path[i][j] == '%' || path[i][j] == '\'' || path[i][j] == '-' || path[i][j] == '_' ||
        path[i][j] == '@' || path[i][j] == '~' || path[i][j] == '`' || path[i][j] == '!' || path[i][j] == '(' ||
        path[i][j] == ')' || path[i][j] == '{' || path[i][j] == '}' || path[i][j] == '^' || path[i][j] == '#' ||
        path[i][j] == '&') {
        pathFormatted[i][k] = path[i][j];
        if (dotFlag)
          extensionSize++;
        else
          nameSize++;

        /* Checks for fields overflow */
        if (nameSize > 8 || extensionSize > 3) {
          printf("Error: Overfill of name or extension field in file %s\n", path[i]);
          exit(1);
        }
      } else {
        printf("Error: Character not accepted in file %s\n", path[i]);
        exit(1);
      }
    }
  }

  *pathSz = pathSize;
  free(path);
  return pathFormatted;
}

/**
 * Reads BPB, calculates the first sector of the root and data sections.
 * ==================================================================================
 * Return
 * @Vol: Structure that contains essential data about the File System (BPB, first
 * sector of data and root section).
 * ==================================================================================
 * Parameters
 * @fd: File descriptor.
**/
VOLUME *fat16_init(FILE *fd) {
  VOLUME *Vol = malloc(sizeof *Vol);

  /* BPB */
  sector_read(fd, 0, & Vol -> Bpb);
  printBPB(Vol -> Bpb);

  /* First sector of the root directory */
  Vol -> FirstRootDirSecNum = Vol -> Bpb.BPB_RsvdSecCnt + (Vol -> Bpb.BPB_FATSz16 *Vol -> Bpb.BPB_NumFATS);

  /* Number of sectors in the root directory */
  DWORD RootDirSectors = ((Vol -> Bpb.BPB_RootEntCnt * 32) +
    (Vol -> Bpb.BPB_BytsPerSec - 1)) / Vol -> Bpb.BPB_BytsPerSec;

  /* First sector of the data region (cluster #2) */
  Vol -> FirstDataSector = Vol -> Bpb.BPB_RsvdSecCnt + (Vol -> Bpb.BPB_NumFATS *
    Vol -> Bpb.BPB_FATSz16) + RootDirSectors;

  return Vol;
}

/**
 * Browse directory entries in root directory.
 * ==================================================================================
 * Return
 * There is no return in this funcion.
 * ==================================================================================
 * Parameters
 * @fd: File descriptor.
 * @Vol: Structure that contains essential data about the File System (BPB, first
 * sector of data and root section).
 * @Root: Variable that will store directory entries in root.
 * @path: Path organized in an array of files names.
 * @pathSize: Number of files in the path.
 * @pathDepth: Depth, or index, o the current file of the path.
**/
void find_root(FILE *fd, VOLUME Vol, DIR_ENTRY Root, char ** path, int pathSize,
  int pathDepth) {
  /* Buffer to store bytes from sector_read */
  BYTE buffer[BYTES_PER_SECTOR];

  sector_read(fd, Vol.FirstRootDirSecNum, & buffer);

  int RootDirCnt = 1, i, j, cmpstring = 1;
  for (i = 1; i <= Vol.Bpb.BPB_RootEntCnt; i++) {
    memcpy( & Root, & buffer[((i - 1) * 32) % BYTES_PER_SECTOR], 32);

    /* If the directory entry is free, all the next directory entries are also
     * free. So this file/directory could not be found */
    if (Root.DIR_Name[0] == 0x00) {
      exit(0);
    }

    /* Comparing strings character by character */
    cmpstring = 1;
    for (j = 0; j < 11; j++) {
      if (Root.DIR_Name[j] != path[pathDepth][j]) {
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

    /* If the path is only one directory (ATTR_DIRECTORY) and it is located in
     * the root directory, stop searching */
    if (cmpstring && Root.DIR_Attr == 0x10 && pathSize == pathDepth + 1) {
      printf("Found the file %s in the root directory!\n", Root.DIR_Name);
      printDIR(Root);
      exit(0);
    }

    /* If the first level of the path is a directory, continue searching
     * in the root's sub-directories */
    if (cmpstring && Root.DIR_Attr == 0x10) {
      find_subdir(fd, Vol, Root, path, pathSize, pathDepth + 1, 1);
    }

    /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries)
     * Read next sector */
    if (i % 16 == 0 && i != Vol.Bpb.BPB_RootEntCnt) {
      sector_read(fd, Vol.FirstRootDirSecNum + RootDirCnt, & buffer);
      RootDirCnt++;
    }
  }
}

/**
 * Browse directory entries in a subdirectory.
 * ==================================================================================
 * Return
 * There is no return in this funcion.
 * ==================================================================================
 * Parameters
 * @fd: File descriptor.
 * @Vol: Structure that contains essential data about the File System (BPB, first
 * sector of data and root section).
 * @Dir: Variable that will store directory entries the subdirectory.
 * @path: Path organized in an array of files names.
 * @pathSize: Number of files in the path.
 * @pathDepth: Depth, or index, o the current file of the path.
 * @rootDepth: Depth to the root.
**/
void find_subdir(FILE *fd, VOLUME Vol, DIR_ENTRY Dir, char ** path, int pathSize,
  int pathDepth, int rootDepth) {
  if (rootDepth == 0) {
    find_root(fd, Vol, Dir, path, pathSize, pathDepth);
  }
  int i, j, DirSecCnt = 1, cmpstring;
  BYTE buffer[BYTES_PER_SECTOR];

  WORD ClusterN = Dir.DIR_FstClusLO;
  WORD FatClusEntryVal = fat_entry_by_cluster(fd, & Vol, ClusterN);

  /* First sector of any valid cluster */
  WORD FirstSectorofCluster = ((ClusterN - 2) *Vol.Bpb.BPB_SecPerClus) + Vol.FirstDataSector;

  sector_read(fd, FirstSectorofCluster, & buffer);
  for (i = 1; Dir.DIR_Name[0] != 0x00; i++) {
    memcpy( & Dir, & buffer[((i - 1) * 32) % BYTES_PER_SECTOR], 32);

    /* Comparing strings */
    cmpstring = 1;
    for (j = 0; j < 11; j++) {
      if (Dir.DIR_Name[j] != path[pathDepth][j]) {
        cmpstring = 0;
        break;
      }
    }

    /* If the last file of the path is located in this
     * directory stop searching */
    if ((cmpstring && Dir.DIR_Attr == 0x20 && pathDepth + 1 == pathSize) ||
      (cmpstring && Dir.DIR_Attr == 0x10 && pathDepth + 1 == pathSize)) {
      printf("Found the file %s in the %s directory!\n", Dir.DIR_Name,
        path[pathDepth - 1]);
      exit(0);
    }

    /* If the directory has been found and it isn't the last file */
    if (cmpstring && Dir.DIR_Attr == 0x10) {
      /* If the file is .., then the rootDepth decreases and search continues*/
      if (path[pathDepth][0] == '.' && path[pathDepth][1] == '.') {
        rootDepth--;
      /* If the file isn't ., then the root Depth increases. */
      } else if (path[pathDepth][0] != '.'){
        rootDepth++;
      }
      /* If it's the . file, then the root depth remains the same, anyways
       * the pathDepth increases by one and the function is called again in
       * recursion. */
      find_subdir(fd, Vol, Dir, path, pathSize, pathDepth + 1, rootDepth);
    }

    /* A sector needs to be readed 16 times by the buffer to reach the end. */
    if (i % 16 == 0) {
      printf("%d\n", i);
      /* If there are still sector to be readen in the cluster, read the next sector. */
      if (DirSecCnt < Vol.Bpb.BPB_SecPerClus) {
        sector_read(fd, FirstSectorofCluster + DirSecCnt, & buffer);
        DirSecCnt++;
      /* Reaches the end of the cluster */
      } else {
        /* Checks if there isn't a cluster to continue to read*/
        if (FatClusEntryVal == 0xffff) {
          printf("%s: file not found\n", path[pathDepth]);
          exit(0);
        /* If there is a cluster to continue reading */
        } else if (FatClusEntryVal >= 0x0002) {
          /* Update the cluster number */
          ClusterN = FatClusEntryVal;
          /* Update the fat entry */
          FatClusEntryVal = fat_entry_by_cluster(fd, & Vol, ClusterN);
          /* Calculates the first sector of the cluster */
          FirstSectorofCluster = ((ClusterN - 2) *Vol.Bpb.BPB_SecPerClus) + Vol.FirstDataSector;
          /* Read it, and then continue */
          sector_read(fd, FirstSectorofCluster, & buffer);
          i = 0;
          DirSecCnt = 1;
        }
      }
    }
  }
}

int main(int argc, char ** argv) {
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

  int pathSize;
  char ** path = path_treatment(argv[2], & pathSize);

  /* Initializing a FAT16 volume */
  VOLUME *Vol = fat16_init(fd);

  /* Root directory */
  DIR_ENTRY Root;

  /* Searching in the root directory first */
  find_root(fd, *Vol, Root, path, pathSize, 0);

  fclose(fd);
  return 0;
}
