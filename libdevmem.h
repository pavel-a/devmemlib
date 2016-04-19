/**
* Library for physical memory access like in devmem
* 32-bit physical address version 
*
* pa03 07-apr-2016 
*/

#ifndef libdevmem_h_
#define libdevmem_h_

#include <stdlib.h>
#include <stdint.h> 
#include <stdio.h> /* for debug prints to FILE */
 

typedef uint32_t dmem_phys_address_t;
typedef uint32_t dmem_mapping_size_t;

struct dmem_mapping_s {
    unsigned flags;               // in out dmem_mapping_flags
    dmem_phys_address_t map_addr; // in
    dmem_mapping_size_t map_size; // in
    char * const map_ptr;         // out  !Add volatile!
    intptr_t reserved[10];
};

enum dmem_mapping_flags {
    MF_ABSOLUTE = 0x01, // Absolute physical address, not offset
    MF_READONLY = 0x02,
};
 
typedef struct dmem_mapping_s *dmem_mapping_hnd_t;

#ifdef __cplusplus
extern "C" {
#endif 

// Initialize:
// int dmem_init(void);
int dmem__init_(int addrsize, int mapsize, void *reserved);
#define dmem_init() \
        dmem__init_(sizeof(dmem_phys_address_t), sizeof(dmem_mapping_size_t), 0)

// Map:
// @param[in] params - struct dmem_mapping_s, caller fills in required fields
// @return error code
int dmem_mapping_map(struct dmem_mapping_s *params);

// Unmap:
// @param[in] params - struct dmem_mapping_s, after successfull dmem_mapping_map()
// @return error code
int dmem_mapping_unmap(struct dmem_mapping_s *params);

// Call this last
int dmem_finalize(void);

// Extras...
int dmem_set_debug(int flags, FILE *to);
int dmem_get_version(void);
int dmem_print_version(void); // via debug print

#ifndef LIBDEVMEM_NO_EXTRAS
// I/O ops with validation, using the param struct:

void      dmem_write32(struct dmem_mapping_s *dp, dmem_mapping_size_t off, uint32_t v);
uint32_t  dmem_read32(struct dmem_mapping_s *dp,  dmem_mapping_size_t off);
void      dmem_write16(struct dmem_mapping_s *dp, dmem_mapping_size_t off, uint16_t v);
uint16_t  dmem_read16(struct dmem_mapping_s *dp,  dmem_mapping_size_t off);
void      dmem_write8(struct dmem_mapping_s *dp,  dmem_mapping_size_t off, uint8_t v);
uint8_t   dmem_read8(struct dmem_mapping_s *dp,   dmem_mapping_size_t off);

void      dmem_write_buf32(struct dmem_mapping_s *dp, const uint32_t *buf, dmem_mapping_size_t off, unsigned cnt);
void      dmem_read_buf32(struct dmem_mapping_s  *dp,       uint32_t *buf, dmem_mapping_size_t off, unsigned cnt);
void      dmem_write_buf16(struct dmem_mapping_s *dp, const uint16_t *buf, dmem_mapping_size_t off, unsigned cnt);
void      dmem_read_buf16(struct dmem_mapping_s *dp,        uint16_t *buf, dmem_mapping_size_t off, unsigned cnt);
void      dmem_write_buf8(struct dmem_mapping_s *dp,  const uint8_t  *buf, dmem_mapping_size_t off, unsigned cnt);
void      dmem_read_buf8(struct dmem_mapping_s *dp,         uint8_t  *buf, dmem_mapping_size_t off, unsigned cnt);

// I/O ops via pointer, quick
void      dmem_write32p(void *mp, uint32_t v);
void      dmem_write16p(void *mp, uint16_t v);
void      dmem_write8p( void *mp, uint8_t v);
uint32_t  dmem_read32p( void *mp); 
uint16_t  dmem_read16p( void *mp); 
uint8_t   dmem_read8p(  void *mp);

void      dmem_write_buf32p(void *mp, const uint32_t *buf, unsigned cnt);
void      dmem_read_buf32p( void *mp,       uint32_t *buf, unsigned cnt);
void      dmem_write_buf16p(void *mp, const uint16_t *buf, unsigned cnt);
void      dmem_read_buf16p( void *mp,       uint16_t *buf, unsigned cnt);
void      dmem_write_buf8p( void *mp, const uint8_t  *buf, unsigned cnt);
void      dmem_read_buf8p(  void *mp,       uint8_t  *buf, unsigned cnt);
#endif //LIBDEVMEM_NO_EXTRAS

#ifdef __cplusplus
}
#endif 

#endif /* libdevmem_h_ */
