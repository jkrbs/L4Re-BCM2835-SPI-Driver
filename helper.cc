#include "helper.h"

/* Read with memory barriers from peripheral
 *
 */
uint32_t bcm2835_peri_read(long unsigned int offset)
{
    uint32_t ret;
    __sync_synchronize();
    ret = spi->read<uint32_t>(offset);
     __sync_synchronize();
    return ret;
}

/* read from peripheral without the read barrier
 * This can only be used if more reads to THE SAME peripheral
 * will follow.  The sequence must terminate with memory barrier
 * before any read or write to another peripheral can occur.
 * The MB can be explicit, or one of the barrier read/write calls.
 */
uint32_t bcm2835_peri_read_nb(long unsigned int offset)
{
	return spi->read<uint32_t>(offset);
}

/* Write with memory barriers to peripheral
 */

void bcm2835_peri_write(long unsigned int offset, uint32_t value)
{
        __sync_synchronize();
        spi->write(offset, value);
        __sync_synchronize();
}

/* write to peripheral without the write barrier */
void bcm2835_peri_write_nb(long unsigned int offset, uint32_t value)
{
	spi->write(offset, value);
}
