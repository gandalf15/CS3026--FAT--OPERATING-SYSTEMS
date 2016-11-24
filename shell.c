#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesys.h"

MyFILE * myfopen(char * name, const char mode);
int myfputc(char b, MyFILE * stream);
char myfgetc(MyFILE * stream);
int myfclose(MyFILE *file);

void cgsD(){
  format("CS3026 Operating Systems Assessment 2016");
  writeDisk("virtualdiskD3_D1\0");
}

void cgsC(){
  // create a new file in write mode
  MyFILE *test_file = myfopen("test_file.txt", 'w');

  // the contents of this string are used when assigning random data to the file
  char string[] = "4096bytes"; //string used for filling the file with data

  // fill a 4kb file
  int i;
  for (i = 0; i < 4*BLOCKSIZE; i++){
    // myfputc(string[rand() % (int) (sizeof string - 1)], test_file); //use for random data
    myfputc('H', test_file);
  }

  myfclose(test_file);

  // use the real fopen to write the output to a text file
  FILE *f = fopen("testfileC3_C1_copy.txt", "w");

  // open our test_file in read mode
  MyFILE *test_file2 = myfopen("test_file.txt", "r");

  // loop over it until it hits EOF
  while(1){
    char character = myfgetc(test_file2);
    if (character == EOF){
      break;
    }
    // print it to the console and write it in the real file
    printf("%c", character);
    fprintf(f, "%c", character);
  }
  printf("\n");
  fclose(f);
  myfclose(test_file2);
  writeDisk("virtualdiskC3_C1\0");
}

void main(){
  cgsD();
  cgsC();
}
