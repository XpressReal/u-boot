#include "rtk_i2c-pow.h"

#define _BIT(_n)      (1 << (_n))

void rtk_i2c_set_pow(int BusId)
{
	if (BusId == 0 || BusId == 1) {
		unsigned int *rst = (unsigned int *)0x98007088;
		unsigned int *clk = (unsigned int *)0x9800708c;

			if (BusId == 0) {
				*rst |= _BIT(11);
				*clk |= _BIT(9);
			} else {
				*rst |= _BIT(12);
				*clk |= _BIT(10);
			}
	}
#if defined(CONFIG_TARGET_RTD1619B)
	else if (BusId == 3) {
		unsigned int *rst = (unsigned int *)0x98000008;
		unsigned int *clk = (unsigned int *)0x98000058;

		*rst |= _BIT(12) | _BIT(13);
		*clk |= _BIT(1) | _BIT(0);
	} else if (BusId == 4) {
		unsigned int *rst = (unsigned int *)0x98000068;
		unsigned int *clk = (unsigned int *)0x9800005c;

		*rst |= _BIT(2) | _BIT(3);
		*clk |= _BIT(20) | _BIT(21);
	} else if (BusId == 5) {
		unsigned int *rst = (unsigned int *)0x98000068;
		unsigned int *clk = (unsigned int *)0x9800005c;

		*rst |= _BIT(4) | _BIT(5);
		*clk |= _BIT(22) | _BIT(23);
	}
#else
	else if (BusId == 5) {
		unsigned int *rst = (unsigned int *)0x98000004;
		unsigned int *clk = (unsigned int *)0x98000010;

		*rst |= _BIT(16);
		*clk |= _BIT(1);
	}
#endif
}

void rtk_i2c_clear_pow(int BusId)
{}
