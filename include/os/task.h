#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define APP_INFO_ENTRY   0xffffffc0502001f0lu

#define TASK_MAXNUM      32
#define TASK_SIZE        0x10000

#define SECTOR_SIZE      512
#define SECTORS_PER_PAGE 8      // 4096/512

#define USER_VA_START    0x10000
#define USER_VA_SP       0xf00010000
#define USER_VA_SP_BASE  0xf0000f000
#define STACK_PAGE_NUM   1

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

typedef struct {
  char name[16];
  int sectors_num;
  int filesz;
  int memsz;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

// App info
extern int kernel_file_sz;
extern int kernel_sectors_num;
extern int total_sectors_num;
extern int tasks_num;

#endif