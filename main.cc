#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/sys/cxx/ipc_epiface>

#include "bcm2835.h"
#include "spi.h"
#include "spi_driver.h"

using L4Re::chksys;
using L4Re::chkcap;

class SPI_Server : public L4::Epiface_t<SPI_Server, SPI> {
    public:
        int op_read(SPI::Rights, l4_uint32_t &res) {
            res = bcm2835_spi_read();

            return L4_EOK;
        }

        int op_write(SPI::Rights, l4_uint32_t &data) {
            bcm2835_spi_write(data);

            return L4_EOK;
        }
        int op_reads(SPI::Rights, l4_uint32_t &res, l4_uint32_t size) {
            res = bcm2835_spi_readnb(size);

            return L4_EOK;
        }

        int op_writes(SPI::Rights, l4_uint32_t &data, l4_uint32_t size) {
            bcm2835_spi_write(data, size);

            return L4_EOK;
        }
};

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;
L4::Io_register_block_mmio* spi;


int main(void) {
    auto vbus = chkcap(L4Re::Env::env()->get_cap<L4vbus::Vbus>("vbus"), "vbus cap not valid");

    unsigned long vaddr;
    chksys(L4Re::Env::env()->rm()->attach(&vaddr, 
                                                1, 
                                                L4Re::Rm::F::Search_addr
                                                | L4Re::Rm::F::Cache_uncached                                                           | L4Re::Rm::F::RW,
                                                L4::Ipc::make_cap_rw(vbus), 
                                                0xFE215080, // same address as in io config
                                                L4_PAGESHIFT),
                    "Attach MMIO.");
    spi = new L4::Io_register_block_mmio(vaddr);

    static SPI_Server spiserver;

    if (! server.registry()->register_obj(&spiserver, "spi").is_valid()) {
        printf("Error while registering server object");

        return -1;
    }
    printf("start spi_driver server loop\n");
    server.loop();

    bcm2835_aux_spi_reset();

    return 0;
}
