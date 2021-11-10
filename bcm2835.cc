/* bcm2835.c
// C and C++ support for Broadcom BCM 2835 as used in Raspberry Pi
// http://elinux.org/RPi_Low-level_peripherals
// http://www.raspberrypi.org/wp-content/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
//
// Author: Mike McCauley
// Copyright (C) 2011-2013 Mike McCauley
// $Id: bcm2835.c,v 1.28 2020/01/11 05:07:13 mikem Exp mikem $
*/

#include <stdio.h>
#include <unistd.h>
#include "bcm2835.h"
#include "helper.h"

/* This define enables a little test program (by default a blinking output on pin RPI_GPIO_PIN_11)
// You can do some safe, non-destructive testing on any platform with:
// gcc bcm2835.c -D BCM2835_TEST
// ./a.out
*/
/*#define BCM2835_TEST*/

/* Uncommenting this define compiles alternative I2C code for the version 1 RPi
// The P1 header I2C pins are connected to SDA0 and SCL0 on V1.
// By default I2C code is generated for the V2 RPi which has SDA1 and SCL1 connected.
*/
/* #define I2C_V1*/

/* Physical address and size of the peripherals block
// May be overridden on RPi2
*/
off_t bcm2835_peripherals_base = BCM2835_PERI_BASE;
size_t bcm2835_peripherals_size = BCM2835_PERI_SIZE;

/* This variable allows us to test on hardware other than RPi.
// It prevents access to the kernel memory, and does not do any peripheral access
// Instead it prints out what it _would_ do if debug were 0
 */
static uint8_t debug = 0;

/* SPI bit order. BCM2835 SPI0 only supports MSBFIRST, so we instead 
 * have a software based bit reversal, based on a contribution by Damiano Benedetti
 */
static uint8_t bcm2835_spi_bit_order = BCM2835_SPI_BIT_ORDER_MSBFIRST;
static uint8_t bcm2835_byte_reverse_table[] = 
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};



static uint8_t bcm2835_correct_order(uint8_t b)
{
    if (bcm2835_spi_bit_order == BCM2835_SPI_BIT_ORDER_LSBFIRST)
	return bcm2835_byte_reverse_table[b];
    else
	return b;
}

/*
// Low level register access functions
*/

void  bcm2835_set_debug(uint8_t d)
{
    debug = d;
}

unsigned int bcm2835_version(void) 
{
    return BCM2835_VERSION;
}



/* Set/clear only the bits in value covered by the mask
 * This is not atomic - can be interrupted.
 */
void bcm2835_peri_set_bits(long unsigned int offset, uint32_t value, uint32_t mask)
{
    uint32_t v = bcm2835_peri_read(offset);
    v = (v & ~mask) | (value & mask);
    bcm2835_peri_write(offset, v);
}

/*
// Low level convenience functions
*/



/*
// Higher level convenience functions
*/

void bcm2835_aux_spi_reset(void)
 {
     auto cntl0 = BCM2835_AUX_SPI_CNTL0/4;
     auto cntl1 = BCM2835_AUX_SPI_CNTL1/4;
 
     bcm2835_peri_write(cntl1, 0);
     bcm2835_peri_write(cntl0, BCM2835_AUX_SPI_CNTL0_CLEARFIFO);
}

int bcm2835_spi_begin(void)
{
    auto paddr = BCM2835_SPI0_CS/4;;

    /* Set the SPI CS register to the some sensible defaults */
    bcm2835_peri_write(paddr, 0); /* All 0s */
    
    /* Clear TX and RX fifos */
    bcm2835_peri_write_nb(paddr, BCM2835_SPI0_CS_CLEAR);

    return 1; // OK
}

void bcm2835_spi_setBitOrder(uint8_t order)
{
    bcm2835_spi_bit_order = order;
}

/* defaults to 0, which means a divider of 65536.
// The divisor must be a power of 2. Odd numbers
// rounded down. The maximum SPI clock rate is
// of the APB clock
*/
void bcm2835_spi_setClockDivider(uint16_t divider)
{
    auto paddr = BCM2835_SPI0_CLK/4;
    bcm2835_peri_write(paddr, divider);
}

