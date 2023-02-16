#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

static uint64_t alloc_kernel_pgtable_page() {
    pg_base += NORMAL_PAGE_SIZE;
    return pg_base;
}

static void map_va_to_io_pa(uint64_t va, uint64_t pa) {
    PTE *pgdir = (PTE *)PGDIR_VA;

    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & ((1 << PPN_BITS) - 1);
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & ((1 << PPN_BITS) - 1);

    if (pgdir[vpn2] == 0) {
        // alloc a new second-level page directory
        set_pfn(&pgdir[vpn2], alloc_kernel_pgtable_page() >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgdir[vpn2])));
    }

    PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
    if (pmd[vpn1] == 0) {
        // alloc a third-level page directory
        set_pfn(&pmd[vpn1], alloc_kernel_pgtable_page() >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }

    PTE *pte = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    set_pfn(&pte[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(&pte[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                        _PAGE_EXEC | _PAGE_USER);
}

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // map one specific physical region to virtual address
    for (uint64_t sz = 0; sz < size; sz += NORMAL_PAGE_SIZE) {
        uint64_t va = io_base + sz;
        uint64_t pa = phys_addr + sz;
        map_va_to_io_pa(va, pa);
    }

    void *ioremap_addr = (void *)io_base;
    io_base += size;

    local_flush_tlb_all();
    return ioremap_addr;
}
