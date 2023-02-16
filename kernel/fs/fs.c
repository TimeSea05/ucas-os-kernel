#include <os/string.h>
#include <os/sched.h>
#include <os/fs.h>
#include <os/kernel.h>
#include <os/time.h>

static superblock_t superblock;
static fdesc_t fdesc_array[NUM_FDESCS];

static char block_buf[NBYTES_PER_BLOCK];

int popcnt(uint64_t val) {
    int cnt = 0;
    while (val != 0) {
        if (val & 0x1) cnt++;
        val >>= 1;
    }

    return cnt;
}

int alloc_block() {
    uint64_t sector_buf[64];
    uint32_t bmap_start = superblock.start + superblock.bmap_offset;

    // sector num of bmap: 32
    int bnum = 0;
    int finished = 0;
    for (int i = 0; i < NSECTORS_BMAP; i++) {
        bios_sdread(kva2pa((uint64_t)sector_buf), 1, bmap_start + i);
        for (int j = 0; j < 64; j++) {
            uint64_t val = sector_buf[j];
            if (val == 0xffffffffffffffff) { bnum += 64; continue; }
            while (val & 1) { val >>= 1; bnum++; }
            sector_buf[j] |= (1 << (bnum % 64));
            finished = 1;
            break;
        }

        // update bmap
        bios_sdwrite(kva2pa((uint64_t)sector_buf), 1, bmap_start + i);
        if (finished) break;
    }

    return bnum + 1;
}

void read_block(int block_num) {
    int sector_num = superblock.start +  block_num * NSECTORS_PER_BLOCK;
    bios_sdread(kva2pa((uint64_t)block_buf), NSECTORS_PER_BLOCK, sector_num);
}

void write_block(int block_num) {
    int sector_num = superblock.start + block_num * NSECTORS_PER_BLOCK;
    bios_sdwrite(kva2pa((uint64_t)block_buf), NSECTORS_PER_BLOCK, sector_num);
}

void free_block(int block_num) {
    if (block_num == 0) return;

    int sector_offset = block_num / NBITS_PER_SECTOR;
    int arr_offset = (block_num % NBITS_PER_SECTOR) / 64;
    int bit_offset = (block_num % NBITS_PER_SECTOR) % 64 - 1;

    uint64_t sector_buf[64];
    uint32_t bmap_start = superblock.start + superblock.bmap_offset;
    bios_sdread(kva2pa((uint64_t)&sector_buf), 1, bmap_start + sector_offset);

    sector_buf[arr_offset] &= ~(1 << bit_offset);
    bios_sdwrite(kva2pa((uint64_t)&sector_buf), 1, bmap_start + sector_offset);
}

int alloc_inode() {
    uint64_t sector_buf[64];
    uint32_t imap_start = superblock.start + superblock.imap_offset;
    bios_sdread(kva2pa((uint64_t)sector_buf), 1, imap_start);

    int inum = 0;
    // try to find the first 0 bit
    for (int i = 0; i < 64; i++) {
        uint64_t val = sector_buf[i];
        if (val == 0xffffffffffffffff) { inum += 64; continue; }
        while (val & 1) { val >>= 1; inum++; }
        sector_buf[i] |= (1 << (inum % 64));
        break;
    }

    // update imap
    bios_sdwrite(kva2pa((uint64_t)sector_buf), 1, imap_start);

    return inum + 1;
}

void read_inode(int inode_num, inode_t *inode) {
    // sizeof inode => 64, 8 inodes in a sector
    uint32_t inode_start = superblock.start + superblock.inode_offset;
    uint32_t sector_num  = inode_start + (inode_num - 1) / NSECTORS_PER_BLOCK;
    uint32_t offset      = (inode_num - 1) % NSECTORS_PER_BLOCK;

    char sector_buf[NBYTES_PER_SECTOR];
    bios_sdread(kva2pa((uint64_t)sector_buf), 1, sector_num);
    memset(inode, 0, sizeof(inode_t));
    memcpy((uint8_t *)inode, (uint8_t *)(sector_buf + sizeof(inode_t) * offset), sizeof(inode_t));
}

