/* filesys.c
 *
 * provides interface to virtual disk
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesys.h"

diskblock_t  virtualDisk [MAXBLOCKS] ;           // define our in-memory virtual, with MAXBLOCKS blocks
fatentry_t   FAT         [MAXBLOCKS] ;           // define a file allocation table with MAXBLOCKS 16-bit entries
fatentry_t   rootDirIndex            = 0 ;       // rootDir will be set by format
direntry_t * currentDir              = NULL ;
fatentry_t   currentDirIndex         = 0 ;

void writeDisk ( const char * filename ){
   printf ( "writedisk> virtualdisk[0] = %s\n", virtualDisk[0].data ) ;
   FILE * dest = fopen( filename, "w" ) ;
   if ( fwrite ( virtualDisk, sizeof(virtualDisk), 1, dest ) < 0 )
      fprintf ( stderr, "write virtual disk to disk failed\n" ) ;
   //write( dest, virtualDisk, sizeof(virtualDisk) ) ;
   fclose(dest) ;
}

void readDisk ( const char * filename ){
   FILE * dest = fopen( filename, "r" ) ;
   if ( fread ( virtualDisk, sizeof(virtualDisk), 1, dest ) < 0 )
      fprintf ( stderr, "write virtual disk to disk failed\n" ) ;
   //write( dest, virtualDisk, sizeof(virtualDisk) ) ;
      fclose(dest) ;
}


/* the basic interface to the virtual disk
 * this moves memory around
 */

void writeBlock ( diskblock_t * block, int block_address ){
   //printf ( "writeblock> block %d = %s\n", block_address, block->data ) ;
   memmove ( virtualDisk[block_address].data, block->data, BLOCKSIZE ) ;
   //printf ( "writeblock> virtualdisk[%d] = %s / %d\n", block_address, virtualDisk[block_address].data, (int)virtualDisk[block_address].data ) ;
}

void copyFat(fatentry_t *FAT){
  int num_of_fat_blocks = (int)(MAXBLOCKS+(FATENTRYCOUNT-1))/FATENTRYCOUNT;
  diskblock_t block;
  int i,j,index = 0;
  for (i = 1; i <= num_of_fat_blocks; i++){
    for (j = 0; j < FATENTRYCOUNT; j++){
      block.fat[j] = FAT[index];
      ++index;
    }
    writeBlock(&block, i);
  }
}

void format (char * disk_name){
   diskblock_t block ;
   int i = 1;
   for (i = 0; i < BLOCKSIZE; i++){
     block.data[i] = '\0';
   }
   memcpy(block.data, disk_name, strlen(disk_name));
   writeBlock(&block, 0);
   FAT[0] = ENDOFCHAIN;
   int num_of_fat_blocks;
   num_of_fat_blocks = (MAXBLOCKS+(FATENTRYCOUNT-1))/FATENTRYCOUNT;
   for (i = 1; i < num_of_fat_blocks; i++){
     FAT[i] = i+1;
   }
   FAT[num_of_fat_blocks] = ENDOFCHAIN; //end of fat table
   FAT[num_of_fat_blocks+1] = ENDOFCHAIN; // root dir
   for(i = num_of_fat_blocks+2; i < MAXBLOCKS; i++){
     FAT[i] = UNUSED;
   }
   copyFat(FAT);
   diskblock_t rootBlock;
   rootBlock.dir.isdir = TRUE;
   rootBlock.dir.nextEntry = FALSE;
   rootDirIndex = num_of_fat_blocks+1;
   currentDirIndex = num_of_fat_blocks+1;
   direntry_t emptyDir;
   emptyDir.isdir = TRUE;
   emptyDir.unused = TRUE;
   emptyDir.filelength = 0;
   memcpy(emptyDir.name,"unused directory", strlen("empty directory"));
   for (i = 0; i < DIRENTRYCOUNT; i++){
     rootBlock.dir.entrylist[i] = emptyDir;
   }
   writeBlock(&rootBlock, (int)rootDirIndex);
}

int getFreeBlock(){
  int i;
  for(i = 0; i < MAXBLOCKS; i++){
    if (FAT[i] == UNUSED){return i;}
  }
  return -1;
}

