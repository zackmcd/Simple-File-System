#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF

int findFileInRootDirec(const char *filename);
int nextOpen();

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

typedef struct FAT
{
  fatB_t blocks;
}FAT, FAT_t;

typedef struct __attribute__((__packed__)) Root
{
  char name[FS_FILENAME_LEN];
  uint32_t size;
  uint16_t firstIndex;
  char padding[10];
}Root, root_t;

typedef struct FDTable
{
  unsigned int indexInRoot;
  unsigned int offset;
}FDTable, fdt_t;

superB_t superBlock;
FAT_t fat;
root_t rootDir[FS_FILE_MAX_COUNT];
fdt_t fdt[FS_OPEN_MAX_COUNT];

int fs_mount(const char *diskname)
{
  if (block_disk_open(diskname) == -1)
    return -1;
 
  if (fat.blocks)
    return -1;

  for(int i = 0; i < FS_OPEN_MAX_COUNT; i++)
  {
    fdt[i].indexInRoot = -1;
    fdt[i].offset = 0;
  }

  if (block_read(0, (void *)&superBlock) == -1)
    return -1;
  
  int temp = superBlock.totBlocks;
  superBlock.sig[8] = '\0';
  if (strcmp(superBlock.sig, "ECS150FS") != 0)
    return -1;

  superBlock.totBlocks = temp;
  if (block_disk_count() != superBlock.totBlocks)
    return -1;

  fat.blocks = (fatB_t)malloc(sizeof(FATBlock) * superBlock.totDataBlocks);
 
  int count = 1;
  for (int i = 0; i < superBlock.numFATBlocks; i++)
  {
    if (block_read(count, (void*)&fat.blocks[(BLOCK_SIZE/2) * i]) == -1)
      return -1;
    count++;
  } 

  if (block_read(superBlock.rootIndex, (void*)&rootDir) == -1)
    return -1;

  return 0;
}

int fs_umount(void)
{
  if(!fat.blocks)
    return -1;
  
  if(block_write(0, (void*)&superBlock) == -1)
    return -1;

  //write blocks out to disk
  int count = 1;
  for(int i = 0; i < superBlock.numFATBlocks; i++)
  {
    if(block_write(count, (void*)&fat.blocks[(BLOCK_SIZE/2) * i]))
      return -1;
    count++;
  }
  
  //write root directory out to disk
  if(block_write(superBlock.rootIndex, (void*)&rootDir) == -1)
    return -1;

  //check disk can be closed
  if(block_disk_close() == -1)
    return -1;

  free(fat.blocks);
  return 0;
}

int fs_info(void)
{
  if (!fat.blocks)
    return -1;

  int fatRatio = 0;
  int rootRatio = 0;

  for (int i = 0; i < superBlock.totDataBlocks; i++)
  {
    if (fat.blocks[i].word != 0)
      fatRatio++;  
  }
  
  for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
  {
    if (strlen(rootDir[i].name) != 0)
      rootRatio++;
  }

  printf("FS Info\n");
  printf("total_blk_count=%d\n", superBlock.totBlocks);
  printf("fat_blk_count=%d\n", ((superBlock.totDataBlocks * 2) / BLOCK_SIZE));
  printf("rdir_blk=%d\n", superBlock.rootIndex);
  printf("data_blk=%d\n", superBlock.dataStartIndex);
  printf("data_blk_count=%d\n", superBlock.totDataBlocks);
  printf("fat_free_ratio=%d/%d\n",  (superBlock.totDataBlocks-fatRatio), superBlock.totDataBlocks);
  printf("rdir_free_ratio=%d/%d\n", (FS_FILE_MAX_COUNT-rootRatio), FS_FILE_MAX_COUNT);

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
  if(sizeof(filename) > FS_FILENAME_LEN) // uses sizeof to add + 1 for the '\0'
    return -1;

  //if file name already exists in file directory
  if(findFileInRootDirec(filename) != -1)
    return -1;

  //find empty entry in root directory
  int full = 0;
  for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
  {
    if(strlen(rootDir[i].name) == 0)
    {
      strcpy(rootDir[i].name, filename);
      rootDir[i].size = 0;
      rootDir[i].firstIndex = FAT_EOC;
      full = 1;
      break;
    }
  }
  
  //if root directory is already full
  if(!full)
    return -1;
  
  return 0;
}

int fs_delete(const char *filename)
{
  //filename is invalid
  if(filename == NULL)
    return -1;

  //if length of filename is too long
  if(sizeof(filename) > FS_FILENAME_LEN) // uses sizeif to add + 1 for the '\0'
    return -1;
  
  //no file in root directory to delete
  int check = findFileInRootDirec(filename);
  if(check == -1)
    return -1;

  unsigned int dataSpot = rootDir[check].firstIndex;
  int next;
  int end;
      
  while(dataSpot != FAT_EOC)
  {
    end = dataSpot;
    next = fat.blocks[dataSpot].word;
    fat.blocks[dataSpot].word = 0;
    dataSpot = next;

    if (dataSpot == FAT_EOC)
      fat.blocks[end].word = 0;
  }
      
  rootDir[check].name[0] = '\0'; // this clears the name from the root directory
  rootDir[check].size = 0;
  rootDir[check].firstIndex = FAT_EOC;

  return 0;
}

