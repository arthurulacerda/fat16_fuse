#include <errno.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "sector.h"
#include "log.h"

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
  BYTE Reserved2[448];
  WORD Signature_word;
} __attribute__ ((packed)) BPB_BS;

typedef struct {
  char DIR_Name[11];
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
  FILE *fd;
  DWORD FirstRootDirSecNum;
  DWORD FirstDataSector;
  BPB_BS Bpb;
} VOLUME;

DIR_ENTRY find_root(VOLUME, DIR_ENTRY, char**, int, int);
DIR_ENTRY find_subdir(VOLUME, DIR_ENTRY, char**, int, int, int);

void printDIR(DIR_ENTRY Dir) {
  int i;
  log_msg("Name: ");
  for (i = 0; i < 11; i++) {
    log_msg("%c", Dir.DIR_Name[i]);
  }
  log_msg("\n");
  log_msg("Attr: %x\n", Dir.DIR_Attr);
  log_msg("NTRes: %d\n", Dir.DIR_NTRes);
  log_msg("CrtTimeTenth: %d\n", Dir.DIR_CrtTimeTenth);
  log_msg("CrtTime: %d\n", Dir.DIR_CrtTime);
  log_msg("CrtDate: %d\n", Dir.DIR_CrtDate);
  log_msg("LstAccDate: %d\n", Dir.DIR_LstAccDate);
  log_msg("FstClusHI: %d\n", Dir.DIR_FstClusHI);
  log_msg("WrtTime: %d\n", Dir.DIR_WrtTime);
  log_msg("WrtDate: %d\n", Dir.DIR_WrtDate);
  log_msg("FstClusLO: %d\n", Dir.DIR_FstClusLO);
  log_msg("FileSize: %d\n\n", Dir.DIR_FileSize);
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
VOLUME *pre_init_fat16(void)
{
  /* Opening the FAT16 image file */
  FILE *fd = fopen("fat16.img", "rb");

  if (fd == NULL) {
    log_msg("Missing FAT16 image file!\n");
    exit(EXIT_FAILURE);
  }

  VOLUME *Vol = malloc(sizeof *Vol);

  if (Vol == NULL) {
    log_msg("Out of memory!\n");
    exit(EXIT_FAILURE);
  }

  Vol->fd = fd;

  /* Reads the BPB */
  sector_read(Vol->fd, 0, &Vol->Bpb);

  /* First sector of the root directory */
  Vol->FirstRootDirSecNum = Vol->Bpb.BPB_RsvdSecCnt
    + (Vol->Bpb.BPB_FATSz16 * Vol->Bpb.BPB_NumFATS);

  /* Number of sectors in the root directory */
  DWORD RootDirSectors = ((Vol->Bpb.BPB_RootEntCnt * 32) +
    (Vol->Bpb.BPB_BytsPerSec - 1)) / Vol->Bpb.BPB_BytsPerSec;

  /* First sector of the data region (cluster #2) */
  Vol->FirstDataSector = Vol->Bpb.BPB_RsvdSecCnt + (Vol->Bpb.BPB_NumFATS *
    Vol->Bpb.BPB_FATSz16) + RootDirSectors;

  return Vol;
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
//WORD fat_entry_by_cluster(FILE *fd, VOLUME *Vol, WORD ClusterN) {
//  BYTE FatBuffer[BYTES_PER_SECTOR];
//  WORD FATOffset = ClusterN * 2;
//  WORD FatSecNum = Vol -> Bpb.BPB_RsvdSecCnt + (FATOffset / Vol -> Bpb.BPB_BytsPerSec);
//  WORD FatEntOffset = FATOffset % Vol -> Bpb.BPB_BytsPerSec;
//  sector_read(fd, FatSecNum, & FatBuffer);
//  return *((WORD *) & FatBuffer[FatEntOffset]);
//}
//
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
            log_msg("Error: Empty extension after dot character (.) inf file %s\n", path[i]);
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
          log_msg("Error: More than one dot character (.) in file %s\n", path[i]);
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
          log_msg("Error: Overfill of name or extension field in file %s\n", path[i]);
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
          log_msg("Error: Overfill of name or extension field in file %s\n", path[i]);
          exit(1);
        }
      } else {
        log_msg("Error: Character not accepted in file %s\n", path[i]);
        exit(1);
      }
    }
  }

  *pathSz = pathSize;
  free(path);
  return pathFormatted;
}

