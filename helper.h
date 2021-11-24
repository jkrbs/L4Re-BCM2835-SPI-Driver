#pragma once

#include <cstdio>
#include <l4/drivers/io_regblock.h>
#include <l4/io/io.h>
#include <l4/vbus/vbus>
#include <sys/types.h>
typedef u_int32_t uint32_t;

extern L4::Io_register_block_mmio *spi;

uint32_t bcm2835_peri_read(long unsigned int offset);
uint32_t bcm2835_peri_read_nb(long unsigned int offset);
void bcm2835_peri_write(long unsigned int offset, uint32_t value);
void bcm2835_peri_write_nb(long unsigned int offset, uint32_t value);