void bcm2835_spi_set_speed_hz(uint32_t speed_hz)
{
	uint16_t divider = (uint16_t) ((uint32_t) BCM2835_CORE_CLK_HZ / speed_hz);
	divider &= 0xFFFE;
	bcm2835_spi_setClockDivider(divider);
}

void bcm2835_spi_setDataMode(uint8_t mode)
{
    auto paddr = BCM2835_SPI0_CS/4;
    /* Mask in the CPO and CPHA bits of CS */
    bcm2835_peri_set_bits(paddr, mode << 2, BCM2835_SPI0_CS_CPOL | BCM2835_SPI0_CS_CPHA);
}

/* Writes (and reads) a single byte to SPI */
uint8_t bcm2835_spi_transfer(uint8_t value)
{
    auto  paddr = BCM2835_SPI0_CS/4;
    auto fifo = BCM2835_SPI0_FIFO/4;
    uint32_t ret;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral? 
    // Clear TX and RX fifos
    */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    /* Maybe wait for TXD */
    while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
	;

    /* Write to FIFO, no barrier */
    bcm2835_peri_write_nb(fifo, bcm2835_correct_order(value));

    /* Wait for DONE to be set */
    while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE))
	;

    /* Read any byte that was sent back by the slave while we sere sending to it */
    ret = bcm2835_correct_order(bcm2835_peri_read_nb(fifo));

    /* Set TA = 0, and also set the barrier */
    bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);

    return ret;
}

/* Writes (and reads) an number of bytes to SPI */
void bcm2835_spi_transfernb(char* tbuf, char* rbuf, uint32_t len)
{
    auto paddr = BCM2835_SPI0_CS/4;
    auto fifo = BCM2835_SPI0_FIFO/4;
    uint32_t TXCnt=0;
    uint32_t RXCnt=0;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral? 
    */

    /* Clear TX and RX fifos */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    /* Use the FIFO's to reduce the interbyte times */
    while((TXCnt < len)||(RXCnt < len))
    {
        /* TX fifo not full, so add some more bytes */
        while(((bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD))&&(TXCnt < len ))
        {
	    bcm2835_peri_write_nb(fifo, bcm2835_correct_order(tbuf[TXCnt]));
	    TXCnt++;
        }
        /* Rx fifo not empty, so get the next received bytes */
        while(((bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_RXD))&&( RXCnt < len ))
        {
	    rbuf[RXCnt] = bcm2835_correct_order(bcm2835_peri_read_nb(fifo));
	    RXCnt++;
        }
    }
    /* Wait for DONE to be set */
    while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE))
	;

    /* Set TA = 0, and also set the barrier */
    bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
}

/* Writes an number of bytes to SPI */
void bcm2835_spi_writenb(const char* tbuf, uint32_t len)
{
    auto paddr = BCM2835_SPI0_CS/4;
    auto fifo = BCM2835_SPI0_FIFO/4;
    uint32_t i;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral?
    // Answer: an ISR is required to issue the required memory barriers.
    */

    /* Clear TX and RX fifos */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    for (i = 0; i < len; i++)
    {
	/* Maybe wait for TXD */
	while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
	    ;
	
	/* Write to FIFO, no barrier */
	bcm2835_peri_write_nb(fifo, bcm2835_correct_order(tbuf[i]));
	
	/* Read from FIFO to prevent stalling */
	while (bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_RXD)
	    (void) bcm2835_peri_read_nb(fifo);
    }
    
    /* Wait for DONE to be set */
    while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {
	while (bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_RXD)
		(void) bcm2835_peri_read_nb(fifo);
    };

    /* Set TA = 0, and also set the barrier */
    bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
}

/* Writes (and reads) an number of bytes to SPI
// Read bytes are copied over onto the transmit buffer
*/
void bcm2835_spi_transfern(char* buf, uint32_t len)
{
    bcm2835_spi_transfernb(buf, buf, len);
}

void bcm2835_spi_chipSelect(uint8_t cs)
{
    auto paddr = BCM2835_SPI0_CS/4;
    /* Mask in the CS bits of CS */
    bcm2835_peri_set_bits(paddr, cs, BCM2835_SPI0_CS_CS);
}

