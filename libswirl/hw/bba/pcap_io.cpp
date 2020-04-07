/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "types.h"

#if HAS_PCAP
#include <stdio.h>
#include <stdarg.h>
#include "pcap.h"
#include "ethernet.h"
#include "oslib/threading.h"
#include <atomic>

mac_address virtual_mac   = { 0x76, 0x6D, 0x61, 0x63, 0x30, 0x31 };
//mac_address virtual_mac   = { 0x6D, 0x76, 0x63, 0x61, 0x31, 0x30 };
mac_address broadcast_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

ip_address virtual_ip = { 192, 168, 2, 4};

pcap_t *adhandle;
volatile int pcap_io_running=0;

char errbuf[PCAP_ERRBUF_SIZE];

char namebuff[256];

FILE*packet_log;

pcap_dumper_t *dump_pcap;

mac_address host_mac = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#if 0

// Fetches the MAC address and prints it
int GetMACaddress(char *adapter, mac_address* addr)
{
	static IP_ADAPTER_INFO AdapterInfo[128];       // Allocate information
												 // for up to 128 NICs
	static PIP_ADAPTER_INFO pAdapterInfo;
	ULONG dwBufLen = sizeof(AdapterInfo);	// Save memory size of buffer

	DWORD dwStatus = GetAdaptersInfo(      // Call GetAdapterInfo
	AdapterInfo,                 // [out] buffer to receive data
	&dwBufLen);                  // [in] size of receive data buffer
	if(dwStatus != ERROR_SUCCESS)    // Verify return value is
		return 0;                       // valid, no buffer overflow

	pAdapterInfo = AdapterInfo; // Contains pointer to
											   // current adapter info
	do {
		if(strcmp(pAdapterInfo->AdapterName,adapter+12)==0)
		{
			memcpy(addr,pAdapterInfo->Address,6);
			return 1;
		}

		pAdapterInfo = pAdapterInfo->Next;    // Progress through
	}
	while(pAdapterInfo);                    // Terminate if last adapter
	return 0;
}



int gettimeofday (struct timeval *tv, void* tz)
{
  unsigned __int64 ns100; /*time since 1 Jan 1601 in 100ns units */

  GetSystemTimeAsFileTime((LPFILETIME)&ns100);
  tv->tv_usec = (long) ((ns100 / 10L) % 1000000L);
  tv->tv_sec = (long) ((ns100 - 116444736000000000L) / 10000000L);
  return (0);
} 
#endif


int pcap_io_send(void* packet, int plen)
{
	struct pcap_pkthdr ph;

	if(pcap_io_running<=0)
		return -1;
	printf("WINPCAP:  * pcap io: Sending %d byte packet.\n",plen);

	if(dump_pcap)
	{
		gettimeofday(&ph.ts,NULL);
		ph.caplen=plen;
		ph.len=plen;
		pcap_dump((u_char*)dump_pcap,&ph,(u_char*)packet);
	}

	if(packet_log)
	{
		int i=0;
		int n=0;

		fprintf(packet_log,"PACKET SEND: %d BYTES\n",plen);
		for(i=0,n=0;i<plen;i++)
		{
			fprintf(packet_log,"%02x",((unsigned char*)packet)[i]);
			n++;
			if(n==16)
			{
				fprintf(packet_log,"\n");
				n=0;
			}
			else
			fprintf(packet_log," ");
		}
		fprintf(packet_log,"\n");
	}

	#if why_did_it_do_that
	if(mac_compare(((ethernet_header*)packet)->dst,broadcast_mac)==0)
	{
		static u_char pack[65536];
		memcpy(pack,packet,plen);

		((ethernet_header*)packet)->dst=host_mac;
		pcap_sendpacket(adhandle, pack, plen);
	}
	#endif

	return pcap_sendpacket(adhandle, (const u_char*)packet, plen);
}

int pcap_io_recv_blocking(void* packet, int max_len)
{
	int res;
	struct pcap_pkthdr *header;
	const u_char *pkt_data;

	if(pcap_io_running<=0)
		return -1;

	if((res = pcap_next_ex(adhandle, &header, &pkt_data)) > 0)
	{
		ethernet_header *ph=(ethernet_header*)pkt_data;
		if(ph->dst != virtual_mac && ph->dst != broadcast_mac)
		{
			return 0;
		}

		memcpy(packet,pkt_data,header->len);

		if(dump_pcap)
			pcap_dump((u_char*)dump_pcap,header,(u_char*)packet);

		if(packet_log)
		{
			int i=0;
			int n=0;
			int plen=header->len;

			fprintf(packet_log,"PACKET RECV: %d BYTES\n",plen);
			for(i=0,n=0;i<plen;i++)
			{
				fprintf(packet_log,"%02x",((unsigned char*)packet)[i]);
				n++;
				if(n==16)
				{
					fprintf(packet_log,"\n");
					n=0;
				}
				else
				fprintf(packet_log," ");
			}
			fprintf(packet_log,"\n");
		}

		return header->len;
	}

	return -1;
}
#define PacketBufferSize (2048*64)
#define PacketBufferMask (PacketBufferSize-1)

