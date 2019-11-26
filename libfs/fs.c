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
  char sig[8]; // signature
  uint16_t totBlocks; // total blocks in virtual disk
  uint16_t rootIndex; // index of root block
  uint16_t dataStartIndex; // index of start of data blocks
  uint16_t totDataBlocks; // total number of data blocks
  uint8_t numFATBlocks; // num of fat blocks
  char padding[4079];
}SuperBlock, superB_t;

typedef struct __attribute__ ((__packed__)) FATBlock
{
  uint16_t word; // one entry in fat
}FATBlock, *fatB_t;

typedef struct FAT
{
  fatB_t blocks; // array fat entries
}FAT, FAT_t;

typedef struct __attribute__((__packed__)) Root
{
  char name[FS_FILENAME_LEN]; // name of file
  uint32_t size; // size of file
  uint16_t firstIndex; // start index of file in fat
  char padding[10];
}Root, root_t;

typedef struct FDTable
{
  unsigned int indexInRoot; // index of file in root directory
  unsigned int offset; // offset of file that is opened
}FDTable, fdt_t;

superB_t superBlock;
FAT_t fat;
root_t rootDir[FS_FILE_MAX_COUNT];
fdt_t fdt[FS_OPEN_MAX_COUNT];

int fs_mount(const char *diskname)
{
  if (block_disk_open(diskname) == -1) // checks if disk is open
    return -1;
 
  if (fat.blocks) // checks if disk is mounted already
    return -1;

  for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) // initializes fd table
  {
    fdt[i].indexInRoot = -1;
    fdt[i].offset = 0;
  }

  if (block_read(0, (void *)&superBlock) == -1) // reads into super block
    return -1;
  
  int temp = superBlock.totBlocks;
  superBlock.sig[8] = '\0';
  if (strcmp(superBlock.sig, "ECS150FS") != 0) // checks if signature is ECS150FS
    return -1;

  superBlock.totBlocks = temp;
  if (block_disk_count() != superBlock.totBlocks) // checks if total blocks were read correctly
    return -1;

  uint16_t *buffer = (uint16_t*) malloc(sizeof(uint16_t) * BLOCK_SIZE); // allocate space for buffer
  fat.blocks = (fatB_t)malloc(sizeof(FATBlock) * superBlock.numFATBlocks * BLOCK_SIZE); // allocate space for fat
 
  int count = 0;
  for (int i = 1; i <= superBlock.numFATBlocks; i++) // reads in the fat entries from the disk
  {
    block_read(i, buffer);
    memcpy(fat.blocks + count, buffer, BLOCK_SIZE); // copies the buffer into fat
    count = count + BLOCK_SIZE;
  }

  if (block_read(superBlock.rootIndex, (void*)&rootDir) == -1) // reads in the root directory from the disk
    return -1;

  return 0;
}

int fs_umount(void)
{
  if(!fat.blocks) // checks that disk id mounted
    return -1;
  
  if(block_write(0, (void*)&superBlock) == -1) // writes super block back to the disk
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

  free(fat.blocks); // frees fat
  return 0;
}

int fs_info(void)
{
  if (!fat.blocks) // checks if disk is mounted
    return -1;

  int fatRatio = 0;
  int rootRatio = 0;

  for (int i = 0; i < superBlock.totDataBlocks; i++) // calculates fat ratio
  {
    if (fat.blocks[i].word != 0)
      fatRatio++;  
  }
  
  for (int i = 0; i < FS_FILE_MAX_COUNT; i++) // calculates root ratio
  {
    if (strlen(rootDir[i].name) != 0)
      rootRatio++;
  }

  int fatCount = 1;

  if (((superBlock.totDataBlocks * 2) / BLOCK_SIZE) != 0) // calculates total fat blocks
    fatCount = (superBlock.totDataBlocks * 2) / BLOCK_SIZE;

  printf("FS Info:\n");
  printf("total_blk_count=%d\n", superBlock.totBlocks);
  printf("fat_blk_count=%d\n", fatCount);
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
    if(strcmp(rootDir[i].name, filename) == 0) // checks if file name is in root directory
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
      
  while(dataSpot != FAT_EOC) // iterates through fat until it reaches FAT_EOC
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

  for (int i = 0; i < FS_FILE_MAX_COUNT; i++) // prints info from root directory
  {
    if (strlen(rootDir[i].name) != 0)
      printf("file: %s, size: %d, data_blk: %d\n", rootDir[i].name, rootDir[i].size, rootDir[i].firstIndex);
  }

  return 0;
}

