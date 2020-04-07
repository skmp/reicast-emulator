#pragma once

#include "types.h"
#include "VirtualNetwork.h"

#pragma pack(push,1)

struct ip_address
{
	u8 bytes[4];
};

struct ethernet_header
{
	mac_address dst;
	mac_address src;
	u16 protocol;
};

struct arp_packet
{
	u16 hw_type;
	u16 protocol;
	u8 h_addr_len;
	u8 p_addr_len;
	u16 operation;
	mac_address h_src;
	ip_address  p_src;
	mac_address h_dst;
	ip_address  p_dst;
};

struct ip_header {
	u8	ip_vhl;			/* version << 4 | header length >> 2 */
	u8	ip_tos;			/* type of service */
	u16	ip_len;			/* total length */
	u16	ip_id;			/* identification */
	u16	ip_off;			/* fragment offset field */
	u8	ip_ttl;			/* time to live */
	u8	ip_p;			/* protocol */
	u16	ip_sum;			/* checksum */
	ip_address ip_src;      /* source and dest address */
	ip_address ip_dst;	
};

struct full_arp_packet
{
	ethernet_header header;
	arp_packet arp;
};

#pragma pack(pop)

#define ARP_REQUEST 0x0100 //values are big-endian