char *path_decode(char *path) {
  char *pathDecoded = malloc(12 * sizeof(char));
  int i, j;
  for (i = 0, j = 0; path[i] != '\0'; i++) {
    if (path[i] != ' ') {
      if (i != 8) {
        pathDecoded[j++] = path[i] + 32;
      } else {
        pathDecoded[j++] = '.';
        pathDecoded[j++] = path[i] + 32;
      }
    }
  }
  pathDecoded[j] = '\0';
  return pathDecoded;
}

/**
 * Browse directory entries in root directory.
 * ==================================================================================
 * Return
 * There is no return in this funcion.
 * ==================================================================================
 * Parameters
 * @Vol: Structure that contains essential data about the File System (BPB, first
 * sector of data and root section).
 * @Root: Variable that will store directory entries in root.
 * @path: Path organized in an array of files names.
 * @pathSize: Number of files in the path.
 * @pathDepth: Depth, or index, o the current file of the path.
**/
//DIR_ENTRY *find_root(VOLUME Vol, DIR_ENTRY Root, char ** path, int pathSize, int pathDepth)
//{
//  /* Buffer to store bytes from sector_read */
//  BYTE buffer[BYTES_PER_SECTOR];
//
//  sector_read(Vol.fd, Vol.FirstRootDirSecNum, & buffer);
//
//  int RootDirCnt = 1, i, j, cmpstring = 1;
//  for (i = 1; i <= Vol.Bpb.BPB_RootEntCnt; i++) {
//    memcpy( & Root, & buffer[((i - 1) * 32) % BYTES_PER_SECTOR], 32);
//
//    /* If the directory entry is free, all the next directory entries are also
//     * free. So this file/directory could not be found */
//    if (Root.DIR_Name[0] == 0x00) {
//      exit(0);
//    }
//
//    /* Comparing strings character by character */
//    cmpstring = 1;
//    for (j = 0; j < 11; j++) {
//      if (Root.DIR_Name[j] != path[pathDepth][j]) {
//        cmpstring = 0;
//        break;
//      }
//    }
//
//    /* If the path is only one file (ATTR_ARCHIVE) and it is located in the
//     * root directory, stop searching */
//    if (cmpstring && Root.DIR_Attr == 0x20) {
//      log_msg("Found the file %s in the root directory!\n", Root.DIR_Name);
//      return Root;
//      //printDIR(Root);
//      //exit(0);
//    }
//
//    /* If the path is only one directory (ATTR_DIRECTORY) and it is located in
//     * the root directory, stop searching */
//    if (cmpstring && Root.DIR_Attr == 0x10 && pathSize == pathDepth + 1) {
//      log_msg("Found the file %s in the root directory!\n", Root.DIR_Name);
//      return Root;
//      //printDIR(Root);
//      //exit(0);
//    }
//
//    /* If the first level of the path is a directory, continue searching
//     * in the root's sub-directories */
//    if (cmpstring && Root.DIR_Attr == 0x10) {
//      find_subdir(Vol, Root, path, pathSize, pathDepth + 1, 1);
//    }
//
//    /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries)
//     * Read next sector */
//    if (i % 16 == 0 && i != Vol.Bpb.BPB_RootEntCnt) {
//      sector_read(Vol.fd, Vol.FirstRootDirSecNum + RootDirCnt, & buffer);
//      RootDirCnt++;
//    }
//  }
//}
//
///**
// * Browse directory entries in a subdirectory.
// * ==================================================================================
// * Return
// * There is no return in this funcion.
// * ==================================================================================
// * Parameters
// * @Vol: Structure that contains essential data about the File System (BPB, first
// * sector of data and root section).
// * @Dir: Variable that will store directory entries the subdirectory.
// * @path: Path organized in an array of files names.
// * @pathSize: Number of files in the path.
// * @pathDepth: Depth, or index, o the current file of the path.
// * @rootDepth: Depth to the root.
//**/
//DIR_ENTRY find_subdir(VOLUME Vol, DIR_ENTRY Dir, char ** path, int pathSize,
//  int pathDepth, int rootDepth) {
//  if (rootDepth == 0) {
//    find_root(Vol, Dir, path, pathSize, pathDepth);
//  }
//  int i, j, DirSecCnt = 1, cmpstring;
//  BYTE buffer[BYTES_PER_SECTOR];
//
//  WORD ClusterN = Dir.DIR_FstClusLO;
//  WORD FatClusEntryVal = fat_entry_by_cluster(Vol.fd, & Vol, ClusterN);
//
//  /* First sector of any valid cluster */
//  WORD FirstSectorofCluster = ((ClusterN - 2) *Vol.Bpb.BPB_SecPerClus) + Vol.FirstDataSector;
//
//  sector_read(Vol.fd, FirstSectorofCluster, & buffer);
//  for (i = 1; Dir.DIR_Name[0] != 0x00; i++) {
//    memcpy( & Dir, & buffer[((i - 1) * 32) % BYTES_PER_SECTOR], 32);
//
//    /* Comparing strings */
//    cmpstring = 1;
//    for (j = 0; j < 11; j++) {
//      if (Dir.DIR_Name[j] != path[pathDepth][j]) {
//        cmpstring = 0;
//        break;
//      }
//    }
//
//    /* If the last file of the path is located in this
//     * directory stop searching */
//    if ((cmpstring && Dir.DIR_Attr == 0x20 && pathDepth + 1 == pathSize) ||
//        (cmpstring && Dir.DIR_Attr == 0x10 && pathDepth + 1 == pathSize)) {
//      log_msg("Found the file %s in the %s directory!\n", Dir.DIR_Name,
//              path[pathDepth - 1]);
//      return Dir;
//    }
//
//    /* If the directory has been found and it isn't the last file */
//    if (cmpstring && Dir.DIR_Attr == 0x10) {
//
//      /* If the file is .., then the rootDepth decreases and search continues*/
//      if (path[pathDepth][0] == '.' && path[pathDepth][1] == '.') {
//        rootDepth--;
//
//      /* If the file isn't ., then the root Depth increases. */
//      } else if (path[pathDepth][0] != '.'){
//        rootDepth++;
//      }
//      /* If it's the . file, then the root depth remains the same, anyways
//       * the pathDepth increases by one and the function is called again in
//       * recursion. */
//      find_subdir(Vol, Dir, path, pathSize, pathDepth + 1, rootDepth);
//    }
//
//    /* A sector needs to be readed 16 times by the buffer to reach the end. */
//    if (i % 16 == 0) {
//
//      /* If there are still sector to be readen in the cluster, read the next sector. */
//      if (DirSecCnt < Vol.Bpb.BPB_SecPerClus) {
//        sector_read(Vol.fd, FirstSectorofCluster + DirSecCnt, & buffer);
//        DirSecCnt++;
//      /* Reaches the end of the cluster */
//      } else {
//        /* Checks if there isn't a cluster to continue to read*/
//        if (FatClusEntryVal == 0xffff) {
//          log_msg("%s: file not found\n", path[pathDepth]);
//          exit(EXIT_FAILURE);
//
//        /* If there is a cluster to continue reading */
//        } else if (FatClusEntryVal >= 0x0002) {
//          /* Update the cluster number */
//          ClusterN = FatClusEntryVal;
//
//          /* Update the fat entry */
//          FatClusEntryVal = fat_entry_by_cluster(Vol.fd, & Vol, ClusterN);
//
//          /* Calculates the first sector of the cluster */
//          FirstSectorofCluster = ((ClusterN - 2) *Vol.Bpb.BPB_SecPerClus) + Vol.FirstDataSector;
//
//          /* Read it, and then continue */
//          sector_read(Vol.fd, FirstSectorofCluster, & buffer);
//          i = 0;
//          DirSecCnt = 1;
//        }
//      }
//    }
//  }
//}

