#include <os/mm.h>
#include <os/sched.h>
#include <os/task.h>
#include <os/string.h>
#include <os/list.h>
#include <os/kernel.h>
#include <pgtable.h>
#include <assert.h>

// free memory list
freemem_t *freemem_list;

void init_kernel_freemem() {
    freemem_list = (freemem_t *)FREEMEM_KERNEL;
    freemem_list->next = NULL;
}

void *kalloc()
{
    freemem_t *mem = freemem_list;
    if (freemem_list->next == NULL) {
        freemem_list = (freemem_t *)((uint64_t)freemem_list + PAGE_SIZE);
        freemem_list->next = NULL;
    } else {
        freemem_list = freemem_list->next;        
    }

    memset(mem, 0, PAGE_SIZE);
    return mem;
}

void kfree(uint64_t base_addr)
{
    // kernel free memory starts at FREEMEM_KERNEL(0xffffffc052000000)
    // memory below FREEMEM_KERNEL should not be used(freed)
    if ((uint64_t)base_addr < FREEMEM_KERNEL) return;

    freemem_t *free_page = (freemem_t *)base_addr;
    free_page->next = freemem_list;
    freemem_list = free_page;
}

/* free a three-level user pagetable */
void free_pagetable(PTE *pgdir) {
    for (int vpn2 = 0; vpn2 < NUM_PTE_ENTRY; vpn2++) {
        // i != KERNEL_VA_VPN2: kernel pagetable cannot be cleaned
        if (pgdir[vpn2] != 0 && vpn2 != KERNEL_VA_VPN2 && vpn2 != IO_REMAP_VA_VPN2) {
            PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
            for (int vpn1 = 0; vpn1 < NUM_PTE_ENTRY; vpn1++) {
                // clear 3-rd level pagetable
                if (pmd[vpn1] != 0) {
                    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
                    for (int vpn0 = 0; vpn0 < NUM_PTE_ENTRY; vpn0++) {
                        if (pte[vpn0] != 0) {
                            uint64_t uva_aligned = get_uva_from_vpns(vpn2, vpn1, vpn0);
                            free_page_with_uva(uva_aligned, pgdir);
                            
                            pte[vpn0] = 0;
                        }
                    }
                    // free 3-rd level pagetable
                    kfree((uint64_t)pte);
                }
            }
            // clear & free 2-nd level pagetable
            memset(pmd, 0, PAGE_SIZE);
            kfree((uint64_t)pmd);
        }
    }
}

