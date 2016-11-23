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
   //call init block and get clean block
   rootBlock.dir.isdir = TRUE;
   rootBlock.dir.nextEntry = FALSE; //does not have to be if the block is zeroed
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
   writeBlock(&rootBlock, rootDirIndex);
}

int getFreeBlock(){
  int i;
  for(i = 0; i < MAXBLOCKS; i++){
    if (FAT[i] == UNUSED){return i;}
  }
  return -1;
}

diskblock_t initBlock(int index, const char type){
  int i;
  diskblock_t block;
  if(type == DIR){
    block.dir.isdir = TRUE;
    block.dir.nextEntry = 0;
    direntry_t *newEntry = malloc(sizeof(direntry_t));
    newEntry->unused = TRUE;
    newEntry->filelength = 0;
    for(i = 0; i < DIRENTRYCOUNT; i ++){
      memcpy(&block.dir.entrylist[i], newEntry, DIRENTRYCOUNT);
    }
    free(newEntry);
  }
  else{
    int i;
    for (i = 0; i < BLOCKSIZE; i++) {
      block.data[i] = '\0';
    }
  }
  writeBlock(&block, index);
  return block;
}

int findEntryIndex(const char * name){
  int i;
  while(TRUE){
    for(i = 0; i < DIRENTRYCOUNT; i++){
      if (strcmp(virtualDisk[currentDirIndex].dir.entrylist[i].name, name) == 0)
        return i;
    }
    if(FAT[currentDirIndex] == ENDOFCHAIN) break;
    currentDirIndex = FAT[currentDirIndex];
  }
  return -1;
}

int myRm (const char * name){
  int entryIndex = findEntryIndex(name);
  int fatBlockIndex = virtualDisk[currentDirIndex].dir.entrylist[entryIndex].firstblock;
  virtualDisk[currentDirIndex].dir.entrylist[entryIndex].unused = TRUE;
  int nextIndex = fatBlockIndex;
  while(TRUE){
    if(FAT[nextIndex] == ENDOFCHAIN){
      FAT[nextIndex] = UNUSED;
      return 0;
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
      memcpy(&stream->buffer, &virtualDisk[stream->blockno], BLOCKSIZE);
      stream->pos = 0;
      return stream->buffer->data[stream->pos];
    }
  }
  return stream->buffer->data[++stream->pos];
}

int myfputc(char b, MyFILE * stream){
  printf("inside myfputc\n");
  if (strcmp(stream->mode, "r") == 0) return 1;
  printf("hsdaj %d\n", stream->pos+1);
  if (stream->pos+1 >= BLOCKSIZE){
    printf("before writeblock\n");
    writeBlock(stream->buffer,stream->blockno);
    printf("after write block\n");
    stream->pos = 0;
    int index = getFreeBlock();
    diskblock_t newBlock = initBlock(index, DATA);
    memcpy(&stream->buffer, &newBlock, BLOCKSIZE);
    printf("after memcpy\n");
    FAT[stream->blockno] = index;
    FAT[index] = ENDOFCHAIN;
    stream->blockno = index;
    copyFat(FAT);
  }
  printf("after if\n");
  printf("%p\n",&stream->buffer->data);
  stream->buffer->data[stream->pos++] = b;
  printf("after buf\n");
  writeBlock(stream->buffer, stream->blockno);
  return 0;
}

MyFILE * myfopen(char * name, char mode){
  int index = getFreeBlock();
  diskblock_t newBlock = initBlock(index, DATA);
  FAT[index] = ENDOFCHAIN;
  int i;
  for(i = 0; i < DIRENTRYCOUNT; i++){
    if (virtualDisk[currentDirIndex].dir.entrylist[i].unused == TRUE){
      virtualDisk[currentDirIndex].dir.entrylist[i].unused = FALSE;
      strcpy(virtualDisk[currentDirIndex].dir.entrylist[i].name, name);
      virtualDisk[currentDirIndex].dir.entrylist[i].firstblock = index;
      break;
    }
  }
  MyFILE * file = malloc(sizeof(MyFILE));
  file->pos = 0;
  file->writing = 0;
  strcpy(file->mode, &mode);
  file->blockno = index;
  memmove (&file->buffer, &virtualDisk[index], BLOCKSIZE );
  copyFat(FAT);
  printf("returning file\n");
  return file;
}

int myfclose(MyFILE *file){
  writeBlock(file->buffer,file->blockno);
  free(file);
  return 0;
}


/* use this for testing
 */

void printBlock ( int blockIndex ){
   printf ( "virtualdisk[%d] = %s\n", blockIndex, virtualDisk[blockIndex].data ) ;
}
