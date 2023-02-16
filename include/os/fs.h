#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>

/* macros of file system */
#define SUPERBLOCK_MAGIC 0x20221205
#define FS_START_SECTOR 0x100000
#define NUM_FDESCS 16

#define NBYTES_PER_BLOCK 4096
#define NBYTES_PER_SECTOR 512
#define NBITS_PER_SECTOR 4096
#define NSECTORS_PER_BLOCK 8

/* SECTOR NUM FOR FILE SYSTEM */
#define FS_START_SECTOR 0x100000
#define NSECTORS_FS 0x100000
#define NSECTORS_SUPERBLOCK 1
#define NSECTORS_BMAP 32
#define NSECTORS_IMAP 1
#define NSECTORS_INODEBLOCK 512

#define ROOT_DIR_INUM 1
#define TEST_BIG_FILE_INUM 2
#define NDIRECT_BLOCKS 10
#define NINDIRECT_BLOCKS 1024

#define DENTRY_NAME_LEN 26

#define NINODES_PER_BLOCK 64
#define NDENTRIES_PER_BLOCK 128

/* data structures of file system */
typedef struct superblock_t{
    // TODO [P6-task1]: Implement the data structure of superblock
    uint32_t magic;         // magic number
    uint32_t start;         // start sector number of file system
    uint32_t size;          // size of file system   (in sector)
    uint32_t bmap_offset;   // offset of block map   (in sector)
    uint32_t imap_offset;   // offset of inode map   (in sector)
    uint32_t inode_offset;  // offset of inode block (in sector)
    uint32_t data_offset;   // offset of data        (in sector)
    uint16_t inode_size;    // size of inode         (in byte)
    uint16_t dentry_size;   // size of dentry        (in byte)
} superblock_t;

typedef struct dentry{
    // TODO [P6-task1]: Implement the data structure of directory entry
    // size of dentry_t: 32 (bytes)
    uint8_t valid;
    uint32_t inum;
    char name[DENTRY_NAME_LEN + 1];
} dentry_t;

typedef enum inode_type {
    NONE,
    FILE,
    DIRECTORY
} inode_type_t;

enum LS_MODE {
    DEFAULT,
    ALL_MODE,
    LONG_MODE
} LS_MODE;

typedef struct inode { 
    // TODO [P6-task1]: Implement the data structure of inode
    // size of inode_t: 64(bytes)
    uint16_t type;                   // 1->file, 2->directory
    uint16_t permission;             // rwxrwxrwx
    uint32_t size;                   // file size in bytes
    uint32_t ref;                    // number of hard links
    uint32_t time_created;           // timestamp
    uint32_t direct[NDIRECT_BLOCKS]; // direct blocks
    uint32_t indirect;               // indirect block
    uint32_t double_indirect;        // double indirect block
} inode_t;

typedef struct fdesc_t{
    // TODO [P6-task2]: Implement the data structure of file descriptor
    int inum;
    short read;
    short write;
    uint64_t offset;
} fdesc_t;

/* modes of do_fopen */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern void init_superblock();
extern int init_dir_inode(inode_t *inode, int parent_inum);

/* fs function declarations */
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_touch(char *path);
extern int do_cat(char *path);
extern int do_fopen(char *path, int mode);
extern int do_fread(int fd, char *buff, int length);
extern int do_fwrite(int fd, char *buff, int length);
extern int do_fclose(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);

#endif