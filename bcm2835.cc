/* bcm2835.c
// C and C++ support for Broadcom BCM 2835 as used in Raspberry Pi
// http://elinux.org/RPi_Low-level_peripherals
//
http://www.raspberrypi.org/wp-content/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
//
// Author: Mike McCauley
// Copyright (C) 2011-2013 Mike McCauley
// $Id: bcm2835.c,v 1.28 2020/01/11 05:07:13 mikem Exp mikem $
*/

// Needed to compile with gcc -std=c99, as reported by John Blaiklock.
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BCK2835_LIBRARY_BUILD
#include "bcm2835.h"

/* This define enables a little test program (by default a blinking output on
pin RPI_GPIO_PIN_11)
// You can do some safe, non-destructive testing on any platform with:
// gcc bcm2835.c -D BCM2835_TEST
// ./a.out
*/
/*#define BCM2835_TEST*/

/* Uncommenting this define compiles alternative I2C code for the version 1 RPi
// The P1 header I2C pins are connected to SDA0 and SCL0 on V1.
// By default I2C code is generated for the V2 RPi which has SDA1 and SCL1
connected.
*/
/* #define I2C_V1*/

/* Physical address and size of the peripherals block
// May be overridden on RPi2
*/
off_t bcm2835_peripherals_base = BCM2835_PERI_BASE;
size_t bcm2835_peripherals_size = BCM2835_PERI_SIZE;

/* Virtual memory address of the mapped peripherals block
 */
uint32_t *bcm2835_peripherals = (uint32_t *)MAP_FAILED;

/* And the register bases within the peripherals block
 */
volatile uint32_t *bcm2835_gpio = (uint32_t *)MAP_FAILED;
volatile uint32_t *bcm2835_clk = (uint32_t *)MAP_FAILED;
volatile uint32_t *bcm2835_pads = (uint32_t *)MAP_FAILED;
volatile uint32_t *bcm2835_spi0 = (uint32_t *)MAP_FAILED;
volatile uint32_t *bcm2835_st = (uint32_t *)MAP_FAILED;
volatile uint32_t *bcm2835_aux = (uint32_t *)MAP_FAILED;
volatile uint32_t *bcm2835_spi1 = (uint32_t *)MAP_FAILED;

/* This variable allows us to test on hardware other than RPi.
// It prevents access to the kernel memory, and does not do any peripheral
access
// Instead it prints out what it _would_ do if debug were 0
 */
static uint8_t debug = 0;

/* RPI 4 has different pullup registers - we need to know if we have that type
 */

static uint8_t pud_type_rpi4 = 0;

/* RPI 4 has different pullup operation - make backwards compat */

static uint8_t pud_compat_setting = BCM2835_GPIO_PUD_OFF;

/* SPI bit order. BCM2835 SPI0 only supports MSBFIRST, so we instead
 * have a software based bit reversal, based on a contribution by Damiano
 * Benedetti
 */
static uint8_t bcm2835_spi_bit_order = BCM2835_SPI_BIT_ORDER_MSBFIRST;
static uint8_t bcm2835_byte_reverse_table[] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0,
    0x30, 0xb0, 0x70, 0xf0, 0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 0x04, 0x84, 0x44, 0xc4,
    0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc,
    0x3c, 0xbc, 0x7c, 0xfc, 0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 0x0a, 0x8a, 0x4a, 0xca,
    0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6,
    0x36, 0xb6, 0x76, 0xf6, 0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 0x01, 0x81, 0x41, 0xc1,
    0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9,
    0x39, 0xb9, 0x79, 0xf9, 0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 0x0d, 0x8d, 0x4d, 0xcd,
    0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3,
    0x33, 0xb3, 0x73, 0xf3, 0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 0x07, 0x87, 0x47, 0xc7,
    0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf,
    0x3f, 0xbf, 0x7f, 0xff};

static uint8_t bcm2835_correct_order(uint8_t b) {
  if (bcm2835_spi_bit_order == BCM2835_SPI_BIT_ORDER_LSBFIRST)
    return bcm2835_byte_reverse_table[b];
  else
    return b;
}

/*
// Low level register access functions
*/