int fs_ls(void)
{
  if (!fat.blocks)
    return -1;

  printf("FS Ls:\n");

  for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
  {
    if (strlen(rootDir[i].name) != 0)
      printf("file: %s, size: %d, data_blk: %d\n", rootDir[i].name, rootDir[i].size, rootDir[i].firstIndex);
  }

  return 0;
}

int fs_open(const char *filename)
{
  if (filename == NULL)
    return -1;

  if (sizeof(filename) > FS_FILENAME_LEN)
    return -1;

  int check = findFileInRootDirec(filename);
  if (check == -1)
    return -1;

  int full = 0;
  int i;
  for (i = 0; i < FS_OPEN_MAX_COUNT; i++)
  {
    if (fdt[i].indexInRoot == -1)
    {
      fdt[i].indexInRoot = check;
      fdt[i].offset = 0;
      full = 1;
      break;
    }
  }

  if (!full)
    return -1;

  return i;
}

int fs_close(int fd)
{
  if (fd < 0 || fd > 31)
    return -1;

  if (fdt[fd].indexInRoot == -1)
    return -1;

  fdt[fd].indexInRoot = -1;
  fdt[fd].offset = 0;

  return 0;
}

int fs_stat(int fd)
{
  if (fd < 0 || fd > 31)
    return -1;

  if (fdt[fd].indexInRoot == -1)
    return -1;

  int result = rootDir[fdt[fd].indexInRoot].size;

  return result;
}

int fs_lseek(int fd, size_t offset)
{
  if (fd < 0 || fd > 31)
    return -1;

  if (fdt[fd].indexInRoot == -1)
    return -1;

  if (offset > fs_stat(fd) || offset < 0)
    return -1;

  fdt[fd].offset = offset;

  return 0;
}

int nextOpen()
{
  for (int i = 1; i < FS_FILE_MAX_COUNT; i++)
  {
    if (fat.blocks[i].word == 0)
      return i;
  }

  return -1;
}

int fs_write(int fd, void *buf, size_t count)
{
  if (fd < 0 || fd > 31)
    return -1;

  if (fdt[fd].indexInRoot == -1)
    return -1;

  if (fdt[fd].offset > fs_stat(fd) || fdt[fd].offset < 0)
    return -1;

  //if (fdt[fd].offset == BLOCK_SIZE && rootDir[fdt[fd].indexInRoot].firstIndex == FAT_EOC)
    //return 0;

  char *block = (char*) malloc(sizeof(char) * BLOCK_SIZE);
  unsigned int totalWrite = 0;
  unsigned int offset = fdt[fd].offset;
  //unsigned int limit = fs_stat(fd);
  unsigned int start = superBlock.dataStartIndex;
  unsigned int fileStartIndex = rootDir[fdt[fd].indexInRoot].firstIndex;

  if (fileStartIndex == FAT_EOC)
  {
    fileStartIndex = nextOpen();
    rootDir[fdt[fd].indexInRoot].firstIndex = fileStartIndex;
    rootDir[fdt[fd].indexInRoot].size = count;
    fat.blocks[fileStartIndex].word = FAT_EOC;
  }

  //if (limit > BLOCK_SIZE) // the file size if greater that 1 block
  //{
    if (offset + count <= BLOCK_SIZE)
    {
      block_read((start + fileStartIndex), block);
      //block = block + count;
      memcpy(block, buf, count);
      //block = block + count;
      totalWrite = count;
      block_write((start + fileStartIndex), block);

    }
    else
    {
      unsigned int tempOffset = offset % BLOCK_SIZE;
      if (tempOffset > 0)
      {
        unsigned int move = offset / BLOCK_SIZE;
	unsigned int temp = fileStartIndex;
        for (int i = 0; i < move; i++)
        {
          temp = fat.blocks[temp].word;
          if (i == (move - 1))
	    fileStartIndex = temp;
        }
	offset = tempOffset;
      }

      block_read((start + fileStartIndex), block);
      //block = block + offset;
      
      unsigned int leftOver;
      if (offset == 0)
      {
	leftOver = count - BLOCK_SIZE;
        totalWrite = BLOCK_SIZE;
        memcpy(block, buf, BLOCK_SIZE);
        //block = block + BLOCK_SIZE;
	buf = buf + BLOCK_SIZE;
	block_write((start + fileStartIndex), block);
      }
      else
      {
	leftOver = 0;
        totalWrite = count;
        memcpy(block, buf, count);
        //block = block + count;
	buf = buf + count;
	block_write((start + fileStartIndex), block);
      }
     
      unsigned int currIndex = fileStartIndex;
      
      while(leftOver != 0)
      {
	unsigned int prev = currIndex;
        currIndex = fat.blocks[currIndex].word;
	if (currIndex == FAT_EOC)
        {
          unsigned int newSpot = nextOpen();
	  if (newSpot == -1)
	    break;

          fat.blocks[prev].word = newSpot;
	  fat.blocks[newSpot].word = FAT_EOC;
	  currIndex = newSpot;
	}

	//block_read((start + currIndex), block);
        
	if (leftOver <= BLOCK_SIZE)
        {
	  //memcpy(buf, block, leftOver);
	  block_write((start + currIndex), buf);
	  buf = buf + leftOver;
	  totalWrite = totalWrite + leftOver;
          leftOver = 0;
        }
	else
	{
          //memcpy(buf, block, BLOCK_SIZE);
	  block_write((start + currIndex), buf);
	  buf = buf + BLOCK_SIZE;
	  leftOver = leftOver - BLOCK_SIZE;
	  totalWrite = totalWrite + BLOCK_SIZE;
	}
      }  
    }
  /*}
  else // the file size is less that the block size
  {
    block_read((start + fileStartIndex), block);
    block = block + offset;
    memcpy(buf, block, count);

    if (limit <= count)
    {
      buf = buf + limit;
      totalRead = limit;
    }
    else
    {
      buf = buf + count;
      totalRead = count; 
    }
  }*/

  //free(block);
  fdt[fd].offset = fdt[fd].offset + totalWrite;  

  return totalWrite;
}