DIR_ENTRY *readdir(VOLUME Vol)
{
  /* Buffer to store bytes from sector_read */
  BYTE buffer[BYTES_PER_SECTOR];

  DIR_ENTRY *Root;

  sector_read(Vol.fd, Vol.FirstRootDirSecNum, & buffer);

  int RootDirCnt = 1, i, j, cmpstring = 1;
  for (i = 1; i <= Vol.Bpb.BPB_RootEntCnt; i++) {
    memcpy(Root, & buffer[((i - 1) * 32) % BYTES_PER_SECTOR], 32);

    if (Root->DIR_Name[0] == 0x00) {
      return NULL;
    }

    if (Root->DIR_Attr == 0x20) {
      //printDIR(*Root);
      return Root;
    }

    if (Root->DIR_Attr == 0x10) {
      //printDIR(*Root);
      return Root;
    }

    /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries)
     * Read next sector */
    if (i % 16 == 0 && i != Vol.Bpb.BPB_RootEntCnt) {
      sector_read(Vol.fd, Vol.FirstRootDirSecNum + RootDirCnt, & buffer);
      RootDirCnt++;
    }
  }
  return NULL;
}

//------------------------------------------------------------------------------

void *fat16_init(struct fuse_conn_info *conn)
{
  log_msg("Chamando init\n");

  // Your code here 
  struct fuse_context *context;
  context = fuse_get_context();

  return context->private_data;
}