// clear & free pagetable directory(1-st level pagetable)
void free_pgdir(PTE *pgdir) {
    memset(pgdir, 0, PAGE_SIZE);
    kfree((uint64_t)pgdir);
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(PTE *dest_pgdir, PTE *src_pgdir)
{
    for (int i = 0; i < NUM_PTE_ENTRY; i++) {
        if (src_pgdir[i] != 0) {
            dest_pgdir[i] = src_pgdir[i];
        }
    }
}

void map_uva_to_kva(uint64_t uva, uint64_t kva, PTE *pgdir) {
    uva &= VA_MASK;
    uint64_t vpn2 = uva >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (uva >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & ((1 << PPN_BITS) - 1);
    uint64_t vpn0 = (uva >> NORMAL_PAGE_SHIFT) & ((1 << PPN_BITS) - 1);

    if (pgdir[vpn2] == 0) {
        // alloc a new second-level page directory
        set_pfn(&pgdir[vpn2], kva2pa((uintptr_t)kalloc()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgdir[vpn2])));
    }

    PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
    if (pmd[vpn1] == 0) {
        // alloc a third-level page directory
        set_pfn(&pmd[vpn1], kva2pa((uintptr_t)kalloc()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }

    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    set_pfn(&pte[vpn0], kva2pa(kva) >> NORMAL_PAGE_SHIFT);
    set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                        _PAGE_EXEC | _PAGE_USER);
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t uva, PTE *pgdir)
{
    uint64_t kva = (uint64_t)kalloc();
    map_uva_to_kva(uva, kva, pgdir);

    // check if `present_pages_num` has reached MAX_PRESENT_PFN
    // if yes, swap out a page 
    if (present_pages_num >= MAX_PRESENT_PFN) {
        swap_out();
    }
    add_new_pre_page(uva, kva, pgdir);

    return kva;
}

PTE *get_pte_of_uva(uint64_t uva, PTE *pgdir) {
    uva &= VA_MASK;
    uint64_t vpn2 = uva >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    if (pgdir[vpn2] == 0) {
        return NULL;
    }

    uint64_t vpn1 = (uva >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & ((1 << PPN_BITS) - 1);
    PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
    if (pmd[vpn1] == 0) {
        return NULL;
    }

    uint64_t vpn0 = (uva >> NORMAL_PAGE_SHIFT) & ((1 << PPN_BITS) - 1);
    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    if (pte[vpn0] == 0) {
        return NULL;
    }

    return &pte[vpn0];
}

uint64_t get_kva_of_uva(uintptr_t uva, PTE *pgdir) {
    PTE *pte = get_pte_of_uva(uva, pgdir);

    if (pte == NULL) {
        return 0;
    }
    return pa2kva(get_pa(*pte) | (uva & ((1 << NORMAL_PAGE_SHIFT) - 1)));
}

int page_id;
int present_pages_num;
page_t pages[MAX_PFN];

// list of present pages and swapped pages
LIST_HEAD(present_pages_queue);
LIST_HEAD(swapped_pages_queue);

void add_new_pre_page(uint64_t uva, uint64_t kva, PTE *pgdir) {
    // find an unused or freed page
    page_t *new_pre_page = NULL;
    for (int i = 0; i < MAX_PFN; i++) {
        if (pages[i].page_id == 0) {
            new_pre_page = &pages[i];
            break;
        }
    }
    assert(new_pre_page != NULL);

    new_pre_page->uva     = uva;
    new_pre_page->kva     = kva;
    new_pre_page->page_id = ++page_id;
    new_pre_page->pgdir   = pgdir;

    // add this page to `present_pages_queue`
    list_add_tail(&new_pre_page->list, &present_pages_queue);

    present_pages_num++;
}

// swap algorithm: FIFO
void swap_out() {
    // move swapped page to swapped_pages_queue
    page_t *swapped_page = list_entry(present_pages_queue.next, page_t);
    list_delete_entry(&swapped_page->list);
    list_add_tail(&swapped_page->list, &swapped_pages_queue);

    // write this page to SD card
    uint64_t pa = kva2pa(swapped_page->kva);
    bios_sdwrite(pa, SECTORS_PER_PAGE, disk_sectors_startid);
    swapped_page->sector_id = disk_sectors_startid;
    disk_sectors_startid += SECTORS_PER_PAGE;

    // unset PAGE_PRESENT bit in PTE
    // then free the memory the page is holding
    PTE *pte = get_pte_of_uva(swapped_page->uva, swapped_page->pgdir);
    unset_attribute(pte, _PAGE_PRESENT);
    kfree(swapped_page->kva);

    present_pages_num--;
    printk("%d out; ", swapped_page->page_id);
}

void swap_in(page_t *page) {
    list_delete_entry(&page->list);
    list_add_tail(&page->list, &present_pages_queue);

    page->kva = (uint64_t)kalloc();
    map_uva_to_kva(page->uva, page->kva, page->pgdir);
    
    // read this page from SD card
    uint64_t pa = kva2pa(page->kva);
    bios_sdread(pa, SECTORS_PER_PAGE, page->sector_id);
    page->sector_id = 0;
    
    present_pages_num++;
    printk("%d in; ", page->page_id);
}

// swap in all pages in swapped_pages satisfying
// page->pgdir == pgdir
void swap_in_all_pages(PTE *pgdir) {
    page_t *page, *page_q;
    list_for_each_entry_safe(page, page_q, &swapped_pages_queue) {
        if (page->pgdir == pgdir && present_pages_num < MAX_PRESENT_PFN) {
            swap_in(page);
        }
    }
}

page_t *find_page_with_uva(uint64_t uva, PTE *pgdir, list_head *queue) {
    page_t *page, *page_q;
    list_for_each_entry_safe(page, page_q, queue) {
        if (page->uva == uva && page->pgdir == pgdir) {
            return page;
        }
    }
    return NULL;
}

page_t *find_page_with_kva(uint64_t kva, list_head *queue) {
    page_t *page, *page_q;
    list_for_each_entry_safe(page, page_q, queue) {
        if (page->kva == kva) {
            return page;
        }
    }
    return NULL;
}

static void reset_page_info(page_t *page) {
    page->page_id = 0, page->sector_id = 0;
    page->uva     = 0, page->kva = 0;
}

void free_page_with_uva(uint64_t uva, PTE *pgdir) {
    page_t *page = NULL;
    
    page = find_page_with_uva(uva, pgdir, &present_pages_queue);
    if (page != NULL) {
        reset_page_info(page);
        list_delete_entry(&page->list);
        kfree(page->kva);
        present_pages_num--;
        
        return;
    }

    page = find_page_with_uva(uva, pgdir, &swapped_pages_queue);
    if (page == NULL) assert(0);

    reset_page_info(page);
    list_delete_entry(&page->list);
    kfree(page->kva);
}

void free_page_with_kva(uint64_t kva) {
    page_t *page = NULL;
    
    page = find_page_with_kva(kva, &present_pages_queue);
    if (page != NULL) {
        reset_page_info(page);
        list_delete_entry(&page->list);
        kfree(page->kva);
        present_pages_num--;
        
        return;
    }

    page = find_page_with_kva(kva, &swapped_pages_queue);
    if (page == NULL) assert(0);

    reset_page_info(page);
    list_delete_entry(&page->list);
    kfree(page->kva);
}

share_page_t share_pages[MAX_SHARE_PAGE_NUM];

uintptr_t shm_page_get(int key)
{   
    PTE *pgdir = current_running->pgdir;

    // try to find a share page with key
    for (int i = 0; i < MAX_SHARE_PAGE_NUM; i++) {
        if (share_pages[i].key == key) {
            share_pages[i].ref++;
            for (int j = 0; ; j++) {
                uint64_t uva = SHARE_PAGE_UVA_START + j * PAGE_SIZE;
                if (get_pte_of_uva(uva, pgdir) == NULL) {
                    map_uva_to_kva(uva, share_pages[i].kva, pgdir);
                    return uva;            
                }
            }
        }
    }

    // cannot find a page with key
    // allocate a new share page
    for (int i = 0; i < MAX_SHARE_PAGE_NUM; i++) {
        if (share_pages[i].key == 0) {
            share_pages[i].key = key;
            share_pages[i].ref++;
            for (int j = 0; ; j++) {
                uint64_t uva = SHARE_PAGE_UVA_START + j * PAGE_SIZE;
                if (get_pte_of_uva(uva, current_running->pgdir) == NULL) {
                    share_pages[i].kva = alloc_page_helper(uva, current_running->pgdir);
                    return uva;
                }
            }
        }
    }

    // unable to find or create a new share page
    return 0;    
}

void shm_page_dt(uintptr_t addr)
{
    PTE *pgdir = current_running->pgdir;
    uint64_t kva = get_kva_of_uva(addr, current_running->pgdir);
    // set kva page aligned
    kva &= (~((1 << NORMAL_PAGE_SHIFT) - 1));

    // unmap addr in current_running->pgdir
    PTE *pte = get_pte_of_uva(addr, pgdir);
    *pte = 0;

    for (int i = 0; i < MAX_SHARE_PAGE_NUM; i++) {
        if (share_pages[i].kva == kva) {
            share_pages[i].ref--;
            if (share_pages[i].ref == 0) {
                share_pages[i].key = 0;
                free_page_with_kva(share_pages[i].kva);
                share_pages[i].kva = 0;
            }
            break;
        }
    }
}