/* Function to return the pointers to the hardware register bases */
uint32_t *bcm2835_regbase(uint8_t regbase) {
  switch (regbase) {
  case BCM2835_REGBASE_ST:
    return (uint32_t *)bcm2835_st;
  case BCM2835_REGBASE_GPIO:
    return (uint32_t *)bcm2835_gpio;
  case BCM2835_REGBASE_CLK:
    return (uint32_t *)bcm2835_clk;
  case BCM2835_REGBASE_PADS:
    return (uint32_t *)bcm2835_pads;
  case BCM2835_REGBASE_SPI0:
    return (uint32_t *)bcm2835_spi0;
  case BCM2835_REGBASE_BSC1:
    return (uint32_t *)bcm2835_st;
  case BCM2835_REGBASE_AUX:
    return (uint32_t *)bcm2835_aux;
  case BCM2835_REGBASE_SPI1:
    return (uint32_t *)bcm2835_spi1;
  }
  return (uint32_t *)MAP_FAILED;
}

void bcm2835_set_debug(uint8_t d) { debug = d; }

unsigned int bcm2835_version(void) { return BCM2835_VERSION; }

/* Set/clear only the bits in value covered by the mask
 * This is not atomic - can be interrupted.
 */
void bcm2835_peri_set_bits(volatile uint32_t *paddr, uint32_t value,
                           uint32_t mask) {
  uint32_t v = bcm2835_peri_read(paddr);
  v = (v & ~mask) | (value & mask);
  bcm2835_peri_write(paddr, v);
}

/*
// Low level convenience functions
*/

/* Function select
// pin is a BCM2835 GPIO pin number NOT RPi pin number
//      There are 6 control registers, each control the functions of a block
//      of 10 pins.
//      Each control register has 10 sets of 3 bits per GPIO pin:
//
//      000 = GPIO Pin X is an input
//      001 = GPIO Pin X is an output
//      100 = GPIO Pin X takes alternate function 0
//      101 = GPIO Pin X takes alternate function 1
//      110 = GPIO Pin X takes alternate function 2
//      111 = GPIO Pin X takes alternate function 3
//      011 = GPIO Pin X takes alternate function 4
//      010 = GPIO Pin X takes alternate function 5
//
// So the 3 bits for port X are:
//      X / 10 + ((X % 10) * 3)
*/
void bcm2835_gpio_fsel(uint8_t pin, uint8_t mode) {
  /* Function selects are 10 pins per 32 bit word, 3 bits per pin */
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPFSEL0 / 4 + (pin / 10);
  uint8_t shift = (pin % 10) * 3;
  uint32_t mask = BCM2835_GPIO_FSEL_MASK << shift;
  uint32_t value = mode << shift;
  bcm2835_peri_set_bits(paddr, value, mask);
}

/* Set output pin */
void bcm2835_gpio_set(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPSET0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  bcm2835_peri_write(paddr, 1 << shift);
}

/* Clear output pin */
void bcm2835_gpio_clr(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPCLR0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  bcm2835_peri_write(paddr, 1 << shift);
}

/* Set all output pins in the mask */
void bcm2835_gpio_set_multi(uint32_t mask) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPSET0 / 4;
  bcm2835_peri_write(paddr, mask);
}

/* Clear all output pins in the mask */
void bcm2835_gpio_clr_multi(uint32_t mask) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPCLR0 / 4;
  bcm2835_peri_write(paddr, mask);
}

/* Read input pin */
uint8_t bcm2835_gpio_lev(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPLEV0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = bcm2835_peri_read(paddr);
  return (value & (1 << shift)) ? HIGH : LOW;
}

/* See if an event detection bit is set
// Sigh cant support interrupts yet
*/
uint8_t bcm2835_gpio_eds(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPEDS0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = bcm2835_peri_read(paddr);
  return (value & (1 << shift)) ? HIGH : LOW;
}

uint32_t bcm2835_gpio_eds_multi(uint32_t mask) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPEDS0 / 4;
  uint32_t value = bcm2835_peri_read(paddr);
  return (value & mask);
}

/* Write a 1 to clear the bit in EDS */
void bcm2835_gpio_set_eds(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPEDS0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_write(paddr, value);
}

void bcm2835_gpio_set_eds_multi(uint32_t mask) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPEDS0 / 4;
  bcm2835_peri_write(paddr, mask);
}