void bcm2835_spi_setChipSelectPolarity(uint8_t cs, uint8_t active)
{
    auto paddr = BCM2835_SPI0_CS/4;
    uint8_t shift = 21 + cs;
    /* Mask in the appropriate CSPOLn bit */
    bcm2835_peri_set_bits(paddr, active << shift, 1 << shift);
}

void bcm2835_spi_write(uint16_t data)
{
    auto paddr = BCM2835_SPI0_CS/4;
    auto fifo = BCM2835_SPI0_FIFO/4;

    /* Clear TX and RX fifos */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

	/* Maybe wait for TXD */
	while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
	    ;

	/* Write to FIFO */
	bcm2835_peri_write_nb(fifo,  (uint32_t) data >> 8);
	bcm2835_peri_write_nb(fifo,  data & 0xFF);


    /* Wait for DONE to be set */
    while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE))
	;

    /* Set TA = 0, and also set the barrier */
    bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
}

int bcm2835_aux_spi_begin(void)
{
    auto enable = BCM2835_AUX_ENABLE/4;
    auto cntl0 = BCM2835_AUX_SPI_CNTL0/4;
    auto cntl1 = BCM2835_AUX_SPI_CNTL1/4;

    bcm2835_aux_spi_setClockDivider(bcm2835_aux_spi_CalcClockDivider(1000000));	// Default 1MHz SPI

    bcm2835_peri_write(enable, BCM2835_AUX_ENABLE_SPI0);
    bcm2835_peri_write(cntl1, 0);
    bcm2835_peri_write(cntl0, BCM2835_AUX_SPI_CNTL0_CLEARFIFO);

    return 1; /* OK */
}

#define DIV_ROUND_UP(n,d)	(((n) + (d) - 1) / (d))

uint16_t bcm2835_aux_spi_CalcClockDivider(uint32_t speed_hz)
{
    uint16_t divider;

    if (speed_hz < (uint32_t) BCM2835_AUX_SPI_CLOCK_MIN) {
	speed_hz = (uint32_t) BCM2835_AUX_SPI_CLOCK_MIN;
    } else if (speed_hz > (uint32_t) BCM2835_AUX_SPI_CLOCK_MAX) {
	speed_hz = (uint32_t) BCM2835_AUX_SPI_CLOCK_MAX;
    }

    divider = (uint16_t) DIV_ROUND_UP(BCM2835_CORE_CLK_HZ, 2 * speed_hz) - 1;

    if (divider > (uint16_t) BCM2835_AUX_SPI_CNTL0_SPEED_MAX) {
	return (uint16_t) BCM2835_AUX_SPI_CNTL0_SPEED_MAX;
    }

    return divider;
}

static uint32_t spi1_speed;

void bcm2835_aux_spi_setClockDivider(uint16_t divider)
{
    spi1_speed = (uint32_t) divider;
}

void bcm2835_aux_spi_write(uint16_t data)
{
    auto cntl0 = BCM2835_AUX_SPI_CNTL0/4;
    auto cntl1 = BCM2835_AUX_SPI_CNTL1/4;
    auto stat = BCM2835_AUX_SPI_STAT/4;
    auto io = BCM2835_AUX_SPI_IO/4;

    uint32_t _cntl0 = (spi1_speed << BCM2835_AUX_SPI_CNTL0_SPEED_SHIFT);
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_CS2_N;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_ENABLE;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_MSBF_OUT;
    _cntl0 |= 16; // Shift length

    bcm2835_peri_write(cntl0, _cntl0);
    bcm2835_peri_write(cntl1, BCM2835_AUX_SPI_CNTL1_MSBF_IN);

    while (bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_TX_FULL)
	;

    bcm2835_peri_write(io, (uint32_t) data << 16);
}

