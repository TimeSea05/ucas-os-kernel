#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
barrier_t barriers[BARRIER_NUM];
condition_t conditions[CONDITION_NUM];
mailbox_t mailboxes[MBOX_NUM];

void init_locks(void)
{
    for (int i = 0; i < LOCK_NUM; i++) {
        spin_lock_init(&mlocks[i].lock);
        INIT_LIST_HEAD(&mlocks[i].block_queue);
        mlocks[i].pid = 0;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    return atomic_swap(LOCKED, (ptr_t)lock);
}

void spin_lock_acquire(spin_lock_t *lock)
{
    while (spin_lock_try_acquire(lock) == LOCKED);
}

void spin_lock_release(spin_lock_t *lock)
{
    atomic_swap(UNLOCKED, (ptr_t)lock);
}

int do_mutex_lock_init(int key)
{
    int index = key % LOCK_NUM;
    mlocks[index].key = key;

    return index;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    mutex_lock_t *mutex = &mlocks[mlock_idx];

    if (spin_lock_try_acquire(&mutex->lock) == UNLOCKED) {
        mutex->pid = current_running->pid;
        return;
    } else do_block(&current_running->list, &mutex->block_queue);
}

void do_mutex_lock_release(int mlock_idx)
{
    mutex_lock_t *mutex = &mlocks[mlock_idx];
    
    if (is_queue_empty(&mutex->block_queue)) {
        spin_lock_release(&mutex->lock);
        mutex->pid = 0;
    } else {
        list_node_t *pcb_node = mutex->block_queue.next;
        pcb_t *p = list_entry(pcb_node, pcb_t);
        mutex->pid = p->pid;

        do_unblock(pcb_node);
    }
}

void init_barriers() {
    for (int i = 0; i < BARRIER_NUM; i++) {
        barriers[i].goal = 0, barriers[i].current = 0;
        INIT_LIST_HEAD(&barriers[i].block_list);
    }
}

int do_barrier_init(int key, int goal) {
    int bar_idx = key % BARRIER_NUM;
    barriers[bar_idx].key = key;
    barriers[bar_idx].goal = goal;

    return bar_idx;
}

void do_barrier_wait(int bar_idx) {
    barrier_t *bar = &barriers[bar_idx];
    
    bar->current++;
    if (bar->current < bar->goal) {
        do_block(&current_running->list, &bar->block_list);
    } else {
        while (!is_queue_empty(&bar->block_list)) {
            do_unblock(bar->block_list.next);
        }
        bar->current = 0;
    }
}

void do_barrier_destroy(int bar_idx) {
    barrier_t *bar = &barriers[bar_idx];
    bar->current = 0, bar->current = 0, bar->key = 0;
    while (!is_queue_empty(&bar->block_list)) {
        list_delete_entry(bar->block_list.next);
    }
}

void init_conditions() {
    for (int i = 0; i < CONDITION_NUM; i++) {
        conditions[i].key = 0;
        INIT_LIST_HEAD(&conditions[i].wait_list);
    }
}

int do_condition_init(int key) {
    int idx = key % CONDITION_NUM;
    conditions[idx].key = key;
    return idx;
}

void do_condition_wait(int cond_idx, int mutex_idx) {
    condition_t *cond = &conditions[cond_idx];
    do_mutex_lock_release(mutex_idx);
    do_block(&current_running->list, &cond->wait_list);
    do_mutex_lock_acquire(mutex_idx);
}

void do_condition_signal(int cond_idx) {
    condition_t *cond = &conditions[cond_idx];
    if (!is_queue_empty(&cond->wait_list)) {
        list_node_t *gonna_unblock = cond->wait_list.next;
        do_unblock(gonna_unblock);
    }
}

void do_condition_broadcast(int cond_idx) {
    condition_t *cond = &conditions[cond_idx];
    while (!is_queue_empty(&cond->wait_list)) {
        list_node_t *gonna_unblock = cond->wait_list.next;
        do_unblock(gonna_unblock);
    }
}

void do_condition_destroy(int cond_idx) {
    condition_t *cond = &conditions[cond_idx];
    cond->key = 0;
    while (!is_queue_empty(&cond->wait_list)) {
        list_node_t *gonna_unblock = cond->wait_list.next;
        do_unblock(gonna_unblock);
    }
}

void init_mbox() {
    for (int i = 0; i < MBOX_NUM; i++) {
        mailbox_t *mbox = &mailboxes[i];
        bzero(mbox->name, 32);
        bzero(mbox->buf, MAX_MBOX_LENGTH);

        mbox->ref = 0, mbox->nread = 0, mbox->nwrite = 0;
        mbox->round_read = 0, mbox->round_write = 0;
        mbox->mutex_id = -1;
        INIT_LIST_HEAD(&mbox->send_list);
        INIT_LIST_HEAD(&mbox->recv_list);
    }
}

int do_mbox_open(char *name) {
    mailbox_t *mbox = NULL;
    int mbox_idx = -1;

    // try to find a mailbox with given name
    for (int i = 0; i < MBOX_NUM; i++) {
        if (strcmp(name, mailboxes[i].name) == 0) {
            mbox = &mailboxes[i];
            mbox_idx = i;
            break;
        }
    }

    // mailbox with given name not found
    // find a mbox not used
    if (mbox == NULL) {
        for (int i = 0; i < MBOX_NUM; i++) {
            if (mailboxes[i].ref == 0) {
                mbox = &mailboxes[i];
                mbox_idx = i;
                strcpy(mbox->name, name);

                // find a mutex for this mailbox
                mbox->mutex_id = mbox_idx;
                break;
            }
        }
    }
    
    // cannot not find a mbox not used
    // open mailbox failed, return -1
    if (mbox == NULL) {
        return -1;
    }

    mbox->ref++;
    return mbox_idx;
}

void do_mbox_close(int mbox_idx) {
    mailbox_t *mbox = &mailboxes[mbox_idx];
    mbox->ref--;

    // no process is using this mailbox, release it
    if (mbox->ref == 0) {
        bzero(mbox->name, 32);
        bzero(mbox->buf, MAX_MBOX_LENGTH);

        do_mutex_lock_release(mbox->mutex_id);
        mbox->mutex_id = -1;
        mbox->nread = 0, mbox->nwrite = 0;
        mbox->round_read = 0, mbox->round_write = 0;
    }
}

static int can_write_to_buffer(mailbox_t *mbox, int msg_len) {
    int written = (mbox->round_write - mbox->round_read) * MAX_MBOX_LENGTH + (mbox->nwrite - mbox->nread);
    return msg_len + written <= MAX_MBOX_LENGTH;
}

static int can_read_from_buffer(mailbox_t *mbox, int msg_len) {
    int written = (mbox->round_write - mbox->round_read) * MAX_MBOX_LENGTH + (mbox->nwrite - mbox->nread);
    return written >= msg_len;
}

int do_mbox_send(int mbox_idx, void * msg, int msg_length) {
    mailbox_t *mbox = &mailboxes[mbox_idx];
    int block_count = 0;

    // first accquire the mutex
    int mutex_id = mbox->mutex_id;
    do_mutex_lock_acquire(mutex_id);

    // check if current running process can send message to the buffer
    // if not, block this process in send_list
    while (!can_write_to_buffer(mbox, msg_length)) {
        do_mutex_lock_release(mutex_id);
        block_count++;
        do_block(&current_running->list, &mbox->send_list);
        do_mutex_lock_acquire(mutex_id);
    }

    // copy message to the buffer
    for (int i = 0; i < msg_length; i++) {
        mbox->buf[mbox->nwrite++] = *(char *)(msg + i);
        if (mbox->nwrite >= MAX_MBOX_LENGTH) {
            mbox->nwrite %= MAX_MBOX_LENGTH;
            mbox->round_write++;
        }
    }

    // wakeup processes waiting on recv_list
    while (!is_queue_empty(&mbox->recv_list)) {
        do_unblock(mbox->recv_list.next);
    }

    // release the mutex then return
    do_mutex_lock_release(mutex_id);
    return block_count;
}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length) {
    mailbox_t *mbox = &mailboxes[mbox_idx];
    int block_count = 0;
    
    // first accquire the mutex
    int mutex_id = mbox->mutex_id;
    do_mutex_lock_acquire(mutex_id);

    // check if current running process can read message from the buffer
    // if not, block this process in recv_list
    while (!can_read_from_buffer(mbox, msg_length)) {
        do_mutex_lock_release(mutex_id);
        block_count++;
        do_block(&current_running->list, &mbox->recv_list);
        do_mutex_lock_acquire(mutex_id);
    }

    // read message from the buffer
    char *msg_arr = (char *)msg;
    for (int i = 0; i < msg_length; i++) {
        msg_arr[i] = mbox->buf[mbox->nread++];
        mbox->nread++;
        if (mbox->nread >= MAX_MBOX_LENGTH) {
            mbox->nread %= MAX_MBOX_LENGTH;
            mbox->round_read++;
        }
    }

    // wakeup processes waiting on send_list
    while (!is_queue_empty(&mbox->send_list)) {
        do_unblock(mbox->send_list.next);
    }

    // release the mutex then return
    do_mutex_lock_release(mutex_id);
    return block_count;
}
