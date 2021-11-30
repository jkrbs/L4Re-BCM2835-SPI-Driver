#pragma once

#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>
#include <l4/sys/cxx/ipc_types>

enum { SPI_PROTO = 0x44 };

struct SPI : L4::Kobject_t<SPI, L4::Kobject, SPI_PROTO> {
  L4_INLINE_RPC(int, transfer,
                (l4_uint8_t * tbuf, l4_uint8_t *rbuf, l4_uint32_t size));
  L4_INLINE_RPC(int, register_irq, (L4::Ipc::Cap<L4::Irq> irq));
  typedef L4::Typeid::Rpcs<transfer_t, register_irq_t> Rpcs;
};