int fs_open(const char *filename)
{
  if (filename == NULL) // checks if file name is null
    return -1;

  if (sizeof(filename) > FS_FILENAME_LEN) // checks if file name is an acceptable length
    return -1;

  int check = findFileInRootDirec(filename); // finds the index of the file in the root directory
  if (check == -1)
    return -1;

  int full = 0;
  int i;
  for (i = 0; i < FS_OPEN_MAX_COUNT; i++) // finds an open space in fd table
  {
    if (fdt[i].indexInRoot == -1)
    {
      fdt[i].indexInRoot = check;
      fdt[i].offset = 0;
      full = 1;
      break;
    }
  }

  if (!full) // checks if the fd table has reached its max
    return -1;

  return i; // returns fd id
}

int fs_close(int fd)
{
  if (fd < 0 || fd > 31) // checks valid fd
    return -1;

  if (fdt[fd].indexInRoot == -1) // checks that fd holds valid index
    return -1;

  fdt[fd].indexInRoot = -1; // resets fd table for file
  fdt[fd].offset = 0;

  return 0;
}

int fs_stat(int fd)
{
  if (fd < 0 || fd > 31) // checks if fd is valid
    return -1;

  if (fdt[fd].indexInRoot == -1) // checks if index in fd table is valid
    return -1;

  int result = rootDir[fdt[fd].indexInRoot].size; // returns size of file

  return result;
}

int fs_lseek(int fd, size_t offset)
{
  if (fd < 0 || fd > 31) // checks if fd is valid
    return -1;

  if (fdt[fd].indexInRoot == -1) // checks if index in fd table is valid
    return -1;

  if (offset > fs_stat(fd) || offset < 0) // checks if offset is valid
    return -1;

  fdt[fd].offset = offset; // changes offset 

  return 0;
}

int nextOpen()
{
  for (int i = 1; i < FS_FILE_MAX_COUNT; i++) // finds the next open spot in fat
  {
    if (fat.blocks[i].word == 0)
      return i;
  }

  return -1;
}

int fs_write(int fd, void *buf, size_t count)
{
  if (fd < 0 || fd > 31) // checks for vaild fd
    return -1;

  if (fdt[fd].indexInRoot == -1) // checks valid index in root directory
    return -1;

  if (fdt[fd].offset > fs_stat(fd) || fdt[fd].offset < 0) // checks for valid offset
    return -1;

  char *block = (char*) malloc(sizeof(char) * BLOCK_SIZE);
  unsigned int totalWrite = 0;
  unsigned int offset = fdt[fd].offset;
  unsigned int start = superBlock.dataStartIndex;
  unsigned int fileStartIndex = rootDir[fdt[fd].indexInRoot].firstIndex;

  if (fileStartIndex == FAT_EOC) // checks if the file has been written to yet 
  {
    fileStartIndex = nextOpen();
    rootDir[fdt[fd].indexInRoot].firstIndex = fileStartIndex;
    rootDir[fdt[fd].indexInRoot].size = count;
    fat.blocks[fileStartIndex].word = FAT_EOC;
  }

  if (offset + count <= BLOCK_SIZE) // checks if offset + count is less than block size
  {
    block_read((start + fileStartIndex), block);
    memcpy(block, buf, count);
    totalWrite = count;
    block_write((start + fileStartIndex), block);

  }
  else // if offset + count is greater than block size
  {
    unsigned int tempOffset = offset % BLOCK_SIZE;
    if (tempOffset > 0) // gets index into the end block of file
    {
      unsigned int move = offset / BLOCK_SIZE;
      unsigned int temp = fileStartIndex;
      for (int i = 0; i < move; i++) // finds the index of the block where the offset is
      {
        temp = fat.blocks[temp].word;
        if (i == (move - 1))
	  fileStartIndex = temp;
      }
      offset = tempOffset;
    }

    block_read((start + fileStartIndex), block); // reads the first block in
      
    unsigned int leftOver;
    if (offset == 0) // if offset is zero
    {
      leftOver = count - BLOCK_SIZE; // calculates bits left to read
      totalWrite = BLOCK_SIZE;
      memcpy(block, buf, BLOCK_SIZE);
      buf = buf + BLOCK_SIZE; // shifts buf over by how much we just wrote into the disk
      block_write((start + fileStartIndex), block);
    }
    else // if offset is not zero
    {
      leftOver = 0; // there is no more left to write from buf
      totalWrite = count;
      memcpy(block, buf, count);
      buf = buf + count;
      block_write((start + fileStartIndex), block); // writes the last part of buf into disk
    }
     
    unsigned int currIndex = fileStartIndex;
      
    while(leftOver != 0) // continues to write data from buf into disk until its all written
    {
      unsigned int prev = currIndex;
      currIndex = fat.blocks[currIndex].word;
      if (currIndex == FAT_EOC) // if we are at the end of a file then we must allocate a new block
      {
        unsigned int newSpot = nextOpen();
	if (newSpot == -1)
	  break;

        fat.blocks[prev].word = newSpot; // this sets prev block to point to next block
	fat.blocks[newSpot].word = FAT_EOC; // sets new block to FAT_EOC
	currIndex = newSpot;
      }

      if (leftOver <= BLOCK_SIZE) // if left to write is less than block size
      {
	block_write((start + currIndex), buf);
	buf = buf + leftOver;
	totalWrite = totalWrite + leftOver;
        leftOver = 0;
      }
      else // if left to write is more than block size
      {
	block_write((start + currIndex), buf);
	buf = buf + BLOCK_SIZE;
	leftOver = leftOver - BLOCK_SIZE;
	totalWrite = totalWrite + BLOCK_SIZE;
      }
    }  
  }

  free(block);
  fdt[fd].offset = fdt[fd].offset + totalWrite; // changes offset to new spot

  return totalWrite;
}

