#pragma once

#pragma pack(push,1)

typedef struct _ip_address
{
	u_char bytes[4];
} ip_address;

typedef struct _mac_address
{
	u_char bytes[6];
} mac_address;

typedef struct _ethernet_header
{
	mac_address dst;
	mac_address src;
	u_short protocol;
} ethernet_header;

typedef struct _arp_packet
{
	u_short hw_type;
	u_short protocol;
	u_char h_addr_len;
	u_char p_addr_len;
	u_short operation;
	mac_address h_src;
	ip_address  p_src;
	mac_address h_dst;
	ip_address  p_dst;
} arp_packet;

typedef struct _ip_header {
	u_char	ip_vhl;			/* version << 4 | header length >> 2 */
	u_char	ip_tos;			/* type of service */
	u_short	ip_len;			/* total length */
	u_short	ip_id;			/* identification */
	u_short	ip_off;			/* fragment offset field */
	u_char	ip_ttl;			/* time to live */
	u_char	ip_p;			/* protocol */
	u_short	ip_sum;			/* checksum */
	ip_address ip_src;      /* source and dest address */
	ip_address ip_dst;	
} ip_header;

typedef struct _full_arp_packet
{
	ethernet_header header;
	arp_packet arp;
} full_arp_packet;

#pragma pack(pop)

#define ARP_REQUEST 0x0100 //values are big-endian

extern mac_address virtual_mac;
extern mac_address broadcast_mac;

extern ip_address virtual_ip;

#define mac_compare(a,b) (memcmp(&(a),&(b),6))
#define ip_compare(a,b) (memcmp(&(a),&(b),4))

int pcap_io_init(char *adapter);
int pcap_io_send(void* packet, int plen);
int pcap_io_recv(void* packet, int max_len);
void pcap_io_close();

int pcap_io_get_dev_num();
char* pcap_io_get_dev_desc(int num);
char* pcap_io_get_dev_name(int num);