/* Rising edge detect enable */
void bcm2835_gpio_ren(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPREN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, value, value);
}
void bcm2835_gpio_clr_ren(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPREN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, 0, value);
}

/* Falling edge detect enable */
void bcm2835_gpio_fen(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPFEN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, value, value);
}
void bcm2835_gpio_clr_fen(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPFEN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, 0, value);
}

/* High detect enable */
void bcm2835_gpio_hen(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPHEN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, value, value);
}
void bcm2835_gpio_clr_hen(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPHEN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, 0, value);
}

/* Low detect enable */
void bcm2835_gpio_len(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPLEN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, value, value);
}
void bcm2835_gpio_clr_len(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPLEN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, 0, value);
}

/* Async rising edge detect enable */
void bcm2835_gpio_aren(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPAREN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, value, value);
}
void bcm2835_gpio_clr_aren(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPAREN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, 0, value);
}

/* Async falling edge detect enable */
void bcm2835_gpio_afen(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPAFEN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, value, value);
}
void bcm2835_gpio_clr_afen(uint8_t pin) {
  volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPAFEN0 / 4 + pin / 32;
  uint8_t shift = pin % 32;
  uint32_t value = 1 << shift;
  bcm2835_peri_set_bits(paddr, 0, value);
}

/* Set pullup/down */
void bcm2835_gpio_pud(uint8_t pud) {
  if (pud_type_rpi4) {
    pud_compat_setting = pud;
  } else {
    volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPPUD / 4;
    bcm2835_peri_write(paddr, pud);
  }
}

/* Pullup/down clock
// Clocks the value of pud into the GPIO pin
*/
void bcm2835_gpio_pudclk(uint8_t pin, uint8_t on) {
  if (pud_type_rpi4) {
    if (on)
      bcm2835_gpio_set_pud(pin, pud_compat_setting);
  } else {
    volatile uint32_t *paddr = bcm2835_gpio + BCM2835_GPPUDCLK0 / 4 + pin / 32;
    uint8_t shift = pin % 32;
    bcm2835_peri_write(paddr, (on ? 1 : 0) << shift);
  }
}

/* Read GPIO pad behaviour for groups of GPIOs */
uint32_t bcm2835_gpio_pad(uint8_t group) {
  if (bcm2835_pads == MAP_FAILED)
    return 0;

  volatile uint32_t *paddr = bcm2835_pads + BCM2835_PADS_GPIO_0_27 / 4 + group;
  return bcm2835_peri_read(paddr);
}

/* Set GPIO pad behaviour for groups of GPIOs
// powerup value for all pads is
// BCM2835_PAD_SLEW_RATE_UNLIMITED | BCM2835_PAD_HYSTERESIS_ENABLED |
BCM2835_PAD_DRIVE_8mA
*/
void bcm2835_gpio_set_pad(uint8_t group, uint32_t control) {
  if (bcm2835_pads == MAP_FAILED)
    return;

  volatile uint32_t *paddr = bcm2835_pads + BCM2835_PADS_GPIO_0_27 / 4 + group;
  bcm2835_peri_write(paddr, control | BCM2835_PAD_PASSWRD);
}

/* Some convenient arduino-like functions
// milliseconds
*/
void bcm2835_delay(unsigned int millis) {
  struct timespec sleeper;

  sleeper.tv_sec = (time_t)(millis / 1000);
  sleeper.tv_nsec = (long)(millis % 1000) * 1000000;
  nanosleep(&sleeper, NULL);
}

/* microseconds */
void bcm2835_delayMicroseconds(uint64_t micros) {
  struct timespec t1;
  uint64_t start;

  if (debug) {
    /* Cant access sytem timers in debug mode */
    printf("bcm2835_delayMicroseconds %lld\n", (long long int)micros);
    return;
  }

  /* Calling nanosleep() takes at least 100-200 us, so use it for
  // long waits and use a busy wait on the System Timer for the rest.
  */
  start = bcm2835_st_read();

  /* Not allowed to access timer registers (result is not as precise)*/
  if (start == 0) {
    t1.tv_sec = 0;
    t1.tv_nsec = 1000 * (long)(micros);
    nanosleep(&t1, NULL);
    return;
  }

  if (micros > 450) {
    t1.tv_sec = 0;
    t1.tv_nsec = 1000 * (long)(micros - 200);
    nanosleep(&t1, NULL);
  }

  bcm2835_st_delay(start, micros);
}

