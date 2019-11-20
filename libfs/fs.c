#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

int findFileInRootDirec(const char *filename);

typedef struct __attribute__ ((__packed__)) SuperBlock
{
  char sig[8];
  uint16_t totBlocks;
  uint16_t rootIndex;
  uint16_t dataStartIndex;
  uint16_t totDataBlocks;
  uint8_t numFATBlocks;
  char padding[4079];
}SuperBlock, superB_t;

typedef struct __attribute__ ((__packed__)) FATBlock
{
  uint16_t word;
}FATBlock, *fatB_t;

typedef struct __attribute__ ((__packed__)) FAT
{
  fatB_t blocks;
}FAT, FAT_t;

typedef struct __attribute__((__packed__)) Root
{
  char name[FS_FILENAME_LEN];
  uint32_t size;
  uint16_t firstIndex;
  char padding[10];
}Root, *root_t;

typedef struct FDTable
{
  int index;
  int offset;
}FDTable, *fdt_t;

superB_t superBlock;
FAT_t fat;
root_t rootDir = (root_t)malloc(sizeof(Root) * FS_FILE_MAX_COUNT);
fdt_t fdt = (fdt_t)malloc(sizeof(FDTable) * FS_OPEN_MAX_COUNT);

int fs_mount(const char *diskname)
{
  if (block_disk_open(diskname) == -1)
    return -1;
 
  if (!fat.blocks)
    return -1;

  for(int i = 0; i < FS_OPEN_MAX_COUNT;i++)
  {
    fdt[i].index = -1;
    fdt[i].offset = 0;
  }

  char givenSig[8] = "ECS150FS";
  
  if (block_read(0, (void *)&superBlock) == -1)
    return -1;

  if (strcmp(givenSig, superBlock.sig) != 0)
    return -1;

  if (block_disk_count() != superBlock.totBlocks)
    return -1;

  fat.blocks = (fatB_t)malloc(sizeof(FATBlock) * BLOCK_SIZE);
 
  int count = 0;
  for (int i = 1; i <= superBlock.numFATBlocks; i++)
  {
    if (block_read(i, (void*)&fat.blocks[(BLOCK_SIZE/2) * count]) == -1)
      return -1;
    count++;
  } 

  if (block_read(superBlock.rootIndex, (void*)&rootDir) == -1)
    return -1;

  return 0;
}

int fs_umount(void)
{
  //chck if still open file descriptors
  for(int i = 0; i < FS_OPEN_MAX_COUNT; i++)
  {
    //if a files index is not -1, the file is open 
    if(fdt[i].index != -1)
      return -1;
  }

  //write root directory out to disk
  if(block_write(superBlock.rootIndex, (void*)&rootDir) == -1)
    return -1;

  //write blocks out to disk
  int count = 0;
  for(int i = 1; i <= superBlock.numFATBlocks; i++)
  {
    if(block_write(i, (void*)&fat.blocks[(BLOCK_SIZE/2) * count]))
      return -1;
    count++;
  }

  //check disk can be closed
  if(block_disk_close() == -1)
    return -1;

  free(rootDir);
  free(fat.blocks);

  return 0;
}

int fs_info(void)
{
  return 0;
}

int findFileInRootDirec(const char *filename)
{
  for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
  {
    if(strcmp(rootDir[i].name, filename) == 0)
      return i; 
  }
  return -1;
}

int fs_create(const char *filename)
{
  //if filename is invalid
  if(filename == NULL)
    return -1;

  //if length of filename is too long
  if(strlen(filename) > FS_FILENAME_LEN)
    return -1;

  //if file name already exists in file directory
  if(findFileInRootDirec(filename) == -1)
    return -1;

  //find empty entry in root directory
  for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
  {
    if(rootDir[i].name == NULL){
      strcpy(rootDir[i].name, filename);
      rootDir[i].size = 0;
      rootDir[i].firstIndex = FAT_EOC; 
  }

  //if root directory is already full
  if(i == FS_FILE_MAX_COUNT - 1)
    return -1;

  return 0;
}

int fs_delete(const char *filename)
{
  //filename is invalid
  if(filename == NULL)
    return -1;

  //no file in root directory to delete
  int check = findFileInRootDirec(filename);
  if(check == -1)
    return -1;	  
  else 
  {
    //check if file is open
   
  } 
  return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
  return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
  return 0;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
  return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
  return 0;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
  return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
  return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
  return 0;
}

