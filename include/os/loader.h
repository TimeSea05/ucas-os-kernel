#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#include <pgtable.h>

#define TASK_SECTORS_ENTRY_QEMU     0x58000000
#define TAKS_SECTORS_ENTRY_KVA_QEMU 0xffffffc058000000
#define READ_BLOCKS_MAX_NUM         64

// #define QEMU

uint64_t load_task_img(char *taskname, PTE *pgdir);

#ifdef QEMU
void load_all_tasks_qemu();
#endif  /* QEMU */

#endif