void write_inode(int inode_num, inode_t *inode) {
    // sizeof inode => 64, 8 inodes in a sector
    uint32_t inode_start = superblock.start + superblock.inode_offset;
    uint32_t sector_num  = inode_start + (inode_num - 1) / NSECTORS_PER_BLOCK;
    uint32_t offset      = (inode_num - 1) % NSECTORS_PER_BLOCK;

    char sector_buf[NBYTES_PER_SECTOR];
    bios_sdread(kva2pa((uint64_t)sector_buf), 1, sector_num);
    memcpy((uint8_t *)(sector_buf + sizeof(inode_t) * offset), (uint8_t *)inode, sizeof(inode_t));
    bios_sdwrite(kva2pa((uint64_t)sector_buf), 1, sector_num);
}

void free_inode(int inode_num) {
    uint64_t sector_buf[64];
    uint32_t imap_start = superblock.start + superblock.imap_offset;
    bios_sdread(kva2pa((uint64_t)sector_buf), 1, imap_start);

    int offset = inode_num / 64, bit_offset = inode_num % 64 - 1;
    sector_buf[offset] &= ~(1 << bit_offset);

    // update imap
    bios_sdwrite(kva2pa((uint64_t)sector_buf), 1, imap_start);
}

int init_dir_inode(inode_t *inode, int parent_inum) {
    memset(inode, 0, sizeof(inode_t));
    memset(block_buf, 0, NBYTES_PER_BLOCK);

    int inode_num = alloc_inode();
    int block_num = alloc_block();

    inode->type = DIRECTORY;
    inode->permission = 0b1111101101;      // drwxr-xr-x
    inode->time_created = get_ticks();
    inode->size = 0;
    inode->direct[0] = block_num;
    inode->ref = 1;
    write_inode(inode_num, inode);

    // write directory entries to block
    dentry_t dentries[2];
    memset(dentries, 0, sizeof(dentries));
    strcpy(dentries[0].name, ".");
    strcpy(dentries[1].name, "..");
    dentries[0].inum = inode_num, dentries[1].inum = parent_inum;
    dentries[0].valid = 1, dentries[1].valid = 1;

    memcpy((uint8_t *)block_buf, (uint8_t *)dentries, 2 * sizeof(dentry_t));
    write_block(block_num);

    return inode_num;
}

void init_superblock() {
    uint64_t sector_buf[64];
    bios_sdread(kva2pa((uint64_t)sector_buf), 1, FS_START_SECTOR);
    memcpy((uint8_t *)(&superblock), (uint8_t *)sector_buf, sizeof(sector_buf));

    // no file system
    if (superblock.magic != SUPERBLOCK_MAGIC) {
        printk("No file system detected, start mkfs...\n");
        do_mkfs();
    } else {
        // set root directory
        current_running->cwd_inum = ROOT_DIR_INUM;
    }
}

void init_bigfile() {
    do_touch("8MB.dat");
    inode_t big_file;
    read_inode(TEST_BIG_FILE_INUM, &big_file);

    // size of test file: 8MB(1024 blocks)
    // 10 direct blocks, 1 indirect block(1024)
    // 1 double indirect block -- 1 indirect block(1014)

    // first: write block map
    // originally, 71 blocks are already occupied(70 blocks and 1 block for root inode)
    // now 2048 blocks are added => 2119 == 33 * 64 + 7
    uint64_t blk_map_buf[64];
    memset(blk_map_buf, 0, NBYTES_PER_SECTOR);
    for (int i = 0; i < 33; i++) blk_map_buf[i] = 0xffffffffffffffff;
    blk_map_buf[33] = 0x7f;

    uint32_t bmap_start = superblock.start + superblock.bmap_offset;
    bios_sdwrite(kva2pa((uint64_t)blk_map_buf), 1, bmap_start);

    int free_block = 72;
    // 10 direct blocks
    for (int i = 0; i < NDIRECT_BLOCKS; i++) {
        big_file.direct[i] = free_block++;
    }

    // 1 indirect block
    big_file.indirect = free_block++;
    memset(block_buf, 0, NBYTES_PER_BLOCK);
    uint32_t *indirect = (uint32_t *)block_buf;
    for (int i = 0; i < 1024; i++) {
        indirect[i] = free_block++;
    }
    write_block(big_file.indirect);
    
    // 1 double indirect block
    big_file.double_indirect = free_block++;
    memset(block_buf, 0, NBYTES_PER_BLOCK);
    uint32_t *db_indirect = (uint32_t *)block_buf;

    uint32_t db_1st_blk_num = free_block++;
    db_indirect[0] = db_1st_blk_num;
    write_block(big_file.double_indirect);

    memset(block_buf, 0, NBYTES_PER_BLOCK);
    uint32_t *db_1st_blk = (uint32_t *)block_buf;
    for (int i = 0; i < 1014; i++) {
        db_1st_blk[i] = free_block++;
    }
    write_block(db_1st_blk_num);

    big_file.size = 8388608;
    write_inode(TEST_BIG_FILE_INUM, &big_file);
}

