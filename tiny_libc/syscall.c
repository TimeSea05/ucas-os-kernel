#include <syscall.h>
#include <stdint.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    long res;
    asm volatile(
        "mv a7, a0;"
        "mv a0, a1;"
        "mv a1, a2;"
        "mv a2, a3;"
        "mv a3, a4;"
        "mv a4, a5;"
        "ecall;"
        "mv %[RESULT], a0;"
        : [RESULT]"=g"(res)
    );

    return res;
}

void sys_yield(void)
{
    invoke_syscall(SYSCALL_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor(int x, int y)
{
    invoke_syscall(SYSCALL_CURSOR, x, y, IGNORE, IGNORE, IGNORE);
}

void sys_write(char *buff)
{
    invoke_syscall(SYSCALL_WRITE, (long)buff, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_clear() {
    invoke_syscall(SYSCALL_CLEAR, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_backspace(int prompt_len) {
    invoke_syscall(SYSCALL_BACKSPACE, (long)prompt_len, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_reflush(void)
{
    invoke_syscall(SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mutex_init(int key)
{
    return invoke_syscall(SYSCALL_LOCK_INIT, key, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_acquire(int mutex_idx)
{
    invoke_syscall(SYSCALL_LOCK_ACQ, mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_release(int mutex_idx)
{
    invoke_syscall(SYSCALL_LOCK_RELEASE, mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

long sys_get_timebase(void)
{
    return invoke_syscall(SYSCALL_GET_TIMEBASE, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

long sys_get_tick(void)
{
    return invoke_syscall(SYSCALL_GET_TICK, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_sleep(uint32_t time)
{
    invoke_syscall(SYSCALL_SLEEP, time, IGNORE, IGNORE, IGNORE, IGNORE);
}

pthread_t sys_thread_create(void (*start_routine)(void *), void *arg) {
    return invoke_syscall(SYSCALL_THREAD_CREATE, (long)start_routine, (long)arg, IGNORE, IGNORE, IGNORE);
}

pthread_t sys_thread_join(pthread_t thread) {
    return invoke_syscall(SYSCALL_THREAD_JOIN, (long)thread, IGNORE, IGNORE, IGNORE, IGNORE);
}

pid_t sys_exec(char *name, int argc, char **argv)
{
    return invoke_syscall(SYSCALL_EXEC, (long)name, (long)argc, (long)argv, IGNORE, IGNORE);
}


void sys_exit(void)
{
    invoke_syscall(SYSCALL_EXIT, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int  sys_kill(pid_t pid)
{
    return invoke_syscall(SYSCALL_KILL, (long)pid, IGNORE, IGNORE, IGNORE, IGNORE);
}

int  sys_waitpid(pid_t pid)
{
    return invoke_syscall(SYSCALL_WAITPID, (long)pid, IGNORE, IGNORE, IGNORE, IGNORE);
}


void sys_ps(void)
{
    invoke_syscall(SYSCALL_PS, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

pid_t sys_getpid()
{
    return invoke_syscall(SYSCALL_GETPID, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int  sys_getchar(void)
{
    return invoke_syscall(SYSCALL_READCH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int  sys_barrier_init(int key, int goal)
{
    return invoke_syscall(SYSCALL_BARR_INIT, (long)key, (long)goal, IGNORE, IGNORE, IGNORE);
}

void sys_barrier_wait(int bar_idx)
{
    invoke_syscall(SYSCALL_BARR_WAIT, (long)bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_barrier_destroy(int bar_idx)
{
    invoke_syscall(SYSCALL_BARR_DESTROY, (long)bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_condition_init(int key)
{
    return invoke_syscall(SYSCALL_COND_INIT, (long)key, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    invoke_syscall(SYSCALL_COND_WAIT, (long)cond_idx, (long)mutex_idx, IGNORE, IGNORE, IGNORE);
}

void sys_condition_signal(int cond_idx)
{
    invoke_syscall(SYSCALL_COND_SIGNAL, (long)cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_broadcast(int cond_idx)
{
    invoke_syscall(SYSCALL_COND_BROADCAST, (long)cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_destroy(int cond_idx)
{
    invoke_syscall(SYSCALL_COND_DESTROY, (long)cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mbox_open(char * name)
{
    return invoke_syscall(SYSCALL_MBOX_OPEN, (long)name, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mbox_close(int mbox_id)
{
    invoke_syscall(SYSCALL_MBOX_CLOSE, (long)mbox_id, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    return invoke_syscall(SYSCALL_MBOX_SEND, (long)mbox_idx, (long)msg, (long)msg_length, IGNORE, IGNORE);
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    return invoke_syscall(SYSCALL_MBOX_RECV, (long)mbox_idx, (long)msg, (long)msg_length, IGNORE, IGNORE);
}

void* sys_shmpageget(int key)
{
    return (void *)invoke_syscall(SYSCALL_SHM_GET, (long)key, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_shmpagedt(void *addr)
{
    invoke_syscall(SYSCALL_SHM_DT, (long)addr, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_net_send(void *txpacket, int length)
{
    return invoke_syscall(SYSCALL_NET_SEND, (long)txpacket, (long)length, IGNORE, IGNORE, IGNORE);
}

int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    return invoke_syscall(SYSCALL_NET_RECV, (long)rxbuffer, (long)pkt_num, (long)pkt_lens, IGNORE, IGNORE);
}

int sys_mkfs(void)
{
    // TODO [P6-task1]: Implement sys_mkfs
    return invoke_syscall(SYSCALL_FS_MKFS, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_statfs(void)
{
    // TODO [P6-task1]: Implement sys_statfs
    return invoke_syscall(SYSCALL_FS_STATFS, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_cd(char *path)
{
    // TODO [P6-task1]: Implement sys_cd
    return invoke_syscall(SYSCALL_FS_CD, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mkdir(char *path)
{
    // TODO [P6-task1]: Implement sys_mkdir
    return invoke_syscall(SYSCALL_FS_MKDIR, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_rmdir(char *path)
{
    // TODO [P6-task1]: Implement sys_rmdir
    return invoke_syscall(SYSCALL_FS_RMDIR, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement sys_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    return invoke_syscall(SYSCALL_FS_LS, (long)path, (long)option, IGNORE, IGNORE, IGNORE);
}

int sys_touch(char *path)
{
    // TODO [P6-task2]: Implement sys_touch
    return invoke_syscall(SYSCALL_FS_TOUCH, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_cat(char *path)
{
    // TODO [P6-task2]: Implement sys_cat
    return invoke_syscall(SYSCALL_FS_CAT, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_fopen(char *path, int mode)
{
    // TODO [P6-task2]: Implement sys_fopen
    return invoke_syscall(SYSCALL_FS_FOPEN, (long)path, (long)mode, IGNORE, IGNORE, IGNORE);
}

int sys_fread(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement sys_fread
    return invoke_syscall(SYSCALL_FS_FREAD, (long)fd, (long)buff, (long)length, IGNORE, IGNORE);
}

int sys_fwrite(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement sys_fwrite
    return invoke_syscall(SYSCALL_FS_FWRITE, (long)fd, (long)buff, (long)length, IGNORE, IGNORE);
}

int sys_fclose(int fd)
{
    // TODO [P6-task2]: Implement sys_fclose
    return invoke_syscall(SYSCALL_FS_FCLOSE, (long)fd, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement sys_ln
    return invoke_syscall(SYSCALL_FS_LN, (long)src_path, (long)dst_path, IGNORE, IGNORE, IGNORE);
}

int sys_rm(char *path)
{
    // TODO [P6-task2]: Implement sys_rm
    return invoke_syscall(SYSCALL_FS_RM, (long)path, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement sys_lseek
    return invoke_syscall(SYSCALL_FS_LSEEK, (long)fd, (long)offset, (long)whence, IGNORE, IGNORE);
}
