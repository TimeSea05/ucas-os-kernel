#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define MAX_NAME_LENGTH 24

#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define APP_INFO_OFFSET (OS_SIZE_LOC - 0xc)

#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))
#define NTHBYTE(num, n) ((num >> (n * 8)) & 0xff)

#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))

typedef struct {
    char name[16];
    int sectors_num;
    int filesz;
    int memsz;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(task_info_t *taskinfo, int tasknum, FILE *img);

int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 2;
    task_info_t *task = &taskinfo[0];

    int kernel_filesz = 0, nsectors_kernel = 0;
    int task_file_sz = 0, nsectors_task = 0;
    int task_mem_sz = 0;

    int phyaddr = 0;
    FILE *fp, *img;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {
        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {
            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);
        
            // update nbytes of each part of image
            if (strcmp(*files, "main") == 0) {
                kernel_filesz += get_filesz(phdr);
            } else if (strcmp(*files, "bootblock") != 0) {
                task_file_sz  += get_filesz(phdr);
                task_mem_sz   += get_memsz(phdr);
            }
        }
        
        /* write padding bytes */
        if (strcmp(*files, "bootblock") == 0) {
            // bootblock
            write_padding(img, &phyaddr, ROUND(phyaddr, SECTOR_SIZE));
        } else if (strcmp(*files, "main") == 0) {
            // kernel
            nsectors_kernel = NBYTES2SEC(kernel_filesz);
            write_padding(img, &phyaddr, ROUND(phyaddr, SECTOR_SIZE));
        } else {
            // tasks
            nsectors_task = NBYTES2SEC(task_file_sz);
            write_padding(img, &phyaddr, ROUND(phyaddr, SECTOR_SIZE));

            // write task infomation to arr `taskinfo`
            strcpy(task->name, *files);
            task->sectors_num = nsectors_task;
            task->filesz = task_file_sz;
            task->memsz = task_mem_sz;

            // reset task attributes
            task++;
            task_file_sz = 0, task_mem_sz = 0;
        }
        files++;
        fclose(fp);
    }
    // write app-info to bootblock, offset: APP_INFO_OFFSET
    fseek(img, APP_INFO_OFFSET, SEEK_SET);

    // write these interger using fwrite
    int total_sectors_num = phyaddr / SECTOR_SIZE;
    fwrite(&kernel_filesz, sizeof(int), 1, img);
    fwrite(&tasknum, sizeof(int), 1, img);
    fwrite(&total_sectors_num, sizeof(int), 1, img);

    // write kernel sectors number and bootblock signature to bootblock
    short nsectors_kernel_short = (short)nsectors_kernel;
    fwrite(&nsectors_kernel_short, sizeof(short), 1, img);
    fprintf(img, "%c%c", BOOT_LOADER_SIG_1, BOOT_LOADER_SIG_2);
    printf("nsectors_kernel: %d\n", nsectors_kernel);

    // write taskinfo arr to the end of image file
    fseek(img, 0, SEEK_END);
    write_img_info(taskinfo, tasknum, img);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(task_info_t *taskinfo, int tasknum, FILE * img)
{
    for (int i = 0; i < tasknum; i++) {
        fwrite(&taskinfo[i], sizeof(task_info_t), 1, img);
    }
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