// file system:
// | 512MB | superblock | block map | inode map | inode block | data block |
// start at 512MB(sector num: 0x100000)
// size of file system: 512MB (number of sectors: 0x100000)
// super block (1 sector)
// block map   (32 sectors: 512MB/4KB -> /512/8 => 32)
// inode map   (1 sector: support 4096 inodes => (4096/8)/512 => 1)
// inode block (512 sectors: 4096 inodes => 4096 * 64(sizeof inode) / 512 = 512)
// total of the above is 546 sectors => 560 sectors => 70 blocks(1 block = 8 sectors)
// data block  (4KB aligned, starts at offset 560)

int do_mkfs(void)
{
    // TODO [P6-task1]: Implement do_mkfs

    // memory buffer of 1 sector
    uint64_t sector_buf[64];
    for (int i = 0; i < 64; i++) sector_buf[i] = 0;

    printk("[FS] Start initializing file system!\n");
    printk("[FS] Setting superblock ...\n");

    superblock.magic = SUPERBLOCK_MAGIC;
    superblock.start = FS_START_SECTOR;
    superblock.size  = NSECTORS_FS;

    superblock.bmap_offset  = NSECTORS_SUPERBLOCK;
    superblock.imap_offset  = NSECTORS_SUPERBLOCK + NSECTORS_BMAP;
    superblock.inode_offset = NSECTORS_SUPERBLOCK + NSECTORS_BMAP + NSECTORS_IMAP;
    superblock.data_offset  = NSECTORS_SUPERBLOCK + NSECTORS_BMAP + NSECTORS_IMAP + NSECTORS_INODEBLOCK;

    superblock.inode_size   = sizeof(inode_t);
    superblock.dentry_size  = sizeof(dentry_t);

    // write super block
    memcpy((uint8_t *)sector_buf, (uint8_t *)(&superblock), sizeof(superblock));
    bios_sdwrite(kva2pa((uint64_t)sector_buf), 1, superblock.start);

    printk("     magic number: %x\n", superblock.magic);
    printk("     sectors num: %d, start sector: %d\n", superblock.size, superblock.start);    
    printk("     block map offset: %d(%d)\n", superblock.bmap_offset, NSECTORS_BMAP);
    printk("     inode map offset: %d(%d)\n", superblock.inode_offset, NSECTORS_IMAP);
    printk("     inode offset: %d(%d)\n", superblock.inode_offset, NSECTORS_INODEBLOCK);
    printk("     data offset: %d(%d)\n", superblock.data_offset, superblock.size - superblock.data_offset);
    
    // initialization of file system finished, number of blocks written: 70
    // write block map
    printk("[FS] Setting block-map...\n");
    for (int i = 0; i < 64; i++) sector_buf[i] = 0;
    sector_buf[0] = 0xffffffffffffffff, sector_buf[1] = 0x3f;  // 64 + 6 => 70
    bios_sdwrite(kva2pa((uint64_t)sector_buf), 1, superblock.start + superblock.bmap_offset);

    // set rest of block map sectors to 0
    uint32_t bmap_start = superblock.start + superblock.bmap_offset;
    for (int i = 0; i < 64; i++) sector_buf[i] = 0;
    for (int i = 1; i < NSECTORS_BMAP; i++) {
        bios_sdwrite(kva2pa((uint64_t)sector_buf), 1, bmap_start + i);
    }

    // set inode map sectors to 0
    uint32_t imap_start = superblock.start + superblock.imap_offset;
    printk("[FS] Setting inode-map...\n");
    bios_sdwrite(kva2pa((uint64_t)sector_buf), 1, imap_start);

    // mount root directory /
    // in /, . & .. have the same inum 1
    printk("[FS] Setting inode...\n");
    inode_t root;
    init_dir_inode(&root, ROOT_DIR_INUM);
    current_running->cwd_inum = ROOT_DIR_INUM;  // root directory

    // initialize test file: 8MB.dat
    printk("[TEST] Init 8MB.dat...\n");
    init_bigfile();

    printk("[FS] File system initialization finished!\n");
    return 0;
}