void bcm2835_aux_spi_writenb(uint32_t *tbuf, uint32_t len) {
    auto cntl0 = BCM2835_AUX_SPI_CNTL0/4;
    auto cntl1 = BCM2835_AUX_SPI_CNTL1/4;
    auto stat = BCM2835_AUX_SPI_STAT/4;
    auto txhold = BCM2835_AUX_SPI_TXHOLD/4;
    auto io = BCM2835_AUX_SPI_IO/4;

    char *tx = (char *) tbuf;
    uint32_t tx_len = len;
    uint32_t count;
    uint32_t data;
    uint32_t i;
    uint8_t byte;

    uint32_t _cntl0 = (spi1_speed << BCM2835_AUX_SPI_CNTL0_SPEED_SHIFT);
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_CS2_N;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_ENABLE;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_MSBF_OUT;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_VAR_WIDTH;

    bcm2835_peri_write(cntl0, _cntl0);
    bcm2835_peri_write(cntl1, BCM2835_AUX_SPI_CNTL1_MSBF_IN);

    while (tx_len > 0) {

	while (bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_TX_FULL)
	    ;

	count = MIN(tx_len, 3);
	data = 0;

	for (i = 0; i < count; i++) {
	    byte = (tx != NULL) ? (uint8_t) *tx++ : (uint8_t) 0;
	    data |= byte << (8 * (2 - i));
	}

	data |= (count * 8) << 24;
	tx_len -= count;

	if (tx_len != 0) {
	    bcm2835_peri_write(txhold, data);
	} else {
	    bcm2835_peri_write(io, data);
	}

	while (bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_BUSY)
	    ;

	(void) bcm2835_peri_read(io);
    }
}

uint32_t bcm2835_aux_spi_readnb(uint32_t* buf, uint32_t len) {
    auto cntl0 = BCM2835_AUX_SPI_CNTL0/4;
    auto cntl1 = BCM2835_AUX_SPI_CNTL1/4;
    auto stat = BCM2835_AUX_SPI_STAT/4;
    auto txhold = BCM2835_AUX_SPI_TXHOLD/4;
    auto io = BCM2835_AUX_SPI_IO/4;

    uint32_t rx_len = len;
    uint32_t count = 0;
    uint32_t data;
    char* rx = reinterpret_cast<char*>(buf);


    uint32_t _cntl0 = (spi1_speed << BCM2835_AUX_SPI_CNTL0_SPEED_SHIFT);
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_CS2_N;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_ENABLE;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_MSBF_OUT;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_VAR_WIDTH;

    bcm2835_peri_write(cntl0, _cntl0);
    bcm2835_peri_write(cntl1, BCM2835_AUX_SPI_CNTL1_MSBF_IN);

    uint32_t read_bytes;

    while (!(bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_BUSY) && (rx_len > 0)) {
            count = MIN(len, 3);
            data = bcm2835_peri_read(io);

            if (buf != NULL) {
                switch (count) {
                case 3:
                    *rx++ = (char)((data >> 16) & 0xFF);
                    /*@fallthrough@*/
                    /* no break */
                case 2:
                    *rx++ = (char)((data >> 8) & 0xFF);
                    /*@fallthrough@*/
                    /* no break */
                case 1:
                    *rx++ = (char)((data >> 0) & 0xFF);
                }
            }

            read_bytes += count;

    rx_len -= count;
    
    }
    return read_bytes;
}