std::atomic<int> PacketCount;
volatile u8 PacketBuffer[PacketBufferSize+2048];
std::atomic<u32> WriteCursor;
std::atomic<u32> ReadCursor;

// statement.


void* rx_thread(void* p)
{
	PacketCount=0;
	WriteCursor=0;

	while(pcap_io_running)
	{
		u32 RCval=ReadCursor;
		if (WriteCursor<RCval && (WriteCursor+2048)>RCval)
		{
			printf("rx_thread : buffer full !\n");
			//Sleep(10);//whee ?
		}
		else
		{
			int len=pcap_io_recv_blocking((void*)&PacketBuffer[WriteCursor+4],1520);
			if (len>0)
			{
				//only this thread uses the write cursor
				*(u32*)&PacketBuffer[WriteCursor]=len;
				WriteCursor+=len+4;
				if (WriteCursor>=PacketBufferSize)
					WriteCursor=0;

				//This however, needs to be interlocked
				PacketCount++;
			}
		}
	}
	printf("rx-thread : Terminated\n");
	return 0;
}
int pcap_io_recv(void* packet, int max_len)
{
	if (PacketCount==0)
		return 0;

	u32 len=*(u32*)&PacketBuffer[ReadCursor];
	if ((u32)max_len<len)
		__debugbreak;
	
	memcpy(packet,(void*)&PacketBuffer[ReadCursor+4],len);

	//Make sure ReadCursor allways has a valid value :)
	//not even sure that much is needed tbh, bored to think :P

	u32 RCpos=ReadCursor;

	RCpos+=len+4;
	if (RCpos>=PacketBufferSize)
		RCpos=0;
	ReadCursor=RCpos;

	//This however, needs to be interlocked
	PacketCount--;
	//EnterCriticalSection(&cs);
	//PacketCount--;
	//ExitCriticalSection(&cs);
	return len;
}
void pcap_io_close()
{
	pcap_io_running=0;
	if(packet_log)
		fclose(packet_log);
	if(dump_pcap)
		pcap_dump_close(dump_pcap);
	pcap_close(adhandle); 
}

int pcap_io_get_dev_num()
{
	pcap_if_t *alldevs;
	pcap_if_t *d;
	int i=0;

	if(pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		return 0;
	}
    
	d=alldevs;
    while(d!=NULL) {d=d->next; i++;}

	pcap_freealldevs(alldevs);

	return i;
}

char* pcap_io_get_dev_name(int num)
{
	pcap_if_t *alldevs;
	pcap_if_t *d;
	int i=0;

	if(pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		return NULL;
	}
    
	d=alldevs;
    while(d!=NULL) {
		if(num==i)
		{
			strcpy(namebuff,d->name);
			pcap_freealldevs(alldevs);
			return namebuff;
		}
		d=d->next; i++;
	}

	pcap_freealldevs(alldevs);

	return NULL;
}

char* pcap_io_get_dev_desc(int num)
{
	pcap_if_t *alldevs;
	pcap_if_t *d;
	int i=0;

	if(pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		return NULL;
	}
    
	d=alldevs;
    while(d!=NULL) {
		if(num==i)
		{
			strcpy(namebuff,d->description);
			pcap_freealldevs(alldevs);
			return namebuff;
		}
		d=d->next; i++;
	}

	pcap_freealldevs(alldevs);

	return NULL;
}

cThread rxthd(rx_thread, 0);

int pcap_io_init(char *adapter)
{
	printf("WINPCAP: Opening adapter '%s'...",adapter);

	//GetMACaddress(adapter,&host_mac);
		
	/* Open the adapter */
	if ((adhandle= pcap_open_live(adapter,	// name of the device
							 65536,			// portion of the packet to capture. 
											// 65536 grants that the whole packet will be captured on all the MACs.
							 1,				// promiscuous mode (nonzero means promiscuous)
							 1,			// read timeout
							 errbuf			// error buffer
							 )) == NULL)
	{
		printf("\nWINPCAP: Unable to open the adapter. %s is not supported by WinPcap\n", adapter);
		return -1;
	}

	/*
	if(pcap_setnonblock(adhandle,1,errbuf)==-1)
	{
		printf("WINPCAP: Error setting non-blocking mode. Default mode will be used.\n");
	}
	*/

	packet_log=fopen("logs/packet.log","w");

	dump_pcap = pcap_dump_open(adhandle,"logs/pkt_log.pcap");


	rxthd.Start();
	pcap_io_running=1;
	printf("WINPCAP: Ok.\n");
	return 0;
}
#endif