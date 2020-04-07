/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "VirtualNetwork.h"

struct VirtualNetwork_null_impl: VirtualNetwork {
    virtual void GetMacAddress(mac_address* mac) {
        memset(mac, 0x88, sizeof(mac));
    }

    virtual void SendEthernetPacket(const void* packet, int len) {
        // do nothing
    }

    virtual int RecvEthernetPacket(void* packet, int max_len) {
        // no packets to recieve
        return 0;
    }
};

VirtualNetwork* VirtualNetwork::CreateNullNetwork() {
    return new VirtualNetwork_null_impl();
}