#pragma once

#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>

struct SPI : L4::Kobject_t<SPI, L4::Kobject> {
    L4_INLINE_RPC(int, read, (l4_uint32_t *res));
    L4_INLINE_RPC(int, write, (l4_uint32_t *data));
   	L4_INLINE_RPC(int, reads, (l4_uint32_t *res, l4_uint32_t size));
    L4_INLINE_RPC(int, writes, (l4_uint32_t *data, l4_uint32_t size));

    typedef L4::Typeid::Rpcs<read_t, write_t, reads_t, writes_t> Rpcs;
};