int fs_read(int fd, void *buf, size_t count)
{
  if (fd < 0 || fd > 31) // checks if fd is valud
    return -1;

  if (fdt[fd].indexInRoot == -1) // checks if index is valid
    return -1;

  if (fdt[fd].offset > fs_stat(fd) || fdt[fd].offset < 0) // checks if offset is valid
    return -1;

  if (fdt[fd].offset == fs_stat(fd)) // checks if offset is at the end of the file
    return 0;

  char *block = (char*) malloc(sizeof(char) * BLOCK_SIZE);
  unsigned int totalRead = 0;
  unsigned int offset = fdt[fd].offset;
  unsigned int limit = fs_stat(fd);
  unsigned int start = superBlock.dataStartIndex;
  unsigned int fileStartIndex = rootDir[fdt[fd].indexInRoot].firstIndex;

  if (limit > BLOCK_SIZE) // the file size if greater than 1 block
  {
    if (offset + count <= BLOCK_SIZE) // if offset + count is less than block size
    {
      block_read((start + fileStartIndex), block);
      block = block + offset;
      memcpy(buf, block, count);
      totalRead = count;
    }
    else // if offset + count is greater than block size
    {
      unsigned int tempOffset = offset % BLOCK_SIZE;
      if (tempOffset > 0) // finds index of offset in particular block
      {
        unsigned int move = offset / BLOCK_SIZE;
	unsigned int temp = fileStartIndex;
        for (int i = 0; i < move; i++) // finds the index of the block where offset is
        {
          temp = fat.blocks[temp].word;
          if (i == (move - 1))
	    fileStartIndex = temp;
        }
	offset = tempOffset;
      }

      block_read((start + fileStartIndex), block); // reads the block in from the disk
      block = block + offset;
      
      unsigned int leftOver;
      if (offset == 0) // if offset is zero
      {
	leftOver = count - BLOCK_SIZE;
        totalRead = BLOCK_SIZE;
        memcpy(buf, block, BLOCK_SIZE); // copies the block into buf
        buf = buf + BLOCK_SIZE; // makes space for block in buf
      }
      else // if offset is not zero
      {
	leftOver = 0;
        totalRead = count;
        memcpy(buf, block, count); // copies a count of bits from disk into buf
        buf = buf + count;
      }
     
      unsigned int currIndex = fileStartIndex;
      
      while(leftOver != 0) // reads from disk until we read everything that was wanted
      {
        currIndex = fat.blocks[currIndex].word;
	if (currIndex == FAT_EOC)
          break;

	block_read((start + currIndex), block);
        
	if (leftOver <= BLOCK_SIZE) // if remaining left to read is less than block size
        {
	  memcpy(buf, block, leftOver);
	  totalRead = totalRead + leftOver;
          leftOver = 0;
        }
	else // if remaining left to read is greater than block size
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

    if (limit <= count) // if file size is less than count
    {
      buf = buf + limit;
      totalRead = limit;
    }
    else // if file size is less than the block size and greater than the count
    {
      buf = buf + count;
      totalRead = count; 
    }
  }

  free(block);
  fdt[fd].offset = fdt[fd].offset + totalRead;
  return totalRead;
}
