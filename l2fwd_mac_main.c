/*-thread created and in which two seperate threads for updation and checking are created infinately

	final working code


 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_arp.h>

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF   8192

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
 
#define default_aging_time 30
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
/*
 * Work-around of a compilation error with ICC on invocations of the
 * rte_be_to_cpu_16() function.
 */
#ifdef __GCC__
#define RTE_BE_TO_CPU_16(be_16_v)  rte_be_to_cpu_16((be_16_v))
#define RTE_CPU_TO_BE_16(cpu_16_v) rte_cpu_to_be_16((cpu_16_v))
#else
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
#define RTE_BE_TO_CPU_16(be_16_v)  (be_16_v)
#define RTE_CPU_TO_BE_16(cpu_16_v) (cpu_16_v)
#else
#define RTE_BE_TO_CPU_16(be_16_v) \
        (uint16_t) ((((be_16_v) & 0xFF) << 8) | ((be_16_v) >> 8))
#define RTE_CPU_TO_BE_16(cpu_16_v) \
        (uint16_t) ((((cpu_16_v) & 0xFF) << 8) | ((cpu_16_v) >> 8))
#endif
#endif /* __GCC__ */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask = 0;
static int64_t l2fwd_user_defined_dst_port = 0;
static int16_t l2fwd_mac_learning_enabled = 0;
//static int16_t l2fwd_mac_learning_enabled = 1;
uint64_t total_duplicate_packets_dropped = 0;

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];

static unsigned int l2fwd_rx_queue_per_lcore = 1;

void add_mac_entry(struct ether_hdr *eth,uint32_t source_ip,unsigned port);
static void l2fwd_mac_ports_update(struct ether_hdr *eth,unsigned rxport);
static void l2fwd_mac_display(void);
static void l2fwd_show_mac_entry(void);
void source_mac_table(struct ether_hdr *eth,uint32_t source_ip,unsigned port);
void add_src_mac_entry(struct ether_hdr *eth,uint32_t source_ip,unsigned port);
void update_aging(struct ether_hdr *eth);
void *thread1(void);
void *thread2(void);
void *threadInit(void);


struct node
{
        unsigned port;
        struct ether_addr s_addr; /**< Source address. */
	struct ether_addr d_addr; /**< Destination address. */
	uint32_t source_ip;
	time_t starttime;
        struct node *next;
}*head = NULL;

struct mac_list
{
        unsigned port;
        struct ether_addr s_addr;
        uint32_t source_ip;
	time_t rx_time;
	char *buff;
	int aging_time;
        struct mac_list *next;
}*start = NULL;


struct mbuf_table {
	unsigned len;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

void update_node(struct mac_list *ptr);
#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
	struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];

} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		//.header_split   = 0, /**< Header Split disabled */
		//.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		//.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		//.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		//.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

struct rte_mempool * l2fwd_pktmbuf_pool = NULL;

/* Per-port statistics struct */
struct l2fwd_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
} __rte_cache_aligned;
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];

/* A tsc-based timer responsible for triggering statistics printout */
#define TIMER_MILLISECOND 2000000ULL /* around 1ms at 2 Ghz */
#define MAX_TIMER_PERIOD 86400 /* 1 day max */
static int64_t timer_period = 10 * TIMER_MILLISECOND * 1000; /* default period is 10 seconds */

static inline void
print_ether_addr(const char *what, struct ether_addr *eth_addr)
{
        char buf[ETHER_ADDR_FMT_SIZE];
        ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
        printf("%s%s", what, buf);
}

static void
ipv4_addr_to_dot(uint32_t be_ipv4_addr, char *buf)
{
        uint32_t ipv4_addr;

        ipv4_addr = rte_be_to_cpu_32(be_ipv4_addr);
        sprintf(buf, "%d.%d.%d.%d", (ipv4_addr >> 24) & 0xFF,
                (ipv4_addr >> 16) & 0xFF, (ipv4_addr >> 8) & 0xFF,
                ipv4_addr & 0xFF);
}

static void
ipv4_addr_dump(const char *what, uint32_t be_ipv4_addr)
{
        char buf[16];

        ipv4_addr_to_dot(be_ipv4_addr, buf);
        if (what)
                printf("%s", what);
        printf("%s", buf);
}

