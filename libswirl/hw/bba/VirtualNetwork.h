/*
	This file is part of libswirl
*/
#include "license/bsd"

#pragma once

#include "types.h"

struct mac_address
{
	u8 bytes[6];

    bool operator==(const mac_address& rhs) const
    {
        return memcmp(bytes, rhs.bytes, sizeof(bytes)) == 0;
    }

    bool operator!=(const mac_address& rhs) const
    {
        return !(*this == rhs);
    }
};

struct VirtualNetwork {
    virtual void GetMacAddress(mac_address* mac) = 0;

    virtual void SendEthernetPacket(const void* packet, int len) = 0;
    virtual int RecvEthernetPacket(void* packet, int max_len) = 0;

    VirtualNetwork* CreatePcapNetwork(const char* adapter);

    VirtualNetwork* CreateNullNetwork();
};