void fat16_destroy(void *data)
{
  log_msg("Chamando destroy\n");

  // Your code here
}

int fat16_getattr(const char *path, struct stat *stbuf)
{
  log_msg("calling getattr: %s\n", path);

  VOLUME *Vol;
  DIR_ENTRY Root;

  struct fuse_context *context;
  context = fuse_get_context();
  Vol = (VOLUME *) context->private_data;

  //TODO: set attributes
  memset(stbuf, 0, sizeof(struct stat));
  stbuf->st_dev = Vol->Bpb.BS_VollID;
  stbuf->st_nlink = 1;
  stbuf->st_uid = 0;
  stbuf->st_gid = 0;
  stbuf->st_rdev = 0;
  stbuf->st_blksize = Vol->Bpb.BPB_SecPerClus * BYTES_PER_SECTOR;

  /* Root directory */
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | S_IRWXU;
    stbuf->st_size = 0;
    stbuf->st_blocks = 0;
    stbuf->st_ctime = stbuf->st_atime = stbuf->st_mtime = 0;
  } else {
    DIR_ENTRY *file = readdir(*Vol);

    if (file != NULL) {
      if (file->DIR_Attr == 0x10) {
        stbuf->st_mode = S_IFDIR | 0755;
      } else {
        stbuf->st_mode = S_IFREG | 0444;
      }
      stbuf->st_size = file->DIR_FileSize;
      stbuf->st_blocks = (stbuf->st_size / stbuf->st_blksize);
    }
  }
  return 0;
}

int fat16_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi)
{
  log_msg("calling readdir: %s\n", path);
  (void) offset;
  (void) fi;
  VOLUME *Vol;
  DIR_ENTRY Root;

  struct fuse_context *context;
  context = fuse_get_context();
  Vol = (VOLUME *) context->private_data;

  /* Buffer to store bytes from sector_read */
  BYTE fatbuffer[BYTES_PER_SECTOR];

  sector_read(Vol->fd, Vol->FirstRootDirSecNum, &fatbuffer);

  int RootDirCnt = 1, i;
  for (i = 1; i <= Vol->Bpb.BPB_RootEntCnt; i++) {
    memcpy(&Root, &fatbuffer[((i - 1) * 32) % BYTES_PER_SECTOR], 32);

    if (Root.DIR_Name[0] == 0x00) {
      return 0;
    }

    if (Root.DIR_Attr == 0x20) {
      const char *filename = strdup(path_decode(Root.DIR_Name));
      filler(buffer, filename, NULL, 0);
    }

    if (Root.DIR_Attr == 0x10) {
      const char *filename = strdup(path_decode(Root.DIR_Name));
      filler(buffer, filename, NULL, 0);
    }

    /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries)
     * Read next sector */
    if (i % 16 == 0 && i != Vol->Bpb.BPB_RootEntCnt) {
      sector_read(Vol->fd, Vol->FirstRootDirSecNum + RootDirCnt, &fatbuffer);
      RootDirCnt++;
    }
  }

  return 0;
}

int fat16_open(const char *path, struct fuse_file_info *fi)
{
  int i;
  log_msg("calling open: %s", path);
  return 0;
}

int fat16_read(const char *path, char *buffer, size_t size, off_t offset,
               struct fuse_file_info *fi)
{
  int i;
  log_msg("calling read: %s", path);
  return 0;
}

//------------------------------------------------------------------------------

struct fuse_operations fat16_oper = {
  .init       = fat16_init,
  .destroy    = fat16_destroy,
  .getattr    = fat16_getattr,
  .readdir    = fat16_readdir,
  .open       = fat16_open,
  .read       = fat16_read
};

//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
  int ret;

  log_open();

  /* Starting pre-initialization of a FAT16 volume */
  VOLUME *Vol = pre_init_fat16();

  ret = fuse_main(argc, argv, &fat16_oper, Vol);

  log_msg("ret: %d\n", ret);

  return ret;
}
