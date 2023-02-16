#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TEST_START_ADDR 0x100004
#define PAGE_SIZE       0x1000
#define TEST_NUM        64

int main() {
  char *start_addr = (char *)TEST_START_ADDR;  

  sys_move_cursor(0, 0);
  printf("Test1: Write random numbers to different pages.\n");

  char *start = start_addr;
  for (int i = 0; i < 64; i++) {
    int *addr = (int *)((char *)start + i * PAGE_SIZE);
    int val = rand() % 100;
    *addr = val;
    printf("0x%x-%d; ", addr, *addr);
  }

  sys_clear();
  sys_move_cursor(0, 0);
  printf("Test2: Read values written to memory in Test 1.\n");

  start = start_addr;
  for (int i = 0; i < 64; i++) {
    int *addr = (int *)((char *)start + i * PAGE_SIZE);
    printf("0x%x-%d; ", addr, *addr);
  }

  sys_clear();
  return 0;
}
