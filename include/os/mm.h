/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <type.h>
#include <os/list.h>
#include <pgtable.h>

#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K

#define FREEMEM_KERNEL      0xffffffc052000000
#define FREEMEM_KERNEL_STOP 0xffffffc060000000

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

extern int disk_sectors_startid;

extern void *kalloc();
extern void kfree(uint64_t base_addr);

extern void init_kernel_freemem();
extern void free_pagetable(PTE *pgdir);
extern void free_pgdir(PTE *pgdir);

extern PTE *get_pte_of_uva(uint64_t va, PTE *pgdir);

extern void share_pgtable(PTE *dest_pgdir, PTE *src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, PTE *pgdir);

typedef struct freemem {
    struct freemem *next;
} freemem_t;
extern freemem_t *freemem_list;

#define MAX_PFN         512
#define MAX_PRESENT_PFN 512
typedef struct page {
    uint64_t uva;
    uint64_t kva;
    PTE *pgdir;
    list_node_t list;
    int page_id;

    // start block id of the page on SD card
    // when this page is swapped out
    int sector_id;       
} page_t;

extern int page_id;
extern int present_pages_num;
extern list_head present_pages_queue;
extern page_t pages[MAX_PFN];

extern int disk_sectors_startid;
extern list_head swapped_pages_queue;

extern void add_new_pre_page(uint64_t uva, uint64_t kva, PTE *pgdir);
extern page_t *find_page_with_uva(uint64_t uva, PTE *pgdir, list_head *queue);
extern page_t *find_page_with_kva(uint64_t kva, list_head *queue);
extern void free_page_with_uva(uint64_t uva, PTE *pgdir);
extern void free_page_with_kva(uint64_t kva);

extern void swap_out();
extern void swap_in(page_t *page);
extern void swap_in_all_pages(PTE *pgdir);

typedef struct share_page {
    uint64_t kva;
    int ref;
    int key;
} share_page_t;

#define MAX_SHARE_PAGE_NUM 16
extern share_page_t share_pages[MAX_SHARE_PAGE_NUM];

#define SHARE_PAGE_UVA_START 0x100000
extern uintptr_t shm_page_get(int key);
extern void shm_page_dt(uintptr_t addr);

#endif /* MM_H */
