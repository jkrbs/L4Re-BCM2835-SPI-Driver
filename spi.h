#pragma once

#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>

enum { SPI_PROTO = 0x44 };

struct SPI : L4::Kobject_t<SPI, L4::Kobject, SPI_PROTO> {
  L4_INLINE_RPC(int, transfer,
                (l4_uint8_t * tbuf, l4_uint8_t *rbuf, l4_uint32_t size));

  typedef L4::Typeid::Rpcs<transfer_t> Rpcs;
};
