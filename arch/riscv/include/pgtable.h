#ifndef PGTABLE_H
#define PGTABLE_H

#include <type.h>
#include <assert.h>

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9

#define SATP_ASID_SHIFT 44lu
#define SATP_MODE_SHIFT 60lu

#define NORMAL_PAGE_SHIFT 12lu
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT)
#define LARGE_PAGE_SHIFT 21lu
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT)

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void)
{
    __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
    __asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory");
}

static inline void local_flush_icache_all(void)
{
    asm volatile ("fence.i" ::: "memory");
}

static inline void set_satp(
    unsigned mode, unsigned asid, unsigned long ppn)
{
    unsigned long __v =
        (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) | ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
    __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR
#define PGDIR_VA 0xffffffc051000000
#define KERNEL_VM_OFFSET 0xffffffc000000000

/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define _PAGE_PRESENT (1 << 0)
#define _PAGE_READ (1 << 1)     /* Readable */
#define _PAGE_WRITE (1 << 2)    /* Writable */
#define _PAGE_EXEC (1 << 3)     /* Executable */
#define _PAGE_USER (1 << 4)     /* User */
#define _PAGE_GLOBAL (1 << 5)   /* Global */
#define _PAGE_ACCESSED (1 << 6) /* Set by hardware on any access \
                                 */
#define _PAGE_DIRTY (1 << 7)    /* Set by hardware on any write */
#define _PAGE_SOFT (1 << 8)     /* Reserved for software */

#define _PAGE_PFN_SHIFT 10lu

#define VA_MASK ((1lu << 39) - 1)
#define PTE_ATTR_MASK ((1lu << 8) - 1)
#define PTE_PPN_AND_ATTR_MASK ((1lu << 54) - 1)

#define PPN_BITS 9lu
#define NUM_PTE_ENTRY (1 << PPN_BITS)

#define KERNEL_VA_VPN2 0x101
#define IO_REMAP_VA_VPN2 0x180

#define get_va(vpn2, vpn1, vpn0) \
        (((vpn2 << (PPN_BITS + PPN_BITS)) | \
        (vpn1 << PPN_BITS) | (vpn0)) << NORMAL_PAGE_SHIFT)

typedef uint64_t PTE;

/* Translation between physical addr and kernel virtual addr */
static inline uintptr_t kva2pa(uintptr_t kva) {
    return kva - KERNEL_VM_OFFSET;
}

static inline uintptr_t pa2kva(uintptr_t pa) {
    return pa + KERNEL_VM_OFFSET;
}

/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(PTE entry)
{
    // set reserved bits in PTE to 0
    entry &= PTE_PPN_AND_ATTR_MASK;
    return (entry >> _PAGE_PFN_SHIFT) << NORMAL_PAGE_SHIFT;
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    entry &= PTE_PPN_AND_ATTR_MASK;
    return entry >> _PAGE_PFN_SHIFT;
}
static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    *entry = 0;
    *entry |= (pfn << _PAGE_PFN_SHIFT);
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry, uint64_t mask)
{
    return entry &= PTE_ATTR_MASK;
}
static inline void set_attribute(PTE *entry, uint64_t bits)
{
    *entry |= bits;
}

static inline void unset_attribute(PTE *entry, uint64_t bits) {
    *entry &= (~bits);
}

static inline void clear_pgdir(uintptr_t pgdir_addr)
{
    PTE *pgdir = (PTE *)pgdir_addr;
    for (int i = 0; i < NUM_PTE_ENTRY; i++) {
        pgdir[i] = 0;
    }
}

extern uintptr_t pg_base;

extern PTE *get_pte_of_uva(uint64_t uva, PTE *pgdir);

/* 
 * query the page table stored in pgdir_va to obtain the physical 
 * address corresponding to the virtual address va.
 * 
 * return the kernel virtual address of the physical address 
 */
extern uint64_t get_kva_of_uva(uintptr_t uva, PTE *pgdir);
extern void map_uva_to_kva(uint64_t uva, uint64_t kva, PTE *pgdir);

extern uintptr_t pg_base;

#define get_uva_from_vpns(vpn2, vpn1, vpn0) \
        ((((uint64_t)(vpn2) << (PPN_BITS + PPN_BITS)) | \
        ((uint64_t)(vpn1) << PPN_BITS) | ((uint64_t)(vpn0))) << NORMAL_PAGE_SHIFT)

#endif  // PGTABLE_H