/*
// Higher level convenience functions
*/

/* Set the state of an output */
void bcm2835_gpio_write(uint8_t pin, uint8_t on) {
  if (on)
    bcm2835_gpio_set(pin);
  else
    bcm2835_gpio_clr(pin);
}

/* Set the state of a all 32 outputs in the mask to on or off */
void bcm2835_gpio_write_multi(uint32_t mask, uint8_t on) {
  if (on)
    bcm2835_gpio_set_multi(mask);
  else
    bcm2835_gpio_clr_multi(mask);
}

/* Set the state of a all 32 outputs in the mask to the values in value */
void bcm2835_gpio_write_mask(uint32_t value, uint32_t mask) {
  bcm2835_gpio_set_multi(value & mask);
  bcm2835_gpio_clr_multi((~value) & mask);
}

/* Set the pullup/down resistor for a pin
//
// The GPIO Pull-up/down Clock Registers control the actuation of internal
pull-downs on
// the respective GPIO pins. These registers must be used in conjunction with
the GPPUD
// register to effect GPIO Pull-up/down changes. The following sequence of
events is
// required:
// 1. Write to GPPUD to set the required control signal (i.e. Pull-up or
Pull-Down or neither
// to remove the current Pull-up/down)
// 2. Wait 150 cycles ? this provides the required set-up time for the control
signal
// 3. Write to GPPUDCLK0/1 to clock the control signal into the GPIO pads you
wish to
// modify ? NOTE only the pads which receive a clock will be modified, all
others will
// retain their previous state.
// 4. Wait 150 cycles ? this provides the required hold time for the control
signal
// 5. Write to GPPUD to remove the control signal
// 6. Write to GPPUDCLK0/1 to remove the clock
//
// RPi has P1-03 and P1-05 with 1k8 pullup resistor
//
// RPI 4 uses a different PUD method - no clock

*/
void bcm2835_gpio_set_pud(uint8_t pin, uint8_t pud) {
  if (pud_type_rpi4) {
    int shiftbits = (pin & 0xf) << 1;
    uint32_t bits;
    uint32_t pull;

    switch (pud) {
    case BCM2835_GPIO_PUD_OFF:
      pull = 0;
      break;
    case BCM2835_GPIO_PUD_UP:
      pull = 1;
      break;
    case BCM2835_GPIO_PUD_DOWN:
      pull = 2;
      break;
    default:
      return;
    }

    volatile uint32_t *paddr =
        bcm2835_gpio + BCM2835_GPPUPPDN0 / 4 + (pin >> 4);

    bits = bcm2835_peri_read_nb(paddr);
    bits &= ~(3 << shiftbits);
    bits |= (pull << shiftbits);

    bcm2835_peri_write_nb(paddr, bits);

  } else {
    bcm2835_gpio_pud(pud);
    delayMicroseconds(10);
    bcm2835_gpio_pudclk(pin, 1);
    delayMicroseconds(10);
    bcm2835_gpio_pud(BCM2835_GPIO_PUD_OFF);
    bcm2835_gpio_pudclk(pin, 0);
  }
}

uint8_t bcm2835_gpio_get_pud(uint8_t pin) {
  uint8_t ret = BCM2835_GPIO_PUD_ERROR;

  if (pud_type_rpi4) {
    uint32_t bits;
    volatile uint32_t *paddr =
        bcm2835_gpio + BCM2835_GPPUPPDN0 / 4 + (pin >> 4);
    bits = (bcm2835_peri_read_nb(paddr) >> ((pin & 0xf) << 1)) & 0x3;

    switch (bits) {
    case 0:
      ret = BCM2835_GPIO_PUD_OFF;
      break;
    case 1:
      ret = BCM2835_GPIO_PUD_UP;
      break;
    case 2:
      ret = BCM2835_GPIO_PUD_DOWN;
      break;
    default:
      ret = BCM2835_GPIO_PUD_ERROR;
    }
  }

  return ret;
}

