/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
extern const u32 mmu_mask[4];

static bool wince_resolve_address(u32 va, TLB_Entry &entry)
{
	// WinCE hack
	if ((va & 0x80000000) == 0)
	{
		u32 page_group = ReadMem32_nommu(CCN_TTB + ((va >> 25) << 2));
		u32 page = ((va >> 16) & 0x1ff) << 2;
		u32 paddr = ReadMem32_nommu(page_group + page);
		if (paddr & 0x80000000)
		{
			u32 whatever = ReadMem32_nommu(r_bank[4] + 0x14);
			if (whatever != ReadMem32_nommu(paddr))
			{
				paddr += 12;
				u32 ptel = ReadMem32_nommu(paddr + ((va >> 10) & 0x3c));
				//FIXME CCN_PTEA = paddr >> 29;
				if (ptel != 0)
				{
					entry.Data.reg_data = ptel - 1;
					entry.Address.ASID = CCN_PTEH.ASID;
					entry.Assistance.reg_data = 0;
					u32 sz = entry.Data.SZ1 * 2 + entry.Data.SZ0;
					entry.Address.VPN = (va & mmu_mask[sz]) >> 10;

					return true;
				}
			}
		}
	}
	else
	{
		// SQ
		if (((va >> 26) & 0x3F) == 0x38)
		{
			u32 r1 = (va - 0xe0000000) & 0xfff00000;
			//r1 &= 0xfff00000;
			//u32 r0 = ReadMem32_nommu(0x8C01258C);	// FIXME
			//u32 r0 = 0x8c138b14;
			//r0 = ReadMem32_nommu(r0);	// 0xE0001F5
			u32 r0 = 0xe0001f5;
			r0 += r1;
			entry.Data.reg_data = r0 - 1;
			entry.Assistance.reg_data = r0 >> 29;
			entry.Address.ASID = CCN_PTEH.ASID;
			u32 sz = entry.Data.SZ1 * 2 + entry.Data.SZ0;
			entry.Address.VPN = (va & mmu_mask[sz]) >> 10;

			return true;
		}
	}

	return false;
}