int do_statfs(void)
{
    // TODO [P6-task1]: Implement do_statfs
    // memory buffer for 1 sector
    uint64_t sector_buf[64];
    bios_sdread(kva2pa((uint64_t)sector_buf), 1, FS_START_SECTOR);
    memcpy((uint8_t *)(&superblock), (uint8_t *)sector_buf, sizeof(sector_buf));
    printk("magic: %x\n", superblock.magic);

    // now read block map to find out how many blocks have been used
    uint32_t bmap_start = superblock.start + superblock.bmap_offset;
    uint32_t blocks_used = 0;
    for (int i = 0; i < NSECTORS_BMAP; i++) {
        bios_sdread(kva2pa((uint64_t)sector_buf), 1, bmap_start + i);
        for (int j = 0; j < 64; j++) {
            int cnt = popcnt(sector_buf[j]);
            blocks_used += cnt;
            if (cnt == 0) goto bmap_rd_finish;
        }
    }
bmap_rd_finish:
    printk("used blocks: %d/131076, start sector: %d\n", blocks_used, superblock.start);
    printk("block map offset: %d, occupied sector: 32\n", superblock.bmap_offset);

    // now read inode map to find out how many inodes have been used
    uint32_t inodes_used = 0;
    bios_sdread(kva2pa((uint64_t)sector_buf), 1, superblock.start + superblock.imap_offset);
    for (int i = 0; i < 64; i++) {
        int cnt = popcnt(sector_buf[i]);
        inodes_used += cnt;
        if (cnt < 32) break;
    }
    printk("inode map offset: %d, occupied sector: 1, used: %d/4096\n",
            superblock.imap_offset, inodes_used);

    printk("data offset: %d, occupied sector: %d\n",
            superblock.data_offset, superblock.size - superblock.data_offset);
    printk("inode entry size: %dB, dir entry size: %dB\n",
            superblock.inode_size, superblock.dentry_size);

    return 0;  // do_statfs succeeds
}

/// @brief get inode number from file or dir path
/// @param path file or dir path
/// @param cwnd_inum inode number of the previous level directory
/// @return inode number of target file or dir, 0 for failure
int parse_path (char *path, int cwnd_inum) {
    if (*path == 0) return cwnd_inum;

    char path_copy[16];
    bzero(path_copy, 16);
    strcpy(path_copy, path);

    char *next = path_copy;
    while (*next != '/' && *next != 0) next++;
    *next++ = 0;

    inode_t inode;
    read_inode(cwnd_inum, &inode);

    // FIXME: add indirect block support
    for (int i = 0; i < NDIRECT_BLOCKS; i++) {
        uint32_t block_num = inode.direct[i];
        if (block_num == 0) break;

        read_block(block_num);
        dentry_t *dentries = (dentry_t *)block_buf;
        for (int j = 0; j < NDENTRIES_PER_BLOCK; j++) {
            if (dentries[j].valid && strcmp(dentries[j].name, path) == 0) {
                return parse_path(next, dentries[j].inum);
            } else if (dentries[j].inum == 0) {
                return 0;
            }
        }
    }

    return 0;
}

int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd
    int inum = parse_path(path, current_running->cwd_inum);
    if (inum == 0) {
        printk("cd failed: no such directory.\n");
        return 1;
    }

    inode_t new_cwd;
    read_inode(inum, &new_cwd);
    if (new_cwd.type == FILE) {
        printk("cd failed: not a directory.\n");
        return 1;
    }

    current_running->cwd_inum = inum;
    return 0;  // do_cd succeeds
}