static void bcm2835_aux_spi_reset(void) {
  volatile uint32_t *cntl0 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL0 / 4;
  volatile uint32_t *cntl1 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL1 / 4;

  bcm2835_peri_write(cntl1, 0);
  bcm2835_peri_write(cntl0, BCM2835_AUX_SPI_CNTL0_CLEARFIFO);
}

int bcm2835_spi_begin(void) {
  volatile uint32_t *paddr;

  if (bcm2835_spi0 == MAP_FAILED)
    return 0; /* bcm2835_init() failed, or not root */

  /* Set the SPI0 pins to the Alt 0 function to enable SPI0 access on them */
  bcm2835_gpio_fsel(RPI_GPIO_P1_26, BCM2835_GPIO_FSEL_ALT0); /* CE1 */
  bcm2835_gpio_fsel(RPI_GPIO_P1_24, BCM2835_GPIO_FSEL_ALT0); /* CE0 */
  bcm2835_gpio_fsel(RPI_GPIO_P1_21, BCM2835_GPIO_FSEL_ALT0); /* MISO */
  bcm2835_gpio_fsel(RPI_GPIO_P1_19, BCM2835_GPIO_FSEL_ALT0); /* MOSI */
  bcm2835_gpio_fsel(RPI_GPIO_P1_23, BCM2835_GPIO_FSEL_ALT0); /* CLK */

  /* Set the SPI CS register to the some sensible defaults */
  paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
  bcm2835_peri_write(paddr, 0); /* All 0s */

  /* Clear TX and RX fifos */
  bcm2835_peri_write_nb(paddr, BCM2835_SPI0_CS_CLEAR);

  return 1; // OK
}

void bcm2835_spi_end(void) {
  /* Set all the SPI0 pins back to input */
  bcm2835_gpio_fsel(RPI_GPIO_P1_26, BCM2835_GPIO_FSEL_INPT); /* CE1 */
  bcm2835_gpio_fsel(RPI_GPIO_P1_24, BCM2835_GPIO_FSEL_INPT); /* CE0 */
  bcm2835_gpio_fsel(RPI_GPIO_P1_21, BCM2835_GPIO_FSEL_INPT); /* MISO */
  bcm2835_gpio_fsel(RPI_GPIO_P1_19, BCM2835_GPIO_FSEL_INPT); /* MOSI */
  bcm2835_gpio_fsel(RPI_GPIO_P1_23, BCM2835_GPIO_FSEL_INPT); /* CLK */
}

void bcm2835_spi_setBitOrder(uint8_t order) { bcm2835_spi_bit_order = order; }

/* defaults to 0, which means a divider of 65536.
// The divisor must be a power of 2. Odd numbers
// rounded down. The maximum SPI clock rate is
// of the APB clock
*/
void bcm2835_spi_setClockDivider(uint16_t divider) {
  volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CLK / 4;
  bcm2835_peri_write(paddr, divider);
}

void bcm2835_spi_set_speed_hz(uint32_t speed_hz) {
  uint16_t divider = (uint16_t)((uint32_t)BCM2835_CORE_CLK_HZ / speed_hz);
  divider &= 0xFFFE;
  bcm2835_spi_setClockDivider(divider);
}

void bcm2835_spi_setDataMode(uint8_t mode) {
  volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
  /* Mask in the CPO and CPHA bits of CS */
  bcm2835_peri_set_bits(paddr, mode << 2,
                        BCM2835_SPI0_CS_CPOL | BCM2835_SPI0_CS_CPHA);
}

/* Writes (and reads) a single byte to SPI */
uint8_t bcm2835_spi_transfer(uint8_t value) {
  volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
  volatile uint32_t *fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO / 4;
  uint32_t ret;

  /* This is Polled transfer as per section 10.6.1
  // BUG ALERT: what happens if we get interupted in this section, and someone
  else
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

  /* Read any byte that was sent back by the slave while we sere sending to it
   */
  ret = bcm2835_correct_order(bcm2835_peri_read_nb(fifo));

  /* Set TA = 0, and also set the barrier */
  bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);

  return ret;
}

