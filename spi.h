#pragma once

#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>

struct SPI : L4::Kobject_t<SPI, L4::Kobject, 0x44> {
    L4_INLINE_RPC(int, read, (l4_uint32_t *res));
    L4_INLINE_RPC(int, write, (l4_uint32_t data));

    typedef L4::Typeid::Rpcs<read_t, write_t> Rpcs;
};