void add_mac_entry(struct ether_hdr *eth, uint32_t source_ip,unsigned port)
{
	struct node *new = NULL;
	new = (struct node*)malloc(sizeof(struct node));
	if(new == NULL)
	{
		printf("Memory not allocated for MAC database");
	}

	source_mac_table(eth,source_ip,port);

	new->port = port;
	new->starttime = time(NULL);
	ether_addr_copy(&eth->s_addr, &new->s_addr);
	ether_addr_copy(&eth->d_addr, &new->d_addr);
	new->source_ip=source_ip; // 2018
	if(head == NULL)
	{
		printf("head == NULL \n");
		head = new;
		head->next = NULL;
		
	}
	else
	{
		printf("head != NULL \n");
		new->next = head;
		head = new;
		
	}
}

void source_mac_table(struct ether_hdr *eth,uint32_t source_ip,unsigned port)
{

        struct mac_list *ptr = NULL;
        ptr = start;
        if(start == NULL)
        {
                printf("Adding entry to the table\n");
                add_src_mac_entry(eth,source_ip,port);
        }
        else
        {
                while(ptr != NULL)
                {
                        if (memcmp(&eth->s_addr, &ptr->s_addr, ETHER_ADDR_LEN) == 0)
                        {
							
     				ptr->rx_time=time(NULL);
				ptr->aging_time=default_aging_time;
                                if(ptr->port != port )
                                {
                                        ptr->port = port;
        	                           break;
	                         }
                                else
                                        break; // received from same mac
                        }
                        ptr = ptr->next;
                }
                if(ptr == NULL)
                {
                        /*If the mac entry is not found in the table, then add the entry 
                        to the mac table */
                        printf("Adding MAC entry to table\n");
                        add_src_mac_entry(eth,source_ip,port);
                }

        }
}

void add_src_mac_entry(struct ether_hdr *eth,uint32_t source_ip,unsigned port)
{
        struct mac_list *new = NULL;
        new = (struct mac_list*)malloc(sizeof(struct mac_list));
        if(new == NULL)
        {
                printf("Memory not allocated for MAC database");
        }
        new->port = port;
        ether_addr_copy(&eth->s_addr, &new->s_addr);
        // 2018 , copy IP address here ??
        new->source_ip=source_ip;
        
	new->rx_time=time(NULL);
	new->aging_time=default_aging_time;
	if(start == NULL)
        {
        	printf("start == NULL\n"); 
                start = new;
                start->next = NULL;
        }
        else
        {
        	printf("start != NULL\n"); 
		new->next=start;
		start=new;               
        }

}

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	unsigned portid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };

		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");

	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		/* skip disabled ports */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nPackets dropped: %21"PRIu64,
			   portid,
			   port_statistics[portid].tx,
			   port_statistics[portid].rx,
			   port_statistics[portid].dropped);

		total_packets_dropped += port_statistics[portid].dropped;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += port_statistics[portid].rx;
	}
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18"PRIu64
		   "\nTotal packets received: %14"PRIu64
		   "\nTotal packets dropped: %15"PRIu64,
		   total_packets_tx,
		   total_packets_rx,
		   total_packets_dropped);
	printf("\nTotal duplicate packets dropped: %5"PRIu64,
		   total_duplicate_packets_dropped);
	printf("\n====================================================\n");
}


/* Send the burst of packets on an output interface */
static int
l2fwd_send_burst(struct lcore_queue_conf *qconf, unsigned n, uint8_t port)
{
	struct rte_mbuf **m_table;
	unsigned ret;
	unsigned queueid =0;

	m_table = (struct rte_mbuf **)qconf->tx_mbufs[port].m_table;

	ret = rte_eth_tx_burst(port, (uint16_t) queueid, m_table, (uint16_t) n);
	port_statistics[port].tx += ret;
	if (unlikely(ret < n)) {
		port_statistics[port].dropped += (n - ret);
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ret < n);
	}

	return 0;
}