/* Writes (and reads) an number of bytes to SPI */
void bcm2835_spi_transfernb(const unsigned char *tbuf, unsigned char *rbuf, uint32_t len) {
  volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
  volatile uint32_t *fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO / 4;
  uint32_t TXCnt = 0;
  uint32_t RXCnt = 0;

  /* This is Polled transfer as per section 10.6.1
  // BUG ALERT: what happens if we get interupted in this section, and someone
  else
  // accesses a different peripheral?
  */

  /* Clear TX and RX fifos */
  bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

  /* Set TA = 1 */
  bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

  /* Use the FIFO's to reduce the interbyte times */
  while ((TXCnt < len) || (RXCnt < len)) {
    /* TX fifo not full, so add some more bytes */
    while (((bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD)) &&
           (TXCnt < len)) {
      bcm2835_peri_write_nb(fifo, bcm2835_correct_order(tbuf[TXCnt]));
      TXCnt++;
    }
    /* Rx fifo not empty, so get the next received bytes */
    while (((bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_RXD)) &&
           (RXCnt < len)) {
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
void bcm2835_spi_writenb(const char *tbuf, uint32_t len) {
  volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
  volatile uint32_t *fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO / 4;
  uint32_t i;

  /* This is Polled transfer as per section 10.6.1
  // BUG ALERT: what happens if we get interupted in this section, and someone
  else
  // accesses a different peripheral?
  // Answer: an ISR is required to issue the required memory barriers.
  */

  /* Clear TX and RX fifos */
  bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

  /* Set TA = 1 */
  bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

  for (i = 0; i < len; i++) {
    /* Maybe wait for TXD */
    while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
      ;

    /* Write to FIFO, no barrier */
    bcm2835_peri_write_nb(fifo, bcm2835_correct_order(tbuf[i]));

    /* Read from FIFO to prevent stalling */
    while (bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_RXD)
      (void)bcm2835_peri_read_nb(fifo);
  }

  /* Wait for DONE to be set */
  while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {
    while (bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_RXD)
      (void)bcm2835_peri_read_nb(fifo);
  };

  /* Set TA = 0, and also set the barrier */
  bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
}

/* Writes (and reads) an number of bytes to SPI
// Read bytes are copied over onto the transmit buffer
*/
void bcm2835_spi_transfern(unsigned char *buf, uint32_t len) {
  bcm2835_spi_transfernb(buf, buf, len);
}

void bcm2835_spi_chipSelect(uint8_t cs) {
  volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
  /* Mask in the CS bits of CS */
  bcm2835_peri_set_bits(paddr, cs, BCM2835_SPI0_CS_CS);
}

void bcm2835_spi_setChipSelectPolarity(uint8_t cs, uint8_t active) {
  volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
  uint8_t shift = 21 + cs;
  /* Mask in the appropriate CSPOLn bit */
  bcm2835_peri_set_bits(paddr, active << shift, 1 << shift);
}

void bcm2835_spi_write(uint16_t data) {
  volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
  volatile uint32_t *fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO / 4;

  /* Clear TX and RX fifos */
  bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

  /* Set TA = 1 */
  bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

  /* Maybe wait for TXD */
  while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
    ;

  /* Write to FIFO */
  bcm2835_peri_write_nb(fifo, (uint32_t)data >> 8);
  bcm2835_peri_write_nb(fifo, data & 0xFF);

  /* Wait for DONE to be set */
  while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE))
    ;

  /* Set TA = 0, and also set the barrier */
  bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
}

int bcm2835_aux_spi_begin(void) {
  volatile uint32_t *enable = bcm2835_aux + BCM2835_AUX_ENABLE / 4;
  volatile uint32_t *cntl0 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL0 / 4;
  volatile uint32_t *cntl1 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL1 / 4;

  if (bcm2835_spi1 == MAP_FAILED)
    return 0; /* bcm2835_init() failed, or not root */

  /* Set the SPI pins to the Alt 4 function to enable SPI1 access on them */
  bcm2835_gpio_fsel(RPI_V2_GPIO_P1_36, BCM2835_GPIO_FSEL_ALT4); /* SPI1_CE2_N */
  bcm2835_gpio_fsel(RPI_V2_GPIO_P1_35, BCM2835_GPIO_FSEL_ALT4); /* SPI1_MISO */
  bcm2835_gpio_fsel(RPI_V2_GPIO_P1_38, BCM2835_GPIO_FSEL_ALT4); /* SPI1_MOSI */
  bcm2835_gpio_fsel(RPI_V2_GPIO_P1_40, BCM2835_GPIO_FSEL_ALT4); /* SPI1_SCLK */

  bcm2835_aux_spi_setClockDivider(
      bcm2835_aux_spi_CalcClockDivider(1000000)); // Default 1MHz SPI

  bcm2835_peri_write(enable, BCM2835_AUX_ENABLE_SPI0);
  bcm2835_peri_write(cntl1, 0);
  bcm2835_peri_write(cntl0, BCM2835_AUX_SPI_CNTL0_CLEARFIFO);

  return 1; /* OK */
}

void bcm2835_aux_spi_end(void) {
  /* Set all the SPI1 pins back to input */
  bcm2835_gpio_fsel(RPI_V2_GPIO_P1_36, BCM2835_GPIO_FSEL_INPT); /* SPI1_CE2_N */
  bcm2835_gpio_fsel(RPI_V2_GPIO_P1_35, BCM2835_GPIO_FSEL_INPT); /* SPI1_MISO */
  bcm2835_gpio_fsel(RPI_V2_GPIO_P1_38, BCM2835_GPIO_FSEL_INPT); /* SPI1_MOSI */
  bcm2835_gpio_fsel(RPI_V2_GPIO_P1_40, BCM2835_GPIO_FSEL_INPT); /* SPI1_SCLK */
}

#define DIV_ROUND_UP(n, d) (((n) + (d)-1) / (d))

uint16_t bcm2835_aux_spi_CalcClockDivider(uint32_t speed_hz) {
  uint16_t divider;

  if (speed_hz < (uint32_t)BCM2835_AUX_SPI_CLOCK_MIN) {
    speed_hz = (uint32_t)BCM2835_AUX_SPI_CLOCK_MIN;
  } else if (speed_hz > (uint32_t)BCM2835_AUX_SPI_CLOCK_MAX) {
    speed_hz = (uint32_t)BCM2835_AUX_SPI_CLOCK_MAX;
  }

  divider = (uint16_t)DIV_ROUND_UP(BCM2835_CORE_CLK_HZ, 2 * speed_hz) - 1;

  if (divider > (uint16_t)BCM2835_AUX_SPI_CNTL0_SPEED_MAX) {
    return (uint16_t)BCM2835_AUX_SPI_CNTL0_SPEED_MAX;
  }

  return divider;
}

static uint32_t spi1_speed;

void bcm2835_aux_spi_setClockDivider(uint16_t divider) {
  spi1_speed = (uint32_t)divider;
}

void bcm2835_aux_spi_write(uint16_t data) {
  volatile uint32_t *cntl0 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL0 / 4;
  volatile uint32_t *cntl1 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL1 / 4;
  volatile uint32_t *stat = bcm2835_spi1 + BCM2835_AUX_SPI_STAT / 4;
  volatile uint32_t *io = bcm2835_spi1 + BCM2835_AUX_SPI_IO / 4;

  uint32_t _cntl0 = (spi1_speed << BCM2835_AUX_SPI_CNTL0_SPEED_SHIFT);
  _cntl0 |= BCM2835_AUX_SPI_CNTL0_CS2_N;
  _cntl0 |= BCM2835_AUX_SPI_CNTL0_ENABLE;
  _cntl0 |= BCM2835_AUX_SPI_CNTL0_MSBF_OUT;
  _cntl0 |= 16; // Shift length

  bcm2835_peri_write(cntl0, _cntl0);
  bcm2835_peri_write(cntl1, BCM2835_AUX_SPI_CNTL1_MSBF_IN);

  while (bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_TX_FULL)
    ;

  bcm2835_peri_write(io, (uint32_t)data << 16);
}

void bcm2835_aux_spi_writenb(const char *tbuf, uint32_t len) {
  volatile uint32_t *cntl0 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL0 / 4;
  volatile uint32_t *cntl1 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL1 / 4;
  volatile uint32_t *stat = bcm2835_spi1 + BCM2835_AUX_SPI_STAT / 4;
  volatile uint32_t *txhold = bcm2835_spi1 + BCM2835_AUX_SPI_TXHOLD / 4;
  volatile uint32_t *io = bcm2835_spi1 + BCM2835_AUX_SPI_IO / 4;

  char *tx = (char *)tbuf;
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
      byte = (tx != NULL) ? (uint8_t)*tx++ : (uint8_t)0;
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

    (void)bcm2835_peri_read(io);
  }
}

