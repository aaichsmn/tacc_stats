#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include "stats.h"
#include "trace.h"
#include "pscanf.h"
#include "pci.h"

#define BOX_CTL        0xF4

#define CTL0           0xD8
#define CTL1           0xDC
#define CTL2           0xE0
#define CTL3           0xE4

#define B_CTR0         0xA0
#define A_CTR0         0xA4
#define B_CTR1         0xA8
#define A_CTR1         0xAC
#define B_CTR2         0xB0
#define A_CTR2         0xB4
#define B_CTR3         0xB8
#define A_CTR3         0xBC

#define FIXED_CTL      0xF0
#define B_FIXED_CTR    0xD0
#define A_FIXED_CTR    0xD4

#define G_CTL_KEYS   \
  X(CTL0),	   \
    X(CTL1),	   \
    X(CTL2),	   \
    X(CTL3)

#define G_CTR_KEYS   \
  X(CTR0),	   \
    X(CTR1),	   \
    X(CTR2),	   \
    X(CTR3)

static void intel_snb_uncore_collect_dev(struct stats_type *type, char *bus_dev)
{
  struct stats *stats = NULL;
  char pci_path[80];
  int pci_fd = -1;

  stats = get_current_stats(type, bus_dev);
  if (stats == NULL)
    goto out;

  TRACE("bus/dev %s\n", bus_dev);

  snprintf(pci_path, sizeof(pci_path), "/proc/bus/pci/%s", bus_dev);
  pci_fd = open(pci_path, O_RDONLY);
  if (pci_fd < 0) {
    ERROR("cannot open `%s': %m\n", pci_path);
    goto out;
  }

#define X(k,r...) \
  ({ \
    uint32_t val; \
    if ( pread(pci_fd, &val, sizeof(val), k) < 0) \
      ERROR("cannot read `%s' (%08X) through `%s': %m\n", #k, k, pci_path); \
    else \
      stats_set(stats, #k, val);	\
  })
  G_CTL_KEYS;
#undef X

#define X(k,r...) \
  ({ \
    uint32_t val_a, val_b; \
    uint64_t val = 0x0ULL; \
    if ( pread(pci_fd, &val_a, sizeof(val_a), A_##k) < 0 || pread(pci_fd, &val_b, sizeof(val_b), B_##k) < 0 ) \
      ERROR("cannot read `%s' (%08X,%08X) through `%s': %m\n", #k, A_##k, B_##k, pci_path); \
    else \
      val = val_a; stats_set(stats, #k, (val<<32) + val_b);	\
  })
  G_CTR_KEYS;
  if (strcmp(type->st_name, "intel_snb_imc") == 0)
    X(FIXED_CTR);
#undef X

 out:
  if (pci_fd >= 0)
    close(pci_fd);
}