/* Enqueue packets for TX and prepare them to be sent */
static int
l2fwd_send_packet(struct rte_mbuf *m, uint8_t port)
{
	unsigned lcore_id, len;
	struct lcore_queue_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_queue_conf[lcore_id];
	len = qconf->tx_mbufs[port].len;
	qconf->tx_mbufs[port].m_table[len] = m;
	len++;

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {
		l2fwd_send_burst(qconf, MAX_PKT_BURST, port);
		len = 0;
	}

	qconf->tx_mbufs[port].len = len;
	return 0;
}
static void l2fwd_mac_display(void)
{
	struct node *node = NULL;
	printf("\n==============DEBUGGING MAC TABLE=================\n");
        printf("        SMAC			DMAC			PORT \n");
        node = head;
        while(node != NULL)
        {
        	print_ether_addr("", &node->s_addr);
                print_ether_addr("	",&node->d_addr);                
		printf("		%d\n",node->port);
		node = node->next;
        }
        printf("========================================\n");
}
static void l2fwd_show_mac_entry(void)
{
        struct mac_list *node = NULL;
        printf("\n==============MAC TABLE=================\n");
        printf("        MAC                     PORT\taging time\n");
        node = start;
        while(node != NULL)
        {
                print_ether_addr("", &node->s_addr);
                printf("                %d\t",node->port);
        	printf("%d\n",node->aging_time);
		
	        node = node->next;
        }
        printf("========================================\n");
}
static void l2fwd_mac_ports_update(struct ether_hdr *eth,unsigned rxport)
{
	struct node *ptr = NULL;
	struct mac_list *ptr1 = NULL;
	
        ptr = head;
        while(ptr != NULL)
        {
		if (memcmp(&eth->s_addr, &ptr->s_addr, ETHER_ADDR_LEN) == 0)
		{
			ptr->port = rxport;
		}
                ptr = ptr->next;
        }
	
	ptr1 = start;
        while(ptr1 != NULL)
        {
		if (memcmp(&eth->s_addr, &ptr1->s_addr, ETHER_ADDR_LEN) == 0)
		{
			ptr1->port = rxport;
			ptr1->rx_time=time(NULL);
			ptr1->aging_time=default_aging_time;
		}
                ptr1 = ptr1->next;
        }
	
}
void update_aging(struct ether_hdr *eth)
{
	struct mac_list *ptr1=NULL;
	ptr1 = start;
        while(ptr1 != NULL)
        {
		if (memcmp(&eth->s_addr, &ptr1->s_addr, ETHER_ADDR_LEN) == 0)
		{
			ptr1->rx_time=time(NULL);
			ptr1->aging_time=default_aging_time;
		}
                ptr1 = ptr1->next;
        }
}
static void
l2fwd_simple_forward(struct rte_mbuf *m,  uint32_t source_ip, unsigned portid)
{
	struct ether_hdr *eth;
	void *tmp;
	unsigned dst_port;
	int currenttime,difference,timeout = 8;
	//int err;
	//pthread_t t1,t2;
	dst_port = l2fwd_dst_ports[portid];
        printf("---------- Forwarding Path ----------\n");
        printf("Default forwarding dst port : %d\n",dst_port);
	
	eth = rte_pktmbuf_mtod(m, struct ether_hdr *);
	
	if(l2fwd_user_defined_dst_port > 0)
	{
		dst_port = l2fwd_user_defined_dst_port;
        	printf("User defined dst Port       : %d\n",dst_port);
		printf("----------------------------------------\n");
		printf("Forwarding path has been modified from default to user defined\n");
	}
	else if(l2fwd_mac_learning_enabled)
	{
        	//l2fwd_mac_display();
		//l2fwd_show_mac_entry();
		//pthread_mutex_lock( &mutex1 );
		printf("--------------l2fwd_mac_learning_enabled--------------------------\n");
		struct node *ptr = NULL;
		struct mac_list *temp = NULL;
		ptr = head;
		if(head == NULL)
		{
			printf("No MAC address exist in table , adding entry to the table\n");
			add_mac_entry(eth,source_ip,portid);
		}
		else
		{
			/* condition to check and overwrite the 'port' if the same mac address 
			   was already received from another 'port' earlier*/
			while(ptr != NULL)
			{
				if ((memcmp(&eth->s_addr, &ptr->s_addr, ETHER_ADDR_LEN) == 0) &&
					 (memcmp(&eth->d_addr, &ptr->d_addr, ETHER_ADDR_LEN) == 0))
				{
				//	printf("----->same SMAC DMAC\n");
					if(portid != ptr->port)
					{
						printf("port id is different %d\n",portid);
						currenttime = time(NULL);
						difference = currenttime - (ptr->starttime);
						if(difference > timeout)
						{
							printf("----->difference %d\n",difference);
							ptr->port = portid;
							ptr->starttime = time(NULL);
							l2fwd_mac_ports_update(eth,portid);
						//	add_mac_entry(eth,portid);
							break;
						} else {
							printf("Detected duplication,packet dropped \n");
							//duplicate packet
							total_duplicate_packets_dropped++;
							//need to check whether break is needed
						//	return;
						}
					}

					ptr->starttime = time(NULL);
					update_aging(eth);
				//	printf("---->received from same mac diff %d\n",difference);
					break;
				}
				ptr = ptr->next;
			}
			if(ptr == NULL)
			{
				/*If the mac entry is not found in the table, then add the entry 
				  to the mac table */
				printf("Adding MAC entry to table\n");
				add_mac_entry(eth,source_ip,portid);
				//l2fwd_mac_ports_update(eth,portid);
			}
		}
	

		/* lookup to get the dst port if the mac entry is already present in the table*/
		temp = start;
		while(temp != NULL)
		{
			if (memcmp(&eth->d_addr, &temp->s_addr, ETHER_ADDR_LEN) == 0)
			{
				dst_port = temp->port;
				printf("\n");
        			printf("dst port from table : %d\n",dst_port);
				break;
			}
			temp = temp->next;
		}	
		/*If the dst mac is not available,then forward the packet in dumping port*/
		if(temp == NULL)
		{
			//dst_port = 9; // BUG ?????
        		printf("Dumping port : %d\n",dst_port);
		}
	
	}
	else 
	{
//========================================================
		/* 02:00:00:00:00:xx */
		tmp = &eth->d_addr.addr_bytes[0];
		*((uint64_t *)tmp) = 0xffffffffffff ;
		//*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dst_port << 40);
	
		/* src addr */
		ether_addr_copy(&l2fwd_ports_eth_addr[dst_port], &eth->s_addr);
	}

	printf("========================================\n");
       	  l2fwd_mac_display();
	  l2fwd_show_mac_entry();
	//pthread_mutex_unlock( &mutex1 );
	printf(" send packet ........\n");
	l2fwd_send_packet(m, (uint8_t) dst_port);
}

