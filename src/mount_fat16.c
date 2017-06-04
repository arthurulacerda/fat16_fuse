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

#define BYTES_PER_DIR 32
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

/* FAT16 BPB Structure */
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

/* FAT Directory Structure */
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

/* FAT16 volume data with a file handler of the FAT16 image file */
typedef struct {
  FILE *fd;
  DWORD FirstRootDirSecNum;
  DWORD FirstDataSector;
  BPB_BS Bpb;
} VOLUME;

/* Prototypes (documentation in the functions definitions) */
int find_root(VOLUME, DIR_ENTRY *Root, char **path, int pathSize, int pathDepth);
int find_subdir(VOLUME, DIR_ENTRY *Dir, char **path, int pathSize, int pathDepth);
char **path_treatment(char *pathInput, int *pathSz);
VOLUME *pre_init_fat16(void);
WORD fat_entry_by_cluster(VOLUME Vol, WORD ClusterN);
BYTE *path_decode(BYTE *);

void *fat16_init(struct fuse_conn_info *conn);
void fat16_destroy(void *data);
int fat16_getattr(const char *path, struct stat *stbuf);
int fat16_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi);
int fat16_open(const char *path, struct fuse_file_info *fi);
int fat16_read(const char *path, char *buffer, size_t size, off_t offset,
               struct fuse_file_info *fi);

