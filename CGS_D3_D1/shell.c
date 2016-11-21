#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesys.h"

void cgsD(){
  format("CS3026 Operating Systems Assessment 2016");
  writeDisk("virtualdiskD3_D1\0");
}

void main(){
  cgsD();
}