/* main processing loop */
static void
l2fwd_main_loop(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	unsigned i, j, portid, nb_rx;
	struct lcore_queue_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;
        struct ether_hdr *eth_hdr;
        uint16_t eth_type;
        struct ipv4_hdr *ip_h;
        int l2_len;
        struct arp_hdr  *arp_h;
        uint32_t sip_addr,tip_addr;

	prev_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	while (1) {

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
				if (qconf->tx_mbufs[portid].len == 0)
					continue;
				l2fwd_send_burst(&lcore_queue_conf[lcore_id],
						 qconf->tx_mbufs[portid].len,
						 (uint8_t) portid);
				qconf->tx_mbufs[portid].len = 0;
			}

			/* if timer is enabled */
			if (timer_period > 0) {

				/* advance the timer */
				timer_tsc += diff_tsc;

				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= (uint64_t) timer_period)) {

					/* do this only on master core */
					if (lcore_id == rte_get_master_lcore()) {
						print_stats();
						/* reset the timer */
						timer_tsc = 0;
					}
				}
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_port; i++) {

			portid = qconf->rx_port_list[i];
			nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,
						 pkts_burst, MAX_PKT_BURST);

			if(nb_rx == 0)
				continue;
			port_statistics[portid].rx += nb_rx;

			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));

                                eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
                                eth_type = RTE_BE_TO_CPU_16(eth_hdr->ether_type);
                                l2_len = sizeof(struct ether_hdr);
                			
				//if (eth_type != ETHER_TYPE_IPv4) {
                        	//	rte_pktmbuf_free(m);
                        	//	continue;
                		//}

                                printf("========================================\n");
                                printf("Packet received on port :%d\n",portid);
                                printf("------------ L2 Details------------\n");
                                print_ether_addr("   Source MAC         : ", &eth_hdr->s_addr);
                                print_ether_addr("\n   Destination MAC    : ", &eth_hdr->d_addr);
                                printf("\n   Port               : %d"
                                	"\n   Ethernet Type      : 0x%04x"
					"\n   Length             : %u "
					"\n   Nb_segs            : %d",
                                        portid,
					eth_type,
					(unsigned) m->pkt_len,
					(int)m->nb_segs);
                                printf("\n");

                                if (eth_type == ETHER_TYPE_ARP)
				{
                                        arp_h = (struct arp_hdr *) ((char *)eth_hdr + l2_len);
                                        sip_addr = arp_h->arp_data.arp_sip;
                                        tip_addr = arp_h->arp_data.arp_tip;
				} else
				{
                			if (eth_type != ETHER_TYPE_IPv4) {
                        			rte_pktmbuf_free(m);
                        			continue;
                			}
                                        ip_h = (struct ipv4_hdr *) ((char *)eth_hdr + l2_len);
                                        sip_addr = ip_h->src_addr;
                                        tip_addr = ip_h->dst_addr;
				}
				printf("------------ L3 Details------------\n");
				ipv4_addr_dump("   Source IP          : ", sip_addr);
                                ipv4_addr_dump("\n   Destination IP     : ", tip_addr);
				printf("\n");
	
				l2fwd_simple_forward(m,  sip_addr,portid);
			}
		}
	}
}