void bcm2835_aux_spi_transfernb(const char *tbuf, char *rbuf, uint32_t len) {
    auto cntl0 = BCM2835_AUX_SPI_CNTL0/4;
    auto cntl1 = BCM2835_AUX_SPI_CNTL1/4;
    auto stat = BCM2835_AUX_SPI_STAT/4;
    auto txhold = BCM2835_AUX_SPI_TXHOLD/4;
    auto io = BCM2835_AUX_SPI_IO/4;

	char *tx = (char *)tbuf;
	char *rx = (char *)rbuf;
	uint32_t tx_len = len;
    uint32_t rx_len = len;
	
	uint32_t count;
	uint32_t data;
	uint32_t i;
	uint8_t byte;

	uint32_t _cntl0 = (spi1_speed << BCM2835_AUX_SPI_CNTL0_SPEED_SHIFT);
	_cntl0 |= BCM2835_AUX_SPI_CNTL0_CS2_N;
	_cntl0 |= BCM2835_AUX_SPI_CNTL0_ENABLE;
	_cntl0 |= BCM2835_AUX_SPI_CNTL0_MSBF_OUT;
	_cntl0 |= BCM2835_AUX_SPI_CNTL0_VAR_WIDTH;

	bcm2835_peri_write(cntl0, _cntl0);
	bcm2835_peri_write(cntl1, BCM2835_AUX_SPI_CNTL1_MSBF_IN);

	while ((tx_len > 0) || (rx_len > 0)) {

		while (!(bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_TX_FULL) && (tx_len > 0)) {
			count = MIN(tx_len, 3);
			data = 0;

			for (i = 0; i < count; i++) {
				byte = (tx != NULL) ? (uint8_t) *tx++ : (uint8_t) 0;
				data |= byte << (8 * (2 - i));
			}

			data |= (count * 8) << 24;
			tx_len -= count;

			if (tx_len != 0) {
				bcm2835_peri_write(txhold, data);
			} else {
				bcm2835_peri_write(io, data);
			}

		}

		while (!(bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_RX_EMPTY) && (rx_len > 0)) {
			count = MIN(rx_len, 3);
			data = bcm2835_peri_read(io);

			if (rbuf != NULL) {
				switch (count) {
				case 3:
					*rx++ = (char)((data >> 16) & 0xFF);
					/*@fallthrough@*/
					/* no break */
				case 2:
					*rx++ = (char)((data >> 8) & 0xFF);
					/*@fallthrough@*/
					/* no break */
				case 1:
					*rx++ = (char)((data >> 0) & 0xFF);
				}
			}

			rx_len -= count;
		}

	 	while (!(bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_BUSY) && (rx_len > 0)) {
			count = MIN(rx_len, 3);
			data = bcm2835_peri_read(io);

			if (rbuf != NULL) {
				switch (count) {
				case 3:
					*rx++ = (char)((data >> 16) & 0xFF);
					/*@fallthrough@*/
					/* no break */
				case 2:
					*rx++ = (char)((data >> 8) & 0xFF);
					/*@fallthrough@*/
					/* no break */
				case 1:
					*rx++ = (char)((data >> 0) & 0xFF);
				}
			}

			rx_len -= count;
		}
	}
}

void bcm2835_aux_spi_transfern(char *buf, uint32_t len) {
	bcm2835_aux_spi_transfernb(buf, buf, len);
}

/* Writes (and reads) a single byte to AUX SPI */
uint8_t bcm2835_aux_spi_transfer(uint8_t value)
{
    auto cntl0 = BCM2835_AUX_SPI_CNTL0/4;
    auto cntl1 = BCM2835_AUX_SPI_CNTL1/4;
    auto stat = BCM2835_AUX_SPI_STAT/4;
    auto io = BCM2835_AUX_SPI_IO/4;

    uint32_t data;

    uint32_t _cntl0 = (spi1_speed << BCM2835_AUX_SPI_CNTL0_SPEED_SHIFT);
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_CS2_N;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_ENABLE;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_MSBF_OUT;
    _cntl0 |= BCM2835_AUX_SPI_CNTL0_CPHA_IN;
    _cntl0 |= 8; // Shift length.

    uint32_t _cntl1 = BCM2835_AUX_SPI_CNTL1_MSBF_IN;

    bcm2835_peri_write(cntl1, _cntl1);
    bcm2835_peri_write(cntl0, _cntl0);

    bcm2835_peri_write(io, (uint32_t) bcm2835_correct_order(value) << 24);

    while (bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_BUSY)
        ;

    data = bcm2835_correct_order(bcm2835_peri_read(io) & 0xff);

    bcm2835_aux_spi_reset();

    return data;
}

uint32_t bcm2835_aux_spi_read() {
    auto stat = BCM2835_AUX_SPI_STAT/4;
    auto io = BCM2835_AUX_SPI_IO/4;

    while (bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_BUSY) {}

    auto data = bcm2835_correct_order(bcm2835_peri_read(io) & 0xff);

    bcm2835_aux_spi_reset();

    return data;
}

