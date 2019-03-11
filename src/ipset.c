/*
 * DPVS is a software load balancer (Virtual Server) based on DPDK.
 *
 * Copyright (C) 2017 iQIYI (www.iqiyi.com).
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <string.h>
#include <assert.h>
#include "route.h"
//#include "conf/route.h"
//
//#include "ctrl.h"
#define IPSET_TAB_SIZE (1<<8)
#define IPSET_TAB_MASK (IPSET_TAB_SIZE - 1)

#define this_ipset_lcore        (RTE_PER_LCORE(ipset_lcore))
#define this_ipset_table_lcore  (this_ipset_lcore.ipset_table)
#define this_num_ipset          (RTE_PER_LCORE(num_ipset))

struct ipset_lcore{
	struct list_head ipset_table[IPSET_TAB_SIZE];
};

static RTE_DEFINE_PER_LCORE(struct ipset_lcore, ipset_lcore);
static RTE_DEFINE_PER_LCORE(rte_atomic32_t, num_ipset);


struct ipset_addr {
	int af;
	union inet_addr    addr;
};

struct ipset_entry {
    struct list_head list;
    struct ipset_addr daddr;
	int route_table_num;
};
int ipset_lcore_init(void);
#if 0
static int ipset_lcore_init(void *arg)
{
    int i;

    if (!rte_lcore_is_enabled(rte_lcore_id()))
        return EDPVS_DISABLED;

    for (i = 0; i < IPSET_TAB_SIZE; i++)
        INIT_LIST_HEAD(&this_ipset_table_lcore[i]);

    return EDPVS_OK;
}
#endif

int ipset_lcore_init(void)
{
	int i;
    for (i = 0; i< IPSET_TAB_SIZE; i++)
        INIT_LIST_HEAD(&this_ipset_table_lcore[i]);
          
    return 0;
}

static inline unsigned int ipset_addr_hash(int af, union inet_addr *addr)
{
    uint32_t addr_fold;

    addr_fold = inet_addr_fold(af, addr);

    if (!addr_fold) {
        printf("%s: IP proto not support.\n", __func__);
        return 0;
    }

    return rte_be_to_cpu_32(addr_fold)&IPSET_TAB_MASK;
}


static struct ipset_entry *ipset_new_entry(int af, union inet_addr * dest, int table_num)
{
    struct ipset_entry *new_ipset=NULL;
    if(!dest)
        return NULL;
    new_ipset = rte_zmalloc("new_ipset_entry", sizeof(struct ipset_entry), 0);
    if (new_ipset == NULL){
        return NULL;
    }
	new_ipset->daddr.af = af;
	memcpy(&new_ipset->daddr.addr, dest, sizeof(union inet_addr));
    new_ipset->route_table_num = table_num;
    return new_ipset;
}



int ipset_add(int af, union inet_addr *dest, int table_num);
int ipset_add(int af, union inet_addr *dest, int table_num)
{
    unsigned int hashkey;
    struct ipset_entry *ipset_node, *ipset_new;

    hashkey = ipset_addr_hash(af, dest);

    list_for_each_entry(ipset_node, &this_ipset_table_lcore[hashkey], list){
        if (ipset_node->daddr.af == af && inet_addr_equal(af, &ipset_node->daddr.addr, dest)) {
            return EDPVS_EXIST;
        }
    }

    ipset_new = ipset_new_entry(af, dest, table_num);
    if (!ipset_new){
        return EDPVS_NOMEM;
    }
 
    list_add(&ipset_new->list, &this_ipset_table_lcore[hashkey]);
	rte_atomic32_inc(&this_num_ipset);	
    return EDPVS_OK;
}

struct ipset_entry *ipset_addr_lookup(int af, union inet_addr *dest);
struct ipset_entry *ipset_addr_lookup(int af, union inet_addr *dest)
{
    unsigned int hashkey;
    struct ipset_entry *ipset_node;

    hashkey = ipset_addr_hash(af, dest);
    list_for_each_entry(ipset_node, &this_ipset_table_lcore[hashkey], list){
        if (ipset_node->daddr.af == af && inet_addr_equal(af, &ipset_node->daddr.addr, dest)) {
            return ipset_node;
        }
    }
    return NULL;
}


int ipset_del(int af, union inet_addr *dest);
int ipset_del(int af, union inet_addr *dest)
{
	struct ipset_entry *ipset_node;

	ipset_node = ipset_addr_lookup(af, dest);
	if (!ipset_node)
		return EDPVS_NOTEXIST;
	list_del(&ipset_node->list);
    rte_atomic32_dec(&this_num_ipset);
    return EDPVS_OK; 
}


int ipset_list(void);
int ipset_list(void)
{
	struct ipset_entry *ipset_node;
	int i;
	char ip6str[64], ip4str[32];
	for (i = 0; i < IPSET_TAB_SIZE; i++) {
		list_for_each_entry(ipset_node, &this_ipset_table_lcore[i], list){
			if (ipset_node && ipset_node->daddr.af == AF_INET) {
                                inet_ntop(AF_INET, (union inet_addr*)&ipset_node->daddr.addr, ip4str, sizeof(ip4str));
                                printf("%s\n", ip4str);
			}
			else if (ipset_node && ipset_node->daddr.af == AF_INET6) {
				inet_ntop(AF_INET6, (union inet_addr*)&ipset_node->daddr.addr, ip6str, sizeof(ip6str));
				printf("%s\n", ip6str);
			}	
		}		
	}
    return 0;
}



int ipset_test(void);
int ipset_test(void)
{
	ulong ip4;
	char *ip6;
	struct in_addr ipv4;
	struct in6_addr ipv6;
	ip4 = inet_addr("192.168.168.168");
	ip6 = strdup("2a01:198:603:0:396e:4789:8e99:890f");
	memcpy(&ipv4, &ip4, sizeof(ip4));
	inet_pton(AF_INET6, ip6, &ipv6);
	ipset_add(AF_INET, (union inet_addr *)&ipv4, 0);
	ipset_list();
	printf("%d\n", this_num_ipset.cnt);
	ipset_add(AF_INET6, (union inet_addr *)&ipv6, 0);
	ipset_list();	
	printf("%d\n", this_num_ipset.cnt);
	ipset_del(AF_INET, (union inet_addr *)&ipv4);
	ipset_list();	
	printf("%d\n", this_num_ipset.cnt);
        ip4 = inet_addr("192.168.168.166");
        memcpy(&ipv4, &ip4, sizeof(ip4));
	ipset_del(AF_INET, (union inet_addr *)&ipv4);
	ipset_list();	
	printf("%d\n", this_num_ipset.cnt);
	ipset_del(AF_INET6, (union inet_addr *)&ipv6);
	ipset_list();	
	printf("%d\n", this_num_ipset.cnt);
	return this_num_ipset.cnt;
}




