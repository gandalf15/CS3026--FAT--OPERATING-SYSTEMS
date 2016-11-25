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
   rootBlock.dir.parentDirBlock = FALSE;
   rootBlock.dir.parentDirEntry = FALSE;
   rootDirIndex = num_of_fat_blocks+1;
   currentDirIndex = num_of_fat_blocks+1;
   direntry_t *emptyDir = calloc(1,sizeof(direntry_t));
   emptyDir->isdir = TRUE;
   emptyDir->unused = TRUE;
   emptyDir->filelength = 0;
   strcpy(emptyDir->name,"\0");
   for (i = 0; i < DIRENTRYCOUNT; i++){
     rootBlock.dir.entrylist[i] = *emptyDir;
   }
   free(emptyDir);
   writeBlock(&rootBlock, rootDirIndex);
}

int getFreeBlockIndex(){
  int i;
  for(i = 0; i < MAXBLOCKS; i++){
    if (FAT[i] == UNUSED){
      FAT[i] = ENDOFCHAIN;
      copyFat(FAT);
      return i;
    }
  }
  return -1;
}

diskblock_t initBlock(int index, const char type, int parentDirBlock, int parentDirEntry){
  int i;
  diskblock_t block;
  if(type == DIR){
    block.dir.isdir = TRUE;
    block.dir.nextEntry = 0;
    block.dir.parentDirBlock = parentDirBlock;
    block.dir.parentDirEntry = parentDirEntry;
    direntry_t *newEntry = calloc(1, sizeof(direntry_t));
    newEntry->unused = TRUE;
    newEntry->filelength = 0;
    for(i = 0; i < DIRENTRYCOUNT; i ++){
      memmove(&block.dir.entrylist[i], newEntry, sizeof(direntry_t));
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
  if (entryIndex == -1){
    return 1;
  }
  int fatBlockIndex = virtualDisk[currentDirIndex].dir.entrylist[entryIndex].firstblock;
  virtualDisk[currentDirIndex].dir.entrylist[entryIndex].firstblock = 0;
  virtualDisk[currentDirIndex].dir.entrylist[entryIndex].isdir = FALSE;
  virtualDisk[currentDirIndex].dir.entrylist[entryIndex].unused = TRUE;
  virtualDisk[currentDirIndex].dir.entrylist[entryIndex].filelength = 0;
  int i;
  for (i = 0; i < MAXNAME; i++) {
    virtualDisk[currentDirIndex].dir.entrylist[entryIndex].name[i] = '\0';
  }
  int nextIndex = fatBlockIndex;
  while(TRUE){
    if(FAT[nextIndex] == ENDOFCHAIN){
      FAT[nextIndex] = UNUSED;
      copyFat(FAT);
      return 0;
    }
    nextIndex = FAT[fatBlockIndex];
    FAT[fatBlockIndex] = UNUSED;
    fatBlockIndex = nextIndex;
  }
}

char myfgetc(MyFILE * stream){
  if (stream->pos >= BLOCKSIZE){
    if (FAT[stream->blockno] == ENDOFCHAIN){
      return EOF;
    }
    stream->blockno = FAT[stream->blockno];
    memcpy(&stream->buffer, &virtualDisk[stream->blockno], BLOCKSIZE);
    stream->pos = 1;
    return stream->buffer.data[stream->pos-1];
  }
  if(stream->buffer.data[stream->pos] == EOF){
    return EOF;
  }
  ++stream->pos;
  return stream->buffer.data[stream->pos-1];
}

int myfputc(char b, MyFILE * stream){
  if (strcmp(stream->mode, "r") == 0) return 1;
  if (stream->pos >= BLOCKSIZE){
    writeBlock(&stream->buffer,stream->blockno);
    stream->pos = 0;
    int index = getFreeBlockIndex();
    if(index == -1){
      return 1;
    }
    diskblock_t newBlock = initBlock(index, DATA, 0, 0);
    memcpy(&stream->buffer, &newBlock, BLOCKSIZE);
    FAT[stream->blockno] = index;
    FAT[index] = ENDOFCHAIN;
    stream->blockno = index;
    copyFat(FAT);
  }
  stream->filelength += 1;
  stream->buffer.data[stream->pos] = b;
  stream->pos += 1;
  return 0;
}

direntry_t *initDirEntry(char *name, const char type, int first_block_index){
  FAT[first_block_index] = ENDOFCHAIN;    //change entry in FAT for DIR or DATA block
  int i;
  for(i = 0; i < DIRENTRYCOUNT; i++){
    if (virtualDisk[currentDirIndex].dir.entrylist[i].unused == TRUE){
      virtualDisk[currentDirIndex].dir.entrylist[i].unused = FALSE;
      virtualDisk[currentDirIndex].dir.entrylist[i].isdir = type;
      initBlock(first_block_index, type, currentDirIndex, i);
      memset(&virtualDisk[currentDirIndex].dir.entrylist[i].name, '\0', MAXNAME);
      strcpy(virtualDisk[currentDirIndex].dir.entrylist[i].name, name);
      virtualDisk[currentDirIndex].dir.entrylist[i].firstblock = first_block_index;
      currentDir = &virtualDisk[currentDirIndex].dir.entrylist[i];
      copyFat(FAT);
      return &virtualDisk[currentDirIndex].dir.entrylist[i];
    }
    if (i == 2){
      i = -1;
      int newBlockIndex = getFreeBlockIndex();
      if(newBlockIndex == -1){
        return NULL;
      }
      initBlock(newBlockIndex, DIR, virtualDisk[currentDirIndex].dir.parentDirBlock, virtualDisk[currentDirIndex].dir.parentDirEntry);
      FAT[currentDirIndex] = newBlockIndex;
      FAT[newBlockIndex] = ENDOFCHAIN;
      virtualDisk[currentDirIndex].dir.nextEntry = newBlockIndex;
      currentDirIndex = newBlockIndex;
    }
  }
}

int mymkdir(char *path){
  const char parser[2] = "/";
  char * pathCopy = malloc(strlen(path));
  strcpy(pathCopy, path);
  char *token;
  token = strtok(pathCopy,parser);
  if (pathCopy[0] != '/'){
    currentDirIndex = currentDir->firstblock;
  }
  currentDirIndex = rootDirIndex;
  while (token != NULL){
    int entryIndex = findEntryIndex(token);
    if (entryIndex > -1){
      currentDirIndex = virtualDisk[currentDirIndex].dir.entrylist[entryIndex].firstblock;

    }else{
      int freeBlockIndex = getFreeBlockIndex();
      initDirEntry(token, DIR, freeBlockIndex);
    }
    token = strtok(NULL, parser);
  }
  free(pathCopy);
  return 0;
}

char **mylistdir(char *path){
  const char parser[2] = "/";
  char * pathCopy = malloc(strlen(path));
  strcpy(pathCopy, path);
  char *token;
  int i = 0;
  char *s = &path;
  char relativePath = 0;
  for (i=0; s[i]; s[i]=='/' ? i++ : *s++);
  s = NULL;
  token = strtok(pathCopy,parser);
  int tempDirIndex = currentDirIndex;
  char **mylist = calloc(30,sizeof(char *));
  if (pathCopy[0] != '/'){
    relativePath = 1;
    currentDirIndex = currentDir->firstblock;
  }
  if (!relativePath){
    currentDirIndex = rootDirIndex;
  }
  while (token != NULL){
    i--;
    int dirEntryIndex = findEntryIndex(token);
    if (entryIndex > -1){
      int firstBlockOfDirBlock = virtualDisk[currentDirIndex].dir.entrylist[dirEntryIndex].firstblock;
      int j;
      while (FAT[currentDirIndex] != UNUSED){
        for (i = 0; i < DIRENTRYCOUNT; i++){
          if (virtualDisk[firstBlockOfDirBlock].entrylist[i].name[0] != '\0'){
            mylist[j] = malloc(strlen(virtualDisk[firstBlockOfDirBlock].entrylist[i].name));
            strcpy(mylist[j], virtualDisk[firstBlockOfDirBlock].entrylist[i].name);
            ++j;
          }else{
            currentDirIndex = tempDirIndex;
            free(pathCopy);
            return mylist;
          }
        }
        currentDirIndex = FAT[currentDirIndex];
      }
      currentDirIndex = tempDirIndex;
      free(pathCopy);
      return mylist;
    }else{
      free(pathCopy);
      return NULL;
    }
    token = strtok(NULL, parser);
  }
  dirEntryIndex = findEntryIndex(path);
  if (dirEntryIndex > -1){
    int firstBlockOfDirBlock = virtualDisk[currentDirIndex].dir.entrylist[dirEntryIndex].firstblock;
    int j;
    while (FAT[currentDirIndex] != UNUSED){
      for (i = 0; i < DIRENTRYCOUNT; i++){
        if (virtualDisk[firstBlockOfDirBlock].entrylist[i].name[0] != '\0'){
          mylist[j] = malloc(strlen(virtualDisk[firstBlockOfDirBlock].entrylist[i].name));
          strcpy(mylist[j], virtualDisk[firstBlockOfDirBlock].entrylist[i].name);
          ++j;
        }else{
          currentDirIndex = tempDirIndex;
          free(pathCopy);
          return mylist;
        }
      }
      currentDirIndex = FAT[currentDirIndex];
    }
    currentDirIndex = tempDirIndex;
    free(pathCopy);
    return mylist;
  }else{
    currentDirIndex = tempDirIndex;
    free(pathCopy);
    return NULL;
  }
  free(pathCopy);
  return 0;
}

MyFILE * myfopen(char * name,const char mode){
  if(mode == 'w'){
    myRm(name);
    int index = getFreeBlockIndex();
    initDirEntry(name, DATA, index);
    MyFILE * file = malloc(sizeof(MyFILE));
    memset(file, '\0', BLOCKSIZE);
    file->pos = 0;
    file->writing = 1;
    file->filelength = currentDir->filelength;
    //printf("when opened file length is %d\n", file->filelength);
    strcpy(file->mode, &mode);
    file->blockno = index;
    memmove (&file->buffer, &virtualDisk[index], BLOCKSIZE );
    copyFat(FAT);
    return file;
  }
  int dirEntryIndex = findEntryIndex(name);
  MyFILE * file = malloc(sizeof(MyFILE));
  file->pos = 0;
  file->writing = 0;
  strcpy(file->mode, &mode);
  file->blockno = virtualDisk[currentDirIndex].dir.entrylist[dirEntryIndex].firstblock;
  currentDir = &virtualDisk[currentDirIndex].dir.entrylist[dirEntryIndex];
  memcpy(&file->buffer, &virtualDisk[file->blockno], BLOCKSIZE);
  return file;
}

int myfclose(MyFILE *file){
  if (file->writing == 1){
    myfputc(EOF, file);
    currentDir->filelength = file->filelength;
    writeBlock(&file->buffer, file->blockno);
  }
  //printf("when closing file length is %d Bytes\n", currentDir->filelength);
  currentDir = NULL;
  free(file);
  return 0;
}


/* use this for testing
 */

void printBlock ( int blockIndex ){
   printf ( "virtualdisk[%d] = %s\n", blockIndex, virtualDisk[blockIndex].data ) ;
}