int fs_read(int fd, void *buf, size_t count)
{
  if (fd < 0 || fd > 31)
    return -1;

  if (fdt[fd].indexInRoot == -1)
    return -1;

  if (fdt[fd].offset > fs_stat(fd) || fdt[fd].offset < 0)
    return -1;

  if (fdt[fd].offset == fs_stat(fd)) //BLOCK_SIZE && rootDir[fdt[fd].indexInRoot].firstIndex == FAT_EOC)
    return 0;

  char *block = (char*) malloc(sizeof(char) * BLOCK_SIZE);
  unsigned int totalRead = 0;
  unsigned int offset = fdt[fd].offset;
  unsigned int limit = fs_stat(fd);
  unsigned int start = superBlock.dataStartIndex;
  unsigned int fileStartIndex = rootDir[fdt[fd].indexInRoot].firstIndex;

  if (limit > BLOCK_SIZE) // the file size if greater that 1 block
  {
    //printf("%ld %ld %d %ld\n", start, fileStartIndex, offset, count);
    if (offset + count <= BLOCK_SIZE)
    {
      block_read((start + fileStartIndex), block);
      block = block + offset;
      memcpy(buf, block, count);
      totalRead = count;
    }
    else
    {
      unsigned int tempOffset = offset % BLOCK_SIZE;
      if (tempOffset > 0)
      {
        unsigned int move = offset / BLOCK_SIZE;
	unsigned int temp = fileStartIndex;
        for (int i = 0; i < move; i++)
        {
          temp = fat.blocks[temp].word;
          if (i == (move - 1))
	    fileStartIndex = temp;
        }
	offset = tempOffset;
      }

      block_read((start + fileStartIndex), block);
      block = block + offset;
      
      unsigned int leftOver;
      if (offset == 0)
      {
	leftOver = count - BLOCK_SIZE;
        totalRead = BLOCK_SIZE;
        memcpy(buf, block, BLOCK_SIZE);
        buf = buf + BLOCK_SIZE;
      }
      else
      {
	leftOver = 0;
        totalRead = count;
        memcpy(buf, block, count);
        buf = buf + count;
      }
     
      unsigned int currIndex = fileStartIndex;
      
      while(leftOver != 0)
      {
        currIndex = fat.blocks[currIndex].word;
	if (currIndex == FAT_EOC)
          break;

	block_read((start + currIndex), block);
        
	if (leftOver <= BLOCK_SIZE)
        {
	  memcpy(buf, block, leftOver);
	  totalRead = totalRead + leftOver;
          leftOver = 0;
        }
	else
	{
          memcpy(buf, block, BLOCK_SIZE);
	  buf = buf + BLOCK_SIZE;
	  leftOver = leftOver - BLOCK_SIZE;
	  totalRead = totalRead + BLOCK_SIZE;
	}
      }  
    }
  }
  else // the file size is less that the block size
  {
    block_read((start + fileStartIndex), block);
    block = block + offset;
    memcpy(buf, block, count);

    if (limit <= count)
    {
      buf = buf + limit;
      totalRead = limit;
    }
    else
    {
      buf = buf + count;
      totalRead = count; 
    }
  }

  //free(block);
  fdt[fd].offset = fdt[fd].offset + totalRead;
  return totalRead;
}