diskblock_t initBlock(int index, const char type){
  diskblock_t block = virtualDisk[index];
  int i;
  if(type == DATA){
    for (i = 0; i < BLOCKSIZE; i++){
      block.data[i] = '\0';
    }
  }
  else{
    block.dir.isdir = TRUE;
    block.dir.nextEntry = 0;
    direntry_t *newEntry;
    for(i = 0; i < DIRENTRYCOUNT; i ++){
      newEntry = malloc(sizeof(direntry_t));
      newEntry->unused = TRUE;
      newEntry->filelength = 0;
      block.dir.entrylist[i] = *newEntry;
    }
  }
  writeBlock(&block, (int)index);
  return block;
}

int findEntryIndex(const char * name, const char * path){
  int i;
  while(TRUE){
    for(i = 0; i < DIRENTRYCOUNT; i++){
      if (memcmp(virtualDisk[currentDirIndex].dir.entrylist[i].name, name, strlen(name) + 1) == 0)
        return i;
    }
    if(FAT[currentDirIndex] == ENDOFCHAIN) break;
    currentDirIndex = FAT[currentDirIndex];
  }
  return -1;
}

int myRm (const char * name, const char * path){
  int entryIndex = findEntryIndex(name, path);
  int fatBlockIndex = virtualDisk[currentDirIndex].dir.entrylist[entryIndex].firstblock;
  virtualDisk[currentDirIndex].dir.entrylist[entryIndex].unused = TRUE;
  int nextIndex = fatBlockIndex;
  while(TRUE){
    if(FAT[nextIndex] == ENDOFCHAIN){
      FAT[nextIndex] = UNUSED;
      break;
    }
    nextIndex = FAT[fatBlockIndex];
    FAT[fatBlockIndex] = UNUSED;
    fatBlockIndex = nextIndex;
  }
}

char myfgetc(MyFILE * stream){
  if (stream->pos+1 >= BLOCKSIZE){
    if (FAT[stream->blockno] == ENDOFCHAIN){
      return EOF;
    }
    else{
      stream->blockno = FAT[stream->blockno];
      stream->buffer = virtualDisk[stream->blockno];
      stream->pos = 0;
      return stream->buffer.data[stream->pos];
    }
  }
  return stream->buffer.data[++stream->pos];
}

int myfputc(char b, MyFILE * stream){
  if (strncmp(stream->mode, "r", 1) == 0) return 1;
  if (stream->pos+1 == BLOCKSIZE){
    stream->pos = 0;
    if(FAT[stream->blockno] == ENDOFCHAIN){
      int index = getFreeBlock();
      stream->blockno = index;
      stream->buffer = initBlock(index, DATA);
      FAT[stream->blockno] = index;
      copyFat(FAT);
    }
  }
  stream->buffer.data[stream->pos++] = b;
  writeBlock(&stream->buffer, stream->blockno);
  return 0;
}

MyFILE * myfopen(char * name, const char * mode){
  int fileEntryIndex = findEntryIndex(name, name);
  if (fileEntryIndex == -1 && strncmp(mode, "w", 1) == 0){
    printf("Creating new file: %s\n", name);
    int freeBlockIndex = getFreeBlock();
    direntry_t *newEntry = malloc(sizeof(direntry_t));
    newEntry->unused = FALSE;
    newEntry->filelength = 0;
    newEntry->firstblock = freeBlockIndex;
    int i = 0;
    for(i = 0; i < DIRENTRYCOUNT; i ++){
      if (virtualDisk[currentDirIndex].dir.entrylist[i].unused == TRUE){
        virtualDisk[currentDirIndex].dir.entrylist[i] = *newEntry;
        break;
      }
    }
    MyFILE * ptrFile = malloc(sizeof(MyFILE));
    ptrFile->pos = 0;
    ptrFile->writing = 1;
    strcpy(ptrFile->mode, "w");
    ptrFile->blockno = freeBlockIndex;
    ptrFile->buffer = initBlock(freeBlockIndex, DATA);
    return ptrFile;
  }
  else{
    printf("no read mode");
  }
}

int myfclose(MyFILE *file)
{
  free(file);
  return 0;
}


/* use this for testing
 */

void printBlock ( int blockIndex ){
   printf ( "virtualdisk[%d] = %s\n", blockIndex, virtualDisk[blockIndex].data ) ;
}
