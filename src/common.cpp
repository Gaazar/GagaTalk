#include "gt_defs.h"

int edcrypt_voip_pack(char* buf, int sz, uint32_t cert)
{
	int n = sz / 4;
	int x = sz % 4;
	uint32_t* ibuf = (uint32_t*)buf;
	for (int i = 1; i < n; i++)
	{
		ibuf[i] ^= cert;
	}
	for (int i = 0; i < x; i++)
	{
		buf[n * 4 + i] ^= ((uint8_t*)&cert)[i];
	}
	return 0;
}