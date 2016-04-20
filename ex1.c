// Example of using libdevmem 1.0
//
// In the makefile:
//    DEVMEM_DIR=/path/to/src-of/devmem
//    CFLAGS += $(shell $(DEVMEM_DIR)/libdevmem-config --cflags)
//    LIBS   += $(shell $(DEVMEM_DIR)/libdevmem-config --libs)
//    SRC = prog.c
//    prog: $(SRC)
//	  $(CC) -o prog $(CFLAGS) $(LDFLAGS) $(LIBS) $(SRC)
//

#include <libdevmem.h> /* the libdevmem header */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <assert.h>

// Defines for your device do here ...
#define MY_MMAP_OFF    0        // Offset of my mapping from the physical window
#define MY_MMAP_SIZE   0x430000 // Size of my mapping
#define SIGNATURE_OFF  0x400000 // offset from the mapping base address
#define MAGICK         5
#define TEST_OFF       0x000000 // offset from the mapping base address

int main(int argc, char **argv)
{
   int rc = 0;
   uint32_t v;

   struct dmem_mapping_s dmap = {
      .map_addr = 0,  // relative from phys. window base address
      .map_size = MY_MMAP_SIZE, 
      .flags = MF_READONLY,  //! remove readonly if need to write
      };

    struct dmem_mapping_s *dm = &dmap;

   // Note: you don't specify the physical memory base address here.
   // It should be defined in environment variable, 
   // and libdevmem will read it automatically.

   // Enable debug messages to see any errors...
   // dmem_set_debug(1, stderr);
   
   rc = dmem_mapping_map(dm);
   if (rc != 0) {
      printf("Cannot access device memory!\n");
      return 1;
   }
   
   printf("Mapped virt.addr=%p\n", dm->map_ptr);
   
   v = dmem_read32(dm, SIGNATURE_OFF); // check that we see the device
   if (v != MAGICK) {
      printf("The signature is invalid: %#X\n", v);
      return 2;
   }

   dmem_write32(dm, TEST_OFF, 0x42);
   v = dmem_read32(dm, TEST_OFF);
   printf("Read back: %#X\n", v);

   // Here you can call dmem_mapping_unmap and dmem_finalize, but this is 
   // not required under Linux. At exit, the memory will unmap automatically.

   return 0;
}