int do_mkdir(char *path)
{
    // TODO: [P6-task1]: Implement do_mkdir
    // FIXME: add indirect block support
    inode_t cwd;
    int cwd_inum = current_running->cwd_inum;
    read_inode(cwd_inum, &cwd);

    if (strlen(path) > DENTRY_NAME_LEN) {
        printk("mkdir failed: dir name too long.\n");
        return 1;
    }

    // check if the directory already exists
    // read direct blocks
    uint32_t block_num;
    for (int i = 0; i < NDIRECT_BLOCKS; i++) {
        block_num = cwd.direct[i];
        if (block_num == 0) break;

        read_block(block_num);
        dentry_t *dentries = (dentry_t *)block_buf;
        for (int j = 0; j < NDENTRIES_PER_BLOCK; j++) {
            if (dentries[j].valid && strcmp(path, dentries[j].name) == 0) {
                printk("mkdir failed: dir already exists.\n");
                return 1;
            } else if (dentries[j].inum == 0) {
                goto blk_rd_finish;
            }
        }
    }

    inode_t new_dir;
    int new_dir_inum;
    dentry_t *dentry;

blk_rd_finish:
    new_dir_inum = init_dir_inode(&new_dir, cwd_inum);
    
    // append new dentry to cwd
    read_block(block_num);
    dentry = (dentry_t *)block_buf;
    while (dentry->inum != 0) dentry++;
    dentry->inum = new_dir_inum;
    dentry->valid = 1;
    strcpy(dentry->name, path);
    write_block(block_num);

    return 0;  // success
}

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir
    // FIXME: add support for multi-level directory
    inode_t cwd_inode, dir_inode;
    int cwd_inum = current_running->cwd_inum;
    int dir_inum = parse_path(path, cwd_inum);
    read_inode(cwd_inum, &cwd_inode);
    read_inode(dir_inum, &dir_inode);

    if (dir_inode.type == FILE) {
        printk("rm failed: not a directory!\n");
        return 1;
    }

    // remove directory: find dentry, set valid to 0
    uint32_t block_num;
    for (int i = 0; i < 10; i++) {
        block_num = cwd_inode.direct[i];
        if (block_num == 0) break;

        read_block(block_num);
        dentry_t *dentries = (dentry_t *)block_buf;
        for (int j = 0; j < NDENTRIES_PER_BLOCK; j++) {
            if (dentries[j].valid && strcmp(dentries[j].name, path) == 0) {
                dentries[j].valid = 0;
                goto blk_rd_finish;
            } else if (dentries[i].inum == 0) {
                printk("rm failed: no such directory!\n");
                return 1;
            }
        }
    }

blk_rd_finish:
    write_block(block_num);
    free_inode(dir_inum);
    for (int i = 0; i < NDIRECT_BLOCKS; i++) {
        free_block(dir_inode.direct[i]);
    }

    return 0;  // do_rmdir succeeds
}

void print_dentry(dentry_t entry, int option) {
    inode_t inode;
    switch (option) {
        case DEFAULT:
            if (entry.name[0] != '.') {
                printk("%s ", entry.name);
            }
            break;
        case ALL_MODE:
            printk("%s ", entry.name);
            break;
        case LONG_MODE:
            read_inode(entry.inum, &inode);
            assert(inode.type == FILE || inode.type == DIRECTORY);
            
            // inode type
            if (inode.type == FILE)           printk("-");
            else if (inode.type == DIRECTORY) printk("d");
            
            printk("rwxr-xr-x ");              // permission
            printk("root ");                   // username
            printk("%d ", inode.size);         // file size, directory -> 0
            printk("%x ", inode.time_created); // create time
            printk("%s\n", entry.name);        // file or dir name
            
    }
}

int do_ls(char *path, int option)
{
    // TODO: [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    int inum = parse_path(path, current_running->cwd_inum);
    inode_t inode;
    read_inode(inum, &inode);

    // FIXME: add indirect block support
    // FIXME: add "-a", "-l" support
    if (inode.type == FILE) {
        printk("ls failed: not a directory!\n");
        return 1;
    }

    for (int i = 0; i < NDIRECT_BLOCKS; i++) {
        uint32_t block_num = inode.direct[i];
        if (block_num == 0) break;

        read_block(block_num);
        dentry_t *dentries = (dentry_t *)block_buf;
        for (int j = 0; j < NDENTRIES_PER_BLOCK; j++) {
            if (dentries[j].valid) {
                print_dentry(dentries[j], option);
            } else if (dentries[j].inum == 0) {
                break;
            }
        }
    }

    printk("\n");
    return 0;  // do_ls succeeds
}

