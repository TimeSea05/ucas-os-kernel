#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* Initialize tx descriptors */
    for (int i = 0; i < TXDESCS; i++) {
        tx_desc_array[i].addr = kva2pa((uint64_t)tx_pkt_buffer[i]);
        // set CMD.DEXT to 0 => legacy
        tx_desc_array[i].cmd = 0;
        tx_desc_array[i].cmd |= E1000_TXD_CMD_RS;     // enable CMD.RS => validate STATUS
        tx_desc_array[i].status |= E1000_TXD_STAT_DD;
    }

    /* Set up the Tx descriptor base address and length */
    uint64_t tx_desc_array_pa = kva2pa((uint64_t)tx_desc_array);
    uint32_t tx_desc_array_pa_low = (uint32_t)(tx_desc_array_pa & 0xffffffff);
    uint32_t tx_desc_array_pa_high = (uint32_t)(tx_desc_array_pa >> 32);

    e1000_write_reg(e1000, E1000_TDBAL, tx_desc_array_pa_low);
    e1000_write_reg(e1000, E1000_TDBAH, tx_desc_array_pa_high);
    e1000_write_reg(e1000, E1000_TDLEN, sizeof(tx_desc_array));

	/* Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* Program the Transmit Control Register */
    // TCTL.EN => 1, TCTL.PSP => 1, TCTL.CT => 0x10, TCTL.COLD => 0x40
    #define E1000_TCTL_CT_OFFSET   4
    #define E1000_TCTL_COLD_OFFSET 12
    uint32_t tctl_val = E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << E1000_TCTL_CT_OFFSET)
                      | (0x40 << E1000_TCTL_COLD_OFFSET);
    e1000_write_reg(e1000, E1000_TCTL, tctl_val);

    local_flush_dcache();
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* Set e1000 MAC Address to RAR[0] */
    uint32_t ral = 0, rah = 0;
    for (int i = 0; i < 4; i++) ral |= (enetaddr[i] << (8*i));
    for (int i = 4; i < 6; i++) rah |= (enetaddr[i] << (8*(i-4)));
    rah |= E1000_RAH_AV;
    e1000_write_reg_array(e1000, E1000_RA, 0, ral);
    e1000_write_reg_array(e1000, E1000_RA, 1, rah);
    
    /* Initialize rx descriptors */
    for (int i = 0; i < TXDESCS; i++) {
        rx_desc_array[i].addr = kva2pa((uint64_t)rx_pkt_buffer[i]);
    }

    /* Set up the Rx descriptor base address and length */
    uint64_t rx_desc_array_pa = kva2pa((uint64_t)rx_desc_array);
    uint32_t rx_desc_array_pa_low = (uint32_t)(rx_desc_array_pa & 0xffffffff);
    uint32_t rx_desc_array_pa_high = (uint32_t)(rx_desc_array_pa >> 32);

    e1000_write_reg(e1000, E1000_RDBAL, rx_desc_array_pa_low);
    e1000_write_reg(e1000, E1000_RDBAH, rx_desc_array_pa_high);
    e1000_write_reg(e1000, E1000_RDLEN, sizeof(rx_desc_array));

    /* Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);

    /* Program the Receive Control Register */
    uint32_t rctl_val = 0;
    rctl_val |= E1000_RCTL_EN;  // enable receiving
    rctl_val |= E1000_RCTL_BAM; // enable receiving broadcast frames
    e1000_write_reg(e1000, E1000_RCTL, rctl_val);

    local_flush_dcache();
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* Transmit one packet from txpacket */
    local_flush_dcache();
    uint32_t tail = e1000_read_reg(e1000, E1000_TDT);
    struct e1000_tx_desc *tail_tx_desc = &tx_desc_array[tail];

    if (!(tx_desc_array[tail].status & E1000_TXD_STAT_DD)) {
        return 0;
    }

    tail_tx_desc->length = length;
    tail_tx_desc->status &= ~E1000_TXD_STAT_DD;
    tail_tx_desc->cmd    |= E1000_TXD_CMD_EOP;

    memcpy((uint8_t *)tx_pkt_buffer[tail], txpacket, length);

    uint32_t next = (tail + 1) % TXDESCS;
    e1000_write_reg(e1000, E1000_TDT, next);

    local_flush_dcache();
    return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* Receive one packet and put it into rxbuffer */
    local_flush_dcache();
    uint32_t tail = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS;
    if (!(rx_desc_array[tail].status & E1000_RXD_STAT_DD)) {
        return 0;
    }

    struct e1000_rx_desc *tail_rx_desc = &rx_desc_array[tail];
    uint32_t len = tail_rx_desc->length;
    memcpy(rxbuffer, (uint8_t *)(rx_pkt_buffer[tail]), len);

    rx_desc_array[tail].status &= (~(E1000_RXD_STAT_DD));
    e1000_write_reg(e1000, E1000_RDT, tail);
    local_flush_dcache();

    return len;
}

int is_tx_desc_stat_dd() {
    local_flush_dcache();
    uint32_t tail = e1000_read_reg(e1000, E1000_TDT);
    if (tx_desc_array[tail].status & E1000_TXD_STAT_DD) {
        return 1;
    }
    return 0;
}

int is_rx_desc_stat_dd() {
    local_flush_dcache();
    uint32_t tail = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS;
    if (rx_desc_array[tail].status & E1000_RXD_STAT_DD) {
        return 1;
    }
    return 0;
}