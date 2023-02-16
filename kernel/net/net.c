#include <e1000.h>
#include <type.h>
#include <os/net.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    // Transmit one network packet via e1000 device
    // Call do_block when e1000 transmit queue is full
    while (e1000_transmit(txpacket, length) == 0) {
        do_block(&current_running->list, &send_block_queue);
    }

    return length;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // Receive one network packet via e1000 device
    // Call do_block when there is no packet on the way

    int bytes = 0, len = 0;
    for (int i = 0; i < pkt_num; i++) {
        while ((len = e1000_poll(rxbuffer)) == 0) {
            do_block(&current_running->list, &recv_block_queue);
        }

        *pkt_lens = len;
        pkt_lens++;
        bytes += len;

        rxbuffer = (void *)((uint64_t)rxbuffer + len);
    }


    return bytes;  // Bytes it has received
}

void check_net_send() {
    if (is_tx_desc_stat_dd()) {
        pcb_t *p, *p_q;
        list_for_each_entry_safe(p, p_q, &send_block_queue) {
            do_unblock(&p->list);
        }
    }
}

void check_net_recv() {
    if (is_rx_desc_stat_dd()) {
        pcb_t *p, *p_q;
        list_for_each_entry_safe(p, p_q, &recv_block_queue) {
            do_unblock(&p->list);
        }
    }
}