int do_touch(char *path)
{
    // TODO [P6-task2]: Implement do_touch
    inode_t cwd;
    int cwd_inum = current_running->cwd_inum;
    read_inode(cwd_inum, &cwd);

    if (strlen(path) > DENTRY_NAME_LEN) {
        printk("touch failed: dir name too long.\n");
        return 1;
    }

    // check if the directory already exists
    // read direct blocks
    uint32_t block_num;
    for (int i = 0; i < NDIRECT_BLOCKS; i++) {
        block_num = cwd.direct[i];
        if (block_num == 0) break;

        read_block(block_num);
        dentry_t *dentries = (dentry_t *)block_buf;
        for (int j = 0; j < NDENTRIES_PER_BLOCK; j++) {
            if (dentries[j].valid && strcmp(path, dentries[j].name) == 0) {
                printk("mkdir failed: dir already exists.\n");
                return 1;
            } else if (dentries[j].inum == 0) {
                goto blk_rd_finish;
            }
        }
    }

    inode_t new_file;
    int new_file_inum;
    dentry_t *dentry;

blk_rd_finish:
    // set file inode
    new_file_inum = alloc_inode();

    memset((uint8_t *)&new_file, 0, sizeof(new_file));
    new_file.type = FILE;
    new_file.size = 0;
    new_file.time_created = get_ticks();
    new_file.permission = 0b0111101101;
    new_file.ref = 1;
    write_inode(new_file_inum, &new_file);
    
    // write directory entries to block
    read_block(block_num);
    dentry = (dentry_t *)block_buf;
    while (dentry->inum != 0) dentry++;
    dentry->inum = new_file_inum;
    dentry->valid = 1;
    strcpy(dentry->name, path);
    write_block(block_num);

    return 0;  // do_touch succeeds
}

int do_cat(char *path)
{
    // TODO [P6-task2]: Implement do_cat
    // FIXME: add indirect block support
    int inum = parse_path(path, current_running->cwd_inum);
    inode_t inode;
    read_inode(inum, &inode);

    if (inode.type == DIRECTORY) {
        printk("cat failed: not a file.\n");
        return 1;
    }

    char str_buf[33];
    for (int i = 0; i < NDIRECT_BLOCKS; i++) {
        read_block(inode.direct[i]);
        for (int j = 0; j < 128; j++) {
            memcpy((uint8_t *)str_buf, (uint8_t *)&block_buf[j * 32], 32);
            str_buf[32] = 0;

            // cat finished
            if (str_buf[0] == 0) return 0;
            printk("%s", str_buf);
        }
    }

    return 0;  // do_cat succeeds
}

int offset_to_blk(uint32_t offset, inode_t *inode) {
    int blk_num = offset / NBYTES_PER_BLOCK;

    if (blk_num < NDIRECT_BLOCKS) return inode->direct[blk_num];
    else if (blk_num >= NDIRECT_BLOCKS && blk_num < NDIRECT_BLOCKS + NINDIRECT_BLOCKS) {
        read_block(inode->indirect);
        uint32_t *indirect = (uint32_t *)block_buf;
        return indirect[blk_num - NDIRECT_BLOCKS];
    } else {
        blk_num -= (NDIRECT_BLOCKS + NINDIRECT_BLOCKS);
        int db_indirect_offset = blk_num / NINDIRECT_BLOCKS;
        int indirect_offset = blk_num % NINDIRECT_BLOCKS;

        read_block(inode->double_indirect);
        uint32_t *db_indirect = (uint32_t *)block_buf;
        uint32_t db_indirect_blk_num = db_indirect[db_indirect_offset];

        read_block(db_indirect_blk_num);
        uint32_t *indirect = (uint32_t *)block_buf;
        return indirect[indirect_offset];
    }

    return 0;
}

