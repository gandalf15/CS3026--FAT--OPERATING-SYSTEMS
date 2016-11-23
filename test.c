#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesys.h"


typedef struct mystructure {
  char mystr[10];
  int myint;
} lol;

int main(void){
  lol a;
  strcpy(a.mystr, "Marcel,QQR");
  memset(a.mystr, 0x0, 10);
  a.myint = 10001;

  printf("%d\n\n",&a);
  printf("%s\n\n", a.mystr);
  printf("%d\n\n", &a.myint);

  lol * ptr;
  lol ** dptr;

  lol b;
  ptr = &b;
  b = a;
  dptr = &ptr;

    printf("%d\n\n",&b);
    printf("%s\n\n", b.mystr);
    printf("%d\n\n", &b.myint);
    printf("%d\n\n",ptr);
    printf("%d\n\n",&ptr);
    printf("%d\n\n",&dptr);
    printf("%d\n\n",dptr);

  return 0;
}