/**
 * Reads BPB, calculates the first sector of the root and data sections.
 * ============================================================================
 * Return
 * @Vol: Structure that contains essential data about the File System (BPB,
 * first sector number of the Data Region, number of sectors in the root
 * directory and the first sector number of the Root Directory Region).
* =============================================================================
**/
VOLUME *pre_init_fat16(void)
{
  /* Opening the FAT16 image file */
  FILE *fd = fopen("fat16.img", "rb");

  if (fd == NULL) {
    log_msg("Missing FAT16 image file!\n");
    exit(EXIT_FAILURE);
  }

  VOLUME *Vol = malloc(sizeof(VOLUME));

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
 * Given a cluster N, this function gets its FAT entry.
 * ============================================================================
 * Return
 * The entry in the FAT for the cluster N
 * ============================================================================
 * Parameters
 * @Vol: Structure that contains essential data about the File System (BPB,
 * first sector number of the Data Region, number of sectors in the root
 * directory and the first sector number of the Root Directory Region).
 * @CusterN: the Nth cluster of the data section.
**/
WORD fat_entry_by_cluster(VOLUME Vol, WORD ClusterN) {
  /* Buffer to store bytes from the image file and the FAT16 offset */
  BYTE sector_buffer[BYTES_PER_SECTOR];
  WORD FATOffset = ClusterN * 2;

  /* FatSecNum is the sector number of the FAT sector that contains the entry
   * for cluster N in the first FAT */
  WORD FatSecNum = Vol.Bpb.BPB_RsvdSecCnt + (FATOffset / Vol.Bpb.BPB_BytsPerSec);
  WORD FatEntOffset = FATOffset % Vol.Bpb.BPB_BytsPerSec;

  /* Reads the sector and extract the FAT entry contained on it */
  sector_read(Vol.fd, FatSecNum, &sector_buffer);
  return *((WORD *) &sector_buffer[FatEntOffset]);
}

/**
 * This function receieves the string given by input and divides it into an
 * array of strings, with the format of a FAT file/directory name.
 * ============================================================================
 * Return
 * @pathFormatted: array of string, each string references a file of the path,
 * in a given depth. It also presents the format of FAT file names.
 * ============================================================================
 * Parameters
 * @pathInput: User input string, the path of files to go trough.
 * @pathSz: Address of the variable that will keep the number of files in the
 * path.
**/
char **path_treatment(char *pathInput, int *pathSz) {
  int pathSize = 1;
  int i, j;

  /* Counting number of files */
  for (i = 2; pathInput[i + 1] != '\0'; i++) {
    if (pathInput[i] == '/') {
      pathSize++;
    }
  }

  char **path = malloc(pathSize * sizeof(char *));

  if (path == NULL) {
    log_msg("Out of memory!\n");
    exit(EXIT_FAILURE);
  }

  const char token[] = "/";
  char *slice;

  i = 0;

  /* Dividing the path into separated strings of file names */
  slice = strtok(pathInput, token);
  while (i < pathSize) {
    path[i++] = slice;
    slice = strtok(NULL, token);
  }

  char **pathFormatted = malloc(pathSize * sizeof(char *));

  if (pathFormatted == NULL) {
    log_msg("Out of memory!\n");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < pathSize; i++) {
    pathFormatted[i] = malloc(11 * sizeof(char));

    if (pathFormatted[i] == NULL) {
      log_msg("Out of memory!\n");
      exit(EXIT_FAILURE);
    }
  }

  int k;
  int dotFlag = 0;

  /* Verifies if each file of the path is a valid input, and then formats it */
  for (i = 0; i < pathSize; i++) {
    for (j = 0, k = 0; ; j++, k++) {

      /* Here, a '.' (dot) character is analysed */
      if (path[i][j] == '.') {

        /* Verifies if it is a "./" */
        if (j == 0 && path[i][j + 1] == '\0') {
          pathFormatted[i][0] = '.';

          for (k = 1; k < 11; k++) {
            pathFormatted[i][k] = ' ';
          }
          break;
        }

        /* Verifies if it's a "../" */
        if (j == 0 && path[i][j + 1] == '.' && path[i][j + 2] == '\0') {
          pathFormatted[i][0] = '.';
          pathFormatted[i][1] = '.';

          for (k = 2; k < 11; k++) {
            pathFormatted[i][k] = ' ';
          }
          break;
        }

        /* Check if there wasn't any other past occurrence of the '.' character */
        if (!dotFlag) {
          /* Marks the occurrence of the '.' character */
          dotFlag = 1;

          /* Fills with space ' ' character the name field leftover */
          for (; k < 8; k++) {
            pathFormatted[i][k] = ' ';
          }
          k = 7;
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
      }
      /* Other character accepted in the file name */
      else if ((path[i][j] >= 'A' && path[i][j] <= 'Z') || (path[i][j] >= '0' &&
                path[i][j] <= '9') || path[i][j] == '$' || path[i][j] == '%' ||
                path[i][j] == '\'' || path[i][j] == '-' || path[i][j] == '_' ||
                path[i][j] == '@' || path[i][j] == '~' || path[i][j] == '`' ||
                path[i][j] == '!' || path[i][j] == '(' || path[i][j] == ')' ||
                path[i][j] == '{' || path[i][j] == '}' || path[i][j] == '^' ||
                path[i][j] == '#' || path[i][j] == '&') {
        pathFormatted[i][k] = path[i][j];
      }
    }
    pathFormatted[i][11] = '\0';
  }

  *pathSz = pathSize;
  free(path);
  return pathFormatted;
}

/**
 * This function receieves a FAT file/directory name (DIR_Name) and decodes it
 * to its original user input name.
 * ==================================================================================
 * Return
 * @pathDecoded: FAT name decoded to its original input name
 * ==================================================================================
 * Parameters
 * @path: DIR_Name string
**/
BYTE *path_decode(BYTE *path) {
  int i, j = 0;
  BYTE *pathDecoded = malloc(11 * sizeof(BYTE));

  if (pathDecoded == NULL) {
    log_msg("Out of memory!\n");
    exit(EXIT_FAILURE);
  }

  /* If the name consists of "./" or "../", return them as the decoded path */
  if (path[0] == '.' && path[1] == '.') {
    pathDecoded[j++] = '.';
    pathDecoded[j++] = '.';
    pathDecoded[j] = '\0';
    return pathDecoded;
  }
  if (path[0] == '.') {
    pathDecoded[j++] = '.';
    pathDecoded[j] = '\0';
    return pathDecoded;
  }

  /* Decoding from uppercase letters to lowercase letters, removing spaces,
   * inserting 'dots' in between them and verifying if they are legal */
  for (i = 0; i < 11; i++) {
    if (path[i] != ' ') {
      if (i != 8) {
        if ((path[i] >= '0' && path[i] <= '9') || path[i] == '$' ||
            path[i] == '%' || path[i] == '\'' || path[i] == '-' ||
            path[i] == '_' || path[i] == '@' || path[i] == '~' ||
            path[i] == '`' || path[i] == '!' || path[i] == '(' ||
            path[i] == ')' || path[i] == '{' || path[i] == '}' ||
            path[i] == '^' || path[i] == '#' || path[i] == '&') {
          pathDecoded[j++] = path[i];
        } else {
          pathDecoded[j++] = path[i] + 32;
        }
      } else {
        pathDecoded[j++] = '.';
        if ((path[i] >= '0' && path[i] <= '9') || path[i] == '$' ||
            path[i] == '%' || path[i] == '\'' || path[i] == '-' ||
            path[i] == '_' || path[i] == '@' || path[i] == '~' ||
            path[i] == '`' || path[i] == '!' || path[i] == '(' ||
            path[i] == ')' || path[i] == '{' || path[i] == '}' ||
            path[i] == '^' || path[i] == '#' || path[i] == '&') {
          pathDecoded[j++] = path[i];
        } else {
          pathDecoded[j++] = path[i] + 32;
        }
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
 * 0, if we did find a file corresponding to the given path or 1 if we did not
 * ==================================================================================
 * Parameters
 * @Vol: Structure that contains essential data about the File System (BPB, first
 * sector number of the Data Region, number of sectors in the root directory and the
 * first sector number of the Root Directory Region).
 * @Root: Variable that will store directory entries in root.
 * @path: Path organized in an array of files names.
 * @pathSize: Number of files in the path.
 * @pathDepth: Depth, or index, o the current file of the path.
**/
int find_root(VOLUME Vol, DIR_ENTRY *Root, char **path, int pathSize, int pathDepth)
{
  int i, j;
  int RootDirCnt = 1, cmpstring = 1;
  BYTE buffer[BYTES_PER_SECTOR];

  sector_read(Vol.fd, Vol.FirstRootDirSecNum, buffer);

  /* We search for the path in the root directory first */
  for (i = 1; i <= Vol.Bpb.BPB_RootEntCnt; i++) {
    memcpy(Root, &buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);

    /* If the directory entry is free, all the next directory entries are also
     * free. So this file/directory could not be found */
    if (Root->DIR_Name[0] == 0x00) {
      return 1;
    }

    /* Comparing strings character by character */
    cmpstring = 1;
    for (j = 0; j < 11; j++) {
      if (Root->DIR_Name[j] != path[pathDepth][j]) {
        cmpstring = 0;
        break;
      }
    }

    /* If the path is only one file (ATTR_ARCHIVE) and it is located in the
     * root directory, stop searching */
    if (cmpstring && Root->DIR_Attr == ATTR_ARCHIVE) {
      return 0;
    }

    /* If the path is only one directory (ATTR_DIRECTORY) and it is located in
     * the root directory, stop searching */
    if (cmpstring && Root->DIR_Attr == ATTR_DIRECTORY && pathSize == pathDepth + 1) {
      return 0;
    }

    /* If the first level of the path is a directory, continue searching
     * in the root's sub-directories */
    if (cmpstring && Root->DIR_Attr == ATTR_DIRECTORY) {
      return find_subdir(Vol, Root, path, pathSize, pathDepth + 1);
    }

    /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries)
     * Read next sector */
    if (i % 16 == 0 && i != Vol.Bpb.BPB_RootEntCnt) {
      sector_read(Vol.fd, Vol.FirstRootDirSecNum + RootDirCnt, buffer);
      RootDirCnt++;
    }
  }

  /* We did not find anything */
  return 1;
}

/**
 * Browse directory entries in a subdirectory.
 * ==================================================================================
 * Return
 * There is no return in this funcion.
 * ==================================================================================
 * Parameters
 * @Vol: Structure that contains essential data about the File System (BPB, first
 * sector number of the Data Region, number of sectors in the root directory and the
 * first sector number of the Root Directory Region).
 * @Dir: Variable that will store directory entries the subdirectory.
 * @path: Path organized in an array of files names.
 * @pathSize: Number of files in the path.
 * @pathDepth: Depth, or index, o the current file of the path.
**/
int find_subdir(VOLUME Vol, DIR_ENTRY *Dir, char **path, int pathSize, int pathDepth)
{
  int i, j, DirSecCnt = 1, cmpstring;
  BYTE buffer[BYTES_PER_SECTOR];

  /* Calculating the first cluster sector for the given path */
  WORD ClusterN = Dir->DIR_FstClusLO;
  WORD FatClusEntryVal = fat_entry_by_cluster(Vol, ClusterN);
  WORD FirstSectorofCluster = ((ClusterN - 2) *Vol.Bpb.BPB_SecPerClus) + Vol.FirstDataSector;

  sector_read(Vol.fd, FirstSectorofCluster, buffer);

  /* Searching for the given path in all directory entries of Dir */
  for (i = 1; Dir->DIR_Name[0] != 0x00; i++) {
    memcpy(Dir, &buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);

    /* Comparing strings */
    cmpstring = 1;
    for (j = 0; j < 11; j++) {
      if (Dir->DIR_Name[j] != path[pathDepth][j]) {
        cmpstring = 0;
        break;
      }
    }

    /* Stop searching if the last file of the path is located in this
     * directory */
    if ((cmpstring && Dir->DIR_Attr == ATTR_ARCHIVE && pathDepth + 1 == pathSize) ||
        (cmpstring && Dir->DIR_Attr == ATTR_DIRECTORY && pathDepth + 1 == pathSize)) {
      return 0;
    }

    /* Recursively keep searching if the directory has been found and it isn't
     * the last file */
    if (cmpstring && Dir->DIR_Attr == ATTR_DIRECTORY) {
      return find_subdir(Vol, Dir, path, pathSize, pathDepth + 1);
    }

    /* A sector needs to be readed 16 times by the buffer to reach the end. */
    if (i % 16 == 0) {
      /* If there are still sector to be read in the cluster, read the next sector. */
      if (DirSecCnt < Vol.Bpb.BPB_SecPerClus) {
        sector_read(Vol.fd, FirstSectorofCluster + DirSecCnt, buffer);
        DirSecCnt++;
      } else { /* Reaches the end of the cluster */

        /* Not strictly necessary, but here we reach the end of the clusters of
         * this directory entry. */
        if (FatClusEntryVal == 0xffff) {
          return 1;
        }

        /* Next cluster */
        ClusterN = FatClusEntryVal;

        /* Updates the fat entry for the above cluster */
        FatClusEntryVal = fat_entry_by_cluster(Vol, ClusterN);

        /* Calculates the first sector of the cluster */
        FirstSectorofCluster = ((ClusterN - 2) * Vol.Bpb.BPB_SecPerClus) + Vol.FirstDataSector;

        /* Read it, and then continue */
        sector_read(Vol.fd, FirstSectorofCluster, buffer);
        i = 0;
        DirSecCnt = 1;
      }
    }
  }

  /* We did not find the given path */
  return 1;
}

//------------------------------------------------------------------------------

void *fat16_init(struct fuse_conn_info *conn)
{
  struct fuse_context *context;
  context = fuse_get_context();

  return context->private_data;
}

void fat16_destroy(void *data)
{
  free(data);
}

int fat16_getattr(const char *path, struct stat *stbuf)
{
  VOLUME *Vol;

  /* Gets volume data supplied in the context during the fat16_init function */
  struct fuse_context *context;
  context = fuse_get_context();
  Vol = (VOLUME *) context->private_data;

  /* stbuf: setting file/directory attributes */
  memset(stbuf, 0, sizeof(struct stat));
  stbuf->st_dev = Vol->Bpb.BS_VollID;
  stbuf->st_blksize = BYTES_PER_SECTOR * Vol->Bpb.BPB_SecPerClus;
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();

  if (strcmp(path, "/") == 0) {

    /* Root directory attributes */
    stbuf->st_mode = S_IFDIR | S_IRWXU;
    stbuf->st_size = 0;
    stbuf->st_blocks = 0;
    stbuf->st_ctime = stbuf->st_atime = stbuf->st_mtime = 0;
  } else {

    /* File/Directory attributes */
    DIR_ENTRY Dir;
    int pathSize;
    char **pathFormatted = path_treatment((char *) path, &pathSize);
    int res = find_root(*Vol, &Dir, pathFormatted, pathSize, 0);

    if (res == 0) {

      /* FAT-like permissions */
      if (Dir.DIR_Attr == ATTR_DIRECTORY) {
        stbuf->st_mode = S_IFDIR | 0755;
      } else {
        stbuf->st_mode = S_IFREG | 0755;
      }
      stbuf->st_size = Dir.DIR_FileSize;

      /* Number of blocks */
      if (stbuf->st_size % stbuf->st_blksize != 0) {
        stbuf->st_blocks = (int) (stbuf->st_size / stbuf->st_blksize) + 1;
      } else {
        stbuf->st_blocks = (int) (stbuf->st_size / stbuf->st_blksize);
      }

      /* Implementing the required FAT Date/Time attributes */
      struct tm t;
      memset((char *) &t, 0, sizeof(struct tm));
      t.tm_sec = Dir.DIR_WrtTime & ((1 << 5) - 1);
      t.tm_min = (Dir.DIR_WrtTime >> 5) & ((1 << 6) - 1);
      t.tm_hour = Dir.DIR_WrtTime >> 11;
      t.tm_mday = (Dir.DIR_WrtDate & ((1 << 5) - 1));
      t.tm_mon = (Dir.DIR_WrtDate >> 5) & ((1 << 4) - 1);
      t.tm_year = 80 + (Dir.DIR_WrtDate >> 9);
      stbuf->st_ctime = stbuf->st_atime = stbuf->st_mtime = mktime(&t);
    }
  }
  return 0;
}

int fat16_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi)
{
  VOLUME *Vol;
  BYTE sector_buffer[BYTES_PER_SECTOR];
  int RootDirCnt = 1, DirSecCnt = 1, i;

  /* Gets volume data supplied in the context during the fat16_init function */
  struct fuse_context *context;
  context = fuse_get_context();
  Vol = (VOLUME *) context->private_data;

  sector_read(Vol->fd, Vol->FirstRootDirSecNum, sector_buffer);

  if (strcmp(path, "/") == 0) {
    DIR_ENTRY Root;

    /* Starts filling the requested directory entries into the buffer */
    for (i = 1; i <= Vol->Bpb.BPB_RootEntCnt; i++) {
      memcpy(&Root, &sector_buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);

      /* No more files to fill */
      if (Root.DIR_Name[0] == 0x00) {
        return 0;
      }

      /* If we find a file or a directory, fill it into the buffer */
      if (Root.DIR_Attr == ATTR_ARCHIVE || Root.DIR_Attr == ATTR_DIRECTORY) {
        const char *filename = (const char *) path_decode(Root.DIR_Name);
        filler(buffer, filename, NULL, 0);
      }

      /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries)
       * Read next sector */
      if (i % 16 == 0 && i != Vol->Bpb.BPB_RootEntCnt) {
        sector_read(Vol->fd, Vol->FirstRootDirSecNum + RootDirCnt, sector_buffer);
        RootDirCnt++;
      }
    }
  } else {
    DIR_ENTRY Dir;
    int pathSize;

    /* Formats the given path into the FAT format name */
    char **pathFormatted = path_treatment((char *) path, &pathSize);

    /* Finds the first corresponding directory entry in the root directory and
     * store the result in the directory entry Dir */
    find_root(*Vol, &Dir, pathFormatted, pathSize, 0);

    /* Calculating the first cluster sector for the given path */
    WORD ClusterN = Dir.DIR_FstClusLO;
    WORD FatClusEntryVal = fat_entry_by_cluster(*Vol, ClusterN);
    WORD FirstSectorofCluster = ((ClusterN - 2) * Vol->Bpb.BPB_SecPerClus) + Vol->FirstDataSector;

    sector_read(Vol->fd, FirstSectorofCluster, sector_buffer);

    /* Start searching the root's sub-directories starting from Dir */
    for (i = 1; Dir.DIR_Name[0] != 0x00; i++) {
      memcpy(&Dir, &sector_buffer[((i - 1) * BYTES_PER_DIR) % BYTES_PER_SECTOR], BYTES_PER_DIR);

      /* If the last file of the path is located in this directory, stop
       * searching */
      if (Dir.DIR_Attr == ATTR_ARCHIVE || Dir.DIR_Attr == ATTR_DIRECTORY) {
        const char *filename = (const char *) path_decode(Dir.DIR_Name);
        filler(buffer, filename, NULL, 0);
      }

      /* End of bytes for this sector (1 sector == 512 bytes == 16 DIR entries) */
      if (i % 16 == 0) {

        /* If there are still sector to be read in the cluster, read the next sector. */
        if (DirSecCnt < Vol->Bpb.BPB_SecPerClus) {
          sector_read(Vol->fd, FirstSectorofCluster + DirSecCnt, sector_buffer);
          DirSecCnt++;

        /* Else, read the next sector */
        } else {

          /* Not strictly necessary, but here we reach the end of the clusters
           * of this directory entry. */
          if (FatClusEntryVal == 0xffff) {
            return 0;
          }

          /* Next cluster */
          ClusterN = FatClusEntryVal;

          /* Updates its fat entry */
          FatClusEntryVal = fat_entry_by_cluster(*Vol, ClusterN);

          /* Calculates its first sector */
          FirstSectorofCluster = ((ClusterN - 2) * Vol->Bpb.BPB_SecPerClus) + Vol->FirstDataSector;

          /* Reads it, and then continue */
          sector_read(Vol->fd, FirstSectorofCluster, sector_buffer);
          i = 0;
          DirSecCnt = 1;
        }
      }
    }
  }

  /* No more files */
  return 0;
}

int fat16_read(const char *path, char *buffer, size_t size, off_t offset,
               struct fuse_file_info *fi)
{
  int i, j;
  BYTE *sector_buffer = malloc((size + offset) * sizeof(BYTE));

  if (sector_buffer == NULL) {
    log_msg("Out of memory!\n");
    exit(EXIT_FAILURE);
  }

  /* Gets volume data supplied in the context during the fat16_init function */
  VOLUME *Vol;
  struct fuse_context *context;
  context = fuse_get_context();
  Vol = (VOLUME *) context->private_data;

  /* Searches for the given path */
  DIR_ENTRY Dir;
  int pathSize;
  char **pathFormatted = path_treatment((char *) path, &pathSize);
  find_root(*Vol, &Dir, pathFormatted, pathSize, 0);

  /* Found it, so we calculate its first cluster location */
  WORD ClusterN = Dir.DIR_FstClusLO;
  WORD FatClusEntryVal = fat_entry_by_cluster(*Vol, ClusterN);
  WORD FirstSectorofCluster = ((ClusterN - 2) * Vol->Bpb.BPB_SecPerClus) + Vol->FirstDataSector;

  /* Read bytes from the given path into the buffer */
  for (i = 0, j = 0; i < size + offset; i += BYTES_PER_SECTOR, j++) {
    sector_read(Vol->fd, FirstSectorofCluster + j, sector_buffer + i);

    /* End of cluster, fetches the next one */
    if ((j + 1) % Vol->Bpb.BPB_SecPerClus == 0) {

      /* Updates the cluster number */
      ClusterN = FatClusEntryVal;

      /* Updates its fat entry */
      FatClusEntryVal = fat_entry_by_cluster(*Vol, ClusterN);

      /* Calculates its first sector */
      FirstSectorofCluster = ((ClusterN - 2) * Vol->Bpb.BPB_SecPerClus) + Vol->FirstDataSector;

      j = -1;
    }
  }

  /* Size is exactly the number of bytes requested or 0 if offset was at or
   * beyond the end of file */
  if (offset < Dir.DIR_FileSize) {
    memcpy(buffer, sector_buffer + offset, size);
  } else {
    size = 0;
  }

  free(sector_buffer);
  return size;
}

//------------------------------------------------------------------------------

struct fuse_operations fat16_oper = {
  .init       = fat16_init,
  .destroy    = fat16_destroy,
  .getattr    = fat16_getattr,
  .readdir    = fat16_readdir,
  .read       = fat16_read
};

//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
  int ret;

  log_open();

  /* Starting a pre-initialization of the FAT16 volume */
  VOLUME *Vol = pre_init_fat16();

  ret = fuse_main(argc, argv, &fat16_oper, Vol);

  return ret;
}