void set_inode_blk_num(uint32_t offset, int new_blk, inode_t *inode, int inum) {
    int blk_num = offset / NBYTES_PER_BLOCK;
    
    if (blk_num < NDIRECT_BLOCKS) {
        inode->direct[blk_num] = new_blk;
        write_inode(inum, inode);
    } else if (blk_num >= NDIRECT_BLOCKS && blk_num < NDIRECT_BLOCKS + NINDIRECT_BLOCKS) {
        read_block(inode->indirect);
        uint32_t *indirect = (uint32_t *)block_buf;
        indirect[blk_num - NDIRECT_BLOCKS] = new_blk;
        write_block(inode->indirect);
    } else {
        blk_num -= (NDIRECT_BLOCKS + NINDIRECT_BLOCKS);
        int db_indirect_offset = blk_num / NINDIRECT_BLOCKS;
        int indirect_offset = blk_num % NINDIRECT_BLOCKS;

        read_block(inode->double_indirect);
        uint32_t *db_indirect = (uint32_t *)block_buf;
        uint32_t db_indirect_blk_num = db_indirect[db_indirect_offset];

        read_block(db_indirect_blk_num);
        uint32_t *indirect = (uint32_t *)block_buf;
        indirect[indirect_offset] = new_blk;
        write_block(db_indirect_blk_num);
    }
}

int do_fopen(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_fopen
    int inum = parse_path(path, current_running->cwd_inum);
    if (inum == 0) {
        printk("fopen failed: no such file.\n");
        return -1;
    }

    for (int i = 0; i < NUM_FDESCS; i++) {
        if (fdesc_array[i].inum == 0) {
            fdesc_array[i].inum = inum;
            switch (mode) {
                case O_RDONLY:
                    fdesc_array[i].read = 1;
                    fdesc_array[i].write = 0;
                    break;
                case O_WRONLY:
                    fdesc_array[i].read = 0;
                    fdesc_array[i].write = 1;
                    break;
                case O_RDWR:
                    fdesc_array[i].read = 1;
                    fdesc_array[i].write = 1;
                    break;
            }
            return i;
        }
    }

    return -1;  // return the id of file descriptor
}

int do_fread(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fread
    fdesc_t *fdesc = &fdesc_array[fd];
    if (!fdesc->read) {
        printk("fread failed: no read permission!\n");
        return -1;
    }
    uint32_t offset = fdesc->offset;

    inode_t inode;
    read_inode(fdesc->inum, &inode);

    int bytes_read = 0;
    while (length > 0) {
        int blk_num = offset_to_blk(offset, &inode);
        if (blk_num == 0) {
            printk("fread failed: block not allocted.\n");
            break;
        }
        
        int inblk_offset = offset % NBYTES_PER_BLOCK;

        // decide the length of content read from SD card at one time
        int len = (inblk_offset + length > NBYTES_PER_BLOCK) ? NBYTES_PER_BLOCK - inblk_offset: length;
        read_block(blk_num);
        memcpy((uint8_t *)buff, (uint8_t *)&block_buf[inblk_offset], len);

        length -= len;
        offset += len, bytes_read += len;
    }
    fdesc->offset += bytes_read;
    return bytes_read;  // return the length of trully read data
}

int do_fwrite(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fwrite
    fdesc_t *fdesc = &fdesc_array[fd];
    if (!fdesc->write) {
        printk("fwrite failed: no write permission!\n");
        return -1;
    }
    uint32_t offset = fdesc->offset;

    inode_t inode;
    read_inode(fdesc->inum, &inode);
    if (offset + length > inode.size) {
        inode.size = offset + length;
    }
    fdesc->offset = offset + length;

    while (length > 0) {
        int blk_num = offset_to_blk(offset, &inode);
        if (blk_num == 0) {
            blk_num = alloc_block();
            set_inode_blk_num(offset, blk_num, &inode, fdesc->inum);
            memset(block_buf, 0, NBYTES_PER_BLOCK);
        } else {
            read_block(blk_num);
        }
        int inblk_offset = offset % NBYTES_PER_BLOCK;

        // decide the length of content written to SD card at one time
        int len = (inblk_offset + length > NBYTES_PER_BLOCK) ? NBYTES_PER_BLOCK - inblk_offset: length;

        memcpy((uint8_t *)&block_buf[inblk_offset], (uint8_t *)buff, len);
        write_block(blk_num);

        length -= len;
        offset += len;
    }
    write_inode(fdesc->inum, &inode);
    return length;  // return the length of trully written data
}

