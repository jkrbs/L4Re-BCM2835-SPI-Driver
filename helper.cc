#include "helper.h"
#include "bcm2835.h"
/* Read with memory barriers from peripheral
 *
 */

extern off_t bcm2835_peripherals_base;

uint32_t bcm2835_peri_read(volatile uint32_t *paddr) {
  return spi->read<uint32_t>((l4_uint64_t)paddr - 0x200000);
}

/* read from peripheral without the read barrier
 * This can only be used if more reads to THE SAME peripheral
 * will follow.  The sequence must terminate with memory barrier
 * before any read or write to another peripheral can occur.
 * The MB can be explicit, or one of the barrier read/write calls.
 */
uint32_t bcm2835_peri_read_nb(volatile uint32_t *paddr) {
  return spi->read<uint32_t>((l4_uint64_t)paddr - 0x200000);
}

/* Write with memory barriers to peripheral
 */

void bcm2835_peri_write(volatile uint32_t *paddr, uint32_t value) {
  spi->write((l4_uint64_t)paddr - 0x200000, value);
}

/* write to peripheral without the write barrier */
void bcm2835_peri_write_nb(volatile uint32_t *paddr, uint32_t value) {
  spi->write((l4_uint64_t)paddr - 0x200000, value);
}