void bcm2835_aux_spi_transfernb(const char *tbuf, char *rbuf, uint32_t len) {
  volatile uint32_t *cntl0 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL0 / 4;
  volatile uint32_t *cntl1 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL1 / 4;
  volatile uint32_t *stat = bcm2835_spi1 + BCM2835_AUX_SPI_STAT / 4;
  volatile uint32_t *txhold = bcm2835_spi1 + BCM2835_AUX_SPI_TXHOLD / 4;
  volatile uint32_t *io = bcm2835_spi1 + BCM2835_AUX_SPI_IO / 4;

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

    while (!(bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_TX_FULL) &&
           (tx_len > 0)) {
      count = MIN(tx_len, 3);
      data = 0;

      for (i = 0; i < count; i++) {
        byte = (tx != NULL) ? (uint8_t)*tx++ : (uint8_t)0;
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

    while (!(bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_RX_EMPTY) &&
           (rx_len > 0)) {
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

    while (!(bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_BUSY) &&
           (rx_len > 0)) {
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
uint8_t bcm2835_aux_spi_transfer(uint8_t value) {
  volatile uint32_t *cntl0 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL0 / 4;
  volatile uint32_t *cntl1 = bcm2835_spi1 + BCM2835_AUX_SPI_CNTL1 / 4;
  volatile uint32_t *stat = bcm2835_spi1 + BCM2835_AUX_SPI_STAT / 4;
  volatile uint32_t *io = bcm2835_spi1 + BCM2835_AUX_SPI_IO / 4;

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

  bcm2835_peri_write(io, (uint32_t)bcm2835_correct_order(value) << 24);

  while (bcm2835_peri_read(stat) & BCM2835_AUX_SPI_STAT_BUSY)
    ;

  data = bcm2835_correct_order(bcm2835_peri_read(io) & 0xff);

  bcm2835_aux_spi_reset();

  return data;
}
uint64_t bcm2835_st_read(void) {
  volatile uint32_t *paddr;
  uint32_t hi, lo;
  uint64_t st;

  if (bcm2835_st == MAP_FAILED)
    return 0;

  paddr = bcm2835_st + BCM2835_ST_CHI / 4;
  hi = bcm2835_peri_read(paddr);

  paddr = bcm2835_st + BCM2835_ST_CLO / 4;
  lo = bcm2835_peri_read(paddr);

  paddr = bcm2835_st + BCM2835_ST_CHI / 4;
  st = bcm2835_peri_read(paddr);

  /* Test for overflow */
  if (st == hi) {
    st <<= 32;
    st += lo;
  } else {
    st <<= 32;
    paddr = bcm2835_st + BCM2835_ST_CLO / 4;
    st += bcm2835_peri_read(paddr);
  }
  return st;
}

/* Delays for the specified number of microseconds with offset */
void bcm2835_st_delay(uint64_t offset_micros, uint64_t micros) {
  uint64_t compare = offset_micros + micros;

  while (bcm2835_st_read() < compare)
    ;
}

/* Initialise this library. */
int bcm2835_init(void) {

  // here the offset into the dataspace
  bcm2835_peripherals = 0;

  bcm2835_pads = bcm2835_peripherals + BCM2835_GPIO_PADS / 4;
  bcm2835_clk = bcm2835_peripherals + BCM2835_CLOCK_BASE / 4;
  bcm2835_gpio = bcm2835_peripherals + BCM2835_GPIO_BASE / 4;
  bcm2835_spi0 = bcm2835_peripherals + BCM2835_SPI0_BASE / 4;
  bcm2835_st = bcm2835_peripherals + BCM2835_ST_BASE / 4;
  bcm2835_aux = bcm2835_peripherals + BCM2835_AUX_BASE / 4;
  bcm2835_spi1 = bcm2835_peripherals + BCM2835_SPI1_BASE / 4;

  return 1; /* Success */
}
/* Close this library and deallocate everything */
int bcm2835_close(void) { return 1; /* Success */ }