int do_fclose(int fd)
{
    // TODO [P6-task2]: Implement do_fclose
    memset((uint8_t *)&fdesc_array[fd], 0, sizeof(fdesc_t));
    return 0;  // do_fclose succeeds
}

int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln
    if (strcmp(src_path, dst_path) == 0) {
        printk("ln failed: hard link name is same as original file.\n");
        return 1;
    }

    int src_inum = parse_path(src_path, current_running->cwd_inum);
    inode_t src_inode;
    read_inode(src_inum, &src_inode);
    src_inode.ref++;

    inode_t cwd_inode;
    int cwd_inum = current_running->cwd_inum;
    read_inode(cwd_inum, &cwd_inode);

    uint32_t block_num;
    for (int i = 0; i < NDIRECT_BLOCKS; i++) {
        block_num = cwd_inode.direct[i];
        if (block_num == 0) break;

        read_block(block_num);
        dentry_t *dentries = (dentry_t *)block_buf;
        for (int j = 0; j < NDENTRIES_PER_BLOCK; j++) {
            if (dentries[j].inum == 0) {
                goto blk_rd_finish;
            }
        }
    }

    dentry_t *link_dentry = NULL;
blk_rd_finish:
    link_dentry = (dentry_t *)block_buf;
    while (link_dentry->inum != 0) {
        link_dentry++;
    }

    link_dentry->inum = src_inum;
    strcpy(link_dentry->name, dst_path);
    link_dentry->valid = 1;
    write_block(block_num);
    write_inode(src_inum, &src_inode);

    return 0;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm
    // FIXME: add multi-level directory support
    int file_inum = parse_path(path, current_running->cwd_inum);
    if (file_inum == 0) {
        printk("rm failed: no such file.\n");
        return 1;
    }
    inode_t file_inode;
    read_inode(file_inum, &file_inode);
    if (file_inode.type != FILE) {
        printk("rm failed: not a file.\n");
        return 1;
    }

    int cwd_inum = current_running->cwd_inum;
    inode_t cwd_inode;
    read_inode(cwd_inum, &cwd_inode);
    file_inode.ref--;

    // delete dentry of the file
    uint32_t block_num;
    for (int i = 0; i < 10; i++) {
        block_num = cwd_inode.direct[i];
        if (block_num == 0) break;

        read_block(block_num);
        dentry_t *dentries = (dentry_t *)block_buf;
        for (int j = 0; j < NDENTRIES_PER_BLOCK; j++) {
            if (dentries[j].valid && strcmp(dentries[j].name, path) == 0) {
                dentries[j].valid = 0;
                goto blk_rd_finish;
            }
        }
    }
blk_rd_finish:
    write_block(block_num);

    if (file_inode.ref > 0) {
        write_inode(file_inum, &file_inode);
        return 0;
    }

    free_inode(file_inum);
    
    // FIXME: add indirect block support
    for (int i = 0; i < NDIRECT_BLOCKS; i++) {
        free_block(file_inode.direct[i]);
    }

    return 0;  // do_rm succeeds 
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO: [P6-task2]: Implement do_lseek
    fdesc_t *fdesc = &fdesc_array[fd];
    int cur_offset = fdesc->offset;

    inode_t inode;
    read_inode(fdesc->inum, &inode);
    int file_size = inode.size;

    // the resulting offset location from the beginning of the file
    int result_offset;
    switch (whence) {
        case SEEK_SET:
            if (offset >= 0) {
                result_offset = (offset > file_size) ? file_size : offset;
            } else {
                result_offset = 0;
            }
            break;
        case SEEK_CUR:
            if (offset >= 0) {
                result_offset = (offset + cur_offset > file_size) ? file_size : offset + cur_offset;
            } else {
                result_offset = (offset + cur_offset >= 0) ? offset + cur_offset : 0;
            }
            break;
        case SEEK_END:
            if (offset >= 0) {
                result_offset = file_size;
            } else {
                result_offset = (file_size + offset >= 0) ? file_size + offset : 0;
            }
            break;
    }

    fdesc->offset = result_offset;
    return result_offset;
}
