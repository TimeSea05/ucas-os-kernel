#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/loader.h>
#include <os/mm.h>
#include <type.h>

// loaded from bootblock in `main`
extern int tasks_num;

uint64_t load_task_img(char *name, PTE *pgdir)
{
#ifndef QEMU
    int block_id = kernel_sectors_num + 1;  // 1: bootblock
#else
    int block_id = 0;
#endif

    // try to find the task with given name in tasks array
    int task_idx = 0;
    for (; task_idx < tasks_num; task_idx++) {
        // found task with given name
        if (strcmp(tasks[task_idx].name, name) == 0) {
            break;
        }
        block_id += tasks[task_idx].sectors_num;
    }
    
    // task with given name not found
    // return 0 for failure
    if (task_idx == tasks_num) {
        return 0;
    }
    task_info_t *task = &tasks[task_idx];

    // allocate pages for user process
    // then fill the page with data from SD card
    int memsz = task->memsz, filesz = task->filesz;
    for (int sz = 0; sz < memsz; sz += PAGE_SIZE) {
        uint64_t uva = USER_VA_START + sz;
        uint64_t kva = alloc_page_helper(uva, pgdir);
        uint64_t pa  = kva2pa(kva);

        // in SD card, the sector number of each task is determined
        // by filesz, so you can read the SD card only when sz < filesz 
        if (sz < filesz) {
            int bytes_left = filesz - sz;
            int blocks_read = (NBYTES2SEC(bytes_left) > SECTORS_PER_PAGE) ? SECTORS_PER_PAGE : NBYTES2SEC(bytes_left);
            
        #ifdef QEMU
            uint8_t *dest = (uint8_t *)kva;
            uint8_t *src  = (uint8_t *)(TAKS_SECTORS_ENTRY_KVA_QEMU + block_id * SECTOR_SIZE);
            int len = blocks_read * SECTOR_SIZE;

            memcpy(dest, src, blocks_read * SECTOR_SIZE);
        #else
            bios_sdread(pa, blocks_read, block_id);
        #endif

            block_id += blocks_read;
        }

        // expand bss when sz + PAGE_SIZE > filesz
        // if the bss size of the program is 0
        // the following operations set the rest of the page to 0
        if (sz + PAGE_SIZE > filesz) {
            uint64_t bss_start, bss_end;
            bss_end = kva + PAGE_SIZE;
            if (ROUNDDOWN(filesz, PAGE_SIZE) == sz) {
                bss_start = kva + filesz % PAGE_SIZE;
            } else {
                bss_start = kva;
            }

            memset((char *)bss_start, 0, bss_end - bss_start);
        }
    }

    // successfully copy the bytes from SD card to
    // the pagetable of user process
    return 1;
}

#ifdef QEMU
// in qemu, slave processor cannot read SD card
// this is a bug, and has not been fixed by OSLab TAs
// to use qemu to debug our programs, the master processor
// need to load all task sectors into memory
void load_all_tasks_qemu() {
    uint64_t tasks_sectors_entry = TASK_SECTORS_ENTRY_QEMU;
    int tasks_sectors_num        = total_sectors_num - kernel_sectors_num - 1;
    int tasks_sectors_startid    = kernel_sectors_num + 1;

    // read 64 blocks at one time
    while (tasks_sectors_num > READ_BLOCKS_MAX_NUM) {
        bios_sdread(tasks_sectors_entry, READ_BLOCKS_MAX_NUM, tasks_sectors_startid);
        tasks_sectors_entry   += READ_BLOCKS_MAX_NUM * SECTOR_SIZE;
        tasks_sectors_num     -= READ_BLOCKS_MAX_NUM;
        tasks_sectors_startid += READ_BLOCKS_MAX_NUM;
    }
    // read remaining blocks
    bios_sdread(tasks_sectors_entry, tasks_sectors_num, tasks_sectors_startid);
}
#endif
