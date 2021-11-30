#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/irq>
#include <l4/re/util/cap_alloc>
#include "bcm2835.h"
#include "spi.h"
#include "spi_driver.h"

using L4Re::chkcap;
using L4Re::chksys;

L4::Cap<L4vbus::Vbus> vbus;

class SPI_Server : public L4::Epiface_t<SPI_Server, SPI> {
public:
  int op_transfer(SPI::Rights, l4_uint8_t &tbuf, l4_uint8_t &rbuf,
                  l4_uint32_t size) {
    bcm2835_spi_transfernb((char *)&tbuf, (char *)&rbuf, size);
    return L4_EOK;
  };

  int op_register_irq(SPI::Rights, L4::Ipc::Snd_fpage const &irq) {
    if(! irq.cap_received()) {
      printf("failed to recieve irq cap");
      return L4_EINVAL;
    }

    L4::Cap<L4::Irq> rirq = chkcap(server_iface()->rcv_cap<L4::Irq>(0), "failed to recieve irq");
    chksys(server_iface()->realloc_rcv_cap(0), "failed to reallocate cap");

    vbus->bind(54, rirq);

    return L4_EOK;
  }
};

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;
L4::Io_register_block_mmio *spi;

int main(void) {
  printf("starting spi driver\n");
  L4::Cap<L4vbus::Vbus> vbus = chkcap(L4Re::Env::env()->get_cap<L4vbus::Vbus>("vbus"),
                     "vbus cap not valid");

  unsigned long vaddr;
  chksys(L4Re::Env::env()->rm()->attach(
             &vaddr, 0xfe2150ff - 0xfe200000,
             L4Re::Rm::F::Search_addr | L4Re::Rm::F::Cache_uncached |
                 L4Re::Rm::F::RW,
             L4::Ipc::make_cap_rw(vbus),
             0xfe200000, // same address as in io config
             L4_PAGESHIFT),
         "Attach MMIO.");
  spi = new L4::Io_register_block_mmio(vaddr);
  printf("registered mmio block\n");

  SPI_Server spiserver;

  if (!server.registry()->register_obj(&spiserver, "spi").is_valid()) {
    printf("Error while registering server object");

    return -1;
  }
  bcm2835_init();
  if (!bcm2835_spi_begin()) {
    printf("bcm2835_spi_begin failed. Are you running as root??\n");
    return 1;
  }
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_65536); // The default
  bcm2835_spi_chipSelect(BCM2835_SPI_CS1);                      // The default
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS1, LOW);      // the default
  printf("start spi_driver server loop\n");
  server.loop();

  return 0;
}
