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

void copyFat(fatentry_t *FAT, unsigned int num_of_fat_blocks){
  diskblock_t block;
  unsigned int i,j,index = 0;
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
   unsigned int i = 1;
   for (i = 0; i < BLOCKSIZE; i++){
     block.data[i] = '\0';
   }
   memcpy(block.data, disk_name, strlen(disk_name));
   writeBlock(&block, 0);
   FAT[0] = ENDOFCHAIN;
   unsigned int num_of_fat_blocks;
   num_of_fat_blocks = (unsigned int)(MAXBLOCKS+(FATENTRYCOUNT-1))/FATENTRYCOUNT;
   for (i = 1; i < num_of_fat_blocks; i++){
     FAT[i] = i+1;
   }
   FAT[num_of_fat_blocks] = ENDOFCHAIN; //end of fat table
   FAT[num_of_fat_blocks+1] = ENDOFCHAIN; // root dir
   for(i = num_of_fat_blocks+2; i < MAXBLOCKS; i++){
     FAT[i] = UNUSED;
   }
   copyFat(FAT, num_of_fat_blocks);
   diskblock_t rootBlock;
   rootBlock.dir.isdir = TRUE;
   rootBlock.dir.nextEntry = FALSE;
   rootDirIndex = num_of_fat_blocks+1;
   currentDirIndex = num_of_fat_blocks+1;
   direntry_t emptyDir;
   emptyDir.isdir = TRUE;
   emptyDir.unused = TRUE;
   emptyDir.filelength = 0;
   memcpy(emptyDir.name,"empty directory", strlen("empty directory"));
   for (i = 0; i < DIRENTRYCOUNT; i++){
     rootBlock.dir.entrylist[i] = emptyDir;
   }
   writeBlock(&rootBlock, (int)rootDirIndex);
}

/* use this for testing
 */

void printBlock ( int blockIndex ){
   printf ( "virtualdisk[%d] = %s\n", blockIndex, virtualDisk[blockIndex].data ) ;
}