static int
l2fwd_launch_one_lcore(__attribute__((unused)) void *dummy)
{
	l2fwd_main_loop();
	return 0;
}

/* display usage */
static void
l2fwd_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
		   "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n"
		"  -d DST PORT: user input to select the destination port\n"
		"  -l MAC LEARNING: enable l2fwd mac learning 1-enable : 0 - disable(default) \n",
	       prgname);
}

static int
l2fwd_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

static unsigned int
l2fwd_parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;

	return n;
}

static int
l2fwd_parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;

	return n;
}

static int
pktcap_parse_dst_port(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n > RTE_MAX_ETHPORTS)
		return -1;

	return n;
}

static int
l2fwd_parse_mac_learning(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 0);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n > 1)
		return -1;

	return n;
}
/* Parse the argument given in the command line of the application */
static int
l2fwd_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:q:T:d:l:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_enabled_port_mask = l2fwd_parse_portmask(optarg);
			if (l2fwd_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			l2fwd_rx_queue_per_lcore = l2fwd_parse_nqueue(optarg);
			if (l2fwd_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_period = l2fwd_parse_timer_period(optarg) * 1000 * TIMER_MILLISECOND;
			if (timer_period < 0) {
				printf("invalid timer period\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* user defined dst port*/
		case 'd':
			l2fwd_user_defined_dst_port = pktcap_parse_dst_port(optarg);
			if (l2fwd_user_defined_dst_port < 0) {
				printf("invalid port number\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* l2 mac learning*/
		case 'l':
			l2fwd_mac_learning_enabled = l2fwd_parse_mac_learning(optarg);
			if (l2fwd_mac_learning_enabled < 0) {
				printf("invalid option\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;
		/* long options */
		case 0:
			l2fwd_usage(prgname);
			return -1;

		default:
			l2fwd_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

void *thread2(void)
{	
	if(start!=NULL)
                {

                        pthread_mutex_lock( &mutex1 );
                        struct mac_list *temp=start;
                        while(temp!=NULL)
                        {
				if(temp->aging_time>0)
                                        temp->aging_time=default_aging_time-(time(NULL)-temp->rx_time);
                                //strcpy(temp->buff,"check");
                                temp=temp->next;
                        }
                      pthread_mutex_unlock( &mutex1 );
                }
	pthread_exit(NULL);
	return NULL;
}

void update_node(struct mac_list *l_temp)
{
	struct node *temp,*temp2,*prev;
	if(head!=NULL)
	{	
		while(head!=NULL)
		{	
			if(memcmp(&l_temp->s_addr, &head->s_addr, ETHER_ADDR_LEN) == 0)
			{
				temp=head;
				head=head->next;
				free(temp);
			}
			else
				break;
		}
	//	if(head!=NULL)
		temp=head->next;
		prev=head;
		while(temp!=NULL && head!=NULL)
		{
			if(memcmp(&l_temp->s_addr, &temp->s_addr, ETHER_ADDR_LEN) == 0)
			{
				prev->next=temp->next;
				temp2=temp;
				temp=temp->next;
				free(temp2);	
			}
			else
			{
				temp=temp->next;
				prev=prev->next;
			}
		}
	}
	
	
}


void *thread1(void)
{
	struct mac_list *temp,*temp2,*prev;
	
		if(start!=NULL)
		{
		
			pthread_mutex_lock( &mutex1 );	
			while(start!=NULL)
			{	
				if( start->aging_time==0)
				{
					temp = start;
					update_node(temp);
					start=start->next;
					free(temp);
				
				}
				else
					break;		 
			}
	//		if(start!=NULL)
			temp = start->next;
			prev=start;
			while(temp!= NULL && start!=NULL)
        		{
		            
		        	if(temp->aging_time==0)
			     {
				update_node(temp);
				prev->next=temp->next;
				temp2=temp;
				temp=temp->next;
				if(temp2!=NULL)
					free(temp2);
		     	      }
		     	      else
	 		      {
					temp = temp->next; 
					prev=prev->next;
	    	       		}
	        	}
			pthread_mutex_unlock( &mutex1 );
		}
	pthread_exit(NULL);
	return NULL;
}
void *threadInit(void)
{
	pthread_t t1,t2;
	int err;
	while(1)
		{
		err=pthread_create(&t1, NULL,(void *)thread1,NULL);//for checking the mac table
		pthread_join(t1,NULL);
		
		
		err=pthread_create(&t2,NULL,(void *)thread2,NULL);//for updating the mac table
		pthread_join(t2,NULL);
		
		if(err!=0)
		{
			printf("thread not created\n");
			exit(0);
		}
	}
	return NULL;	
}

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	int ret,err;
	uint8_t nb_ports;
	uint8_t nb_ports_available;
	uint8_t portid, last_port;
	unsigned lcore_id, rx_lcore_id;
	unsigned nb_ports_in_mask = 0;
	pthread_t t3;
	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");

	/* create the mbuf pool */
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF, 32,
		0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

	/* reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;
	last_port = 0;
		
	printf("Entered port : %" PRId64 "\n",l2fwd_user_defined_dst_port);

	/*
	 * Each logical core is assigned a dedicated TX queue on each port.
	 */
	if(l2fwd_user_defined_dst_port == 0)
	{
		for (portid = 0; portid < nb_ports; portid++) {
			/* skip ports that are not enabled */
			if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
				continue;
	
			if (nb_ports_in_mask % 2) {
				l2fwd_dst_ports[portid] = last_port;
				l2fwd_dst_ports[last_port] = portid;
			}
			else
				last_port = portid;
	
				nb_ports_in_mask++;
	
			rte_eth_dev_info_get(portid, &dev_info);
		}
		if (nb_ports_in_mask % 2) {
			printf("Notice: odd number of ports in portmask.\n");
			l2fwd_dst_ports[last_port] = last_port;
		}

	}
	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       lcore_queue_conf[rx_lcore_id].n_rx_port ==
		       l2fwd_rx_queue_per_lcore) {
			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
		}

		if (qconf != &lcore_queue_conf[rx_lcore_id])
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];

		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->n_rx_port++;
		printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);
	}

	nb_ports_available = nb_ports;
	
	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", (unsigned) portid);
			nb_ports_available--;
			continue;
		}
		/* init port */
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);
		ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, (unsigned) portid);

		rte_eth_macaddr_get(portid,&l2fwd_ports_eth_addr[portid]);

		/* init one RX queue */
		fflush(stdout);
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     NULL,
					     l2fwd_pktmbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		/* init one TX queue on each port */
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, (unsigned) portid);
		/****************************************/
		err=pthread_create(&t3,NULL,(void *)threadInit,NULL);
		//thread for checking and updating the mac table
		if(err!=0)
		{
			printf("thread not created\n");
			exit(0);
		}	
		/****************************************/
		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		printf("done: \n");

		rte_eth_promiscuous_enable(portid);

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				(unsigned) portid,
				l2fwd_ports_eth_addr[portid].addr_bytes[0],
				l2fwd_ports_eth_addr[portid].addr_bytes[1],
				l2fwd_ports_eth_addr[portid].addr_bytes[2],
				l2fwd_ports_eth_addr[portid].addr_bytes[3],
				l2fwd_ports_eth_addr[portid].addr_bytes[4],
				l2fwd_ports_eth_addr[portid].addr_bytes[5]);

		/* initialize port stats */
		memset(&port_statistics, 0, sizeof(port_statistics));
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}

	check_all_ports_link_status(nb_ports, l2fwd_enabled_port_mask);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}
	//pthread_join(t1,NULL);
        //pthread_join(t2,NULL);

	return 0;
}
