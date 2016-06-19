/**
* Library for physical memory access like in devmem
*
* pa05 16-may-2016
* 32-bit phys addr and usermode
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <assert.h>

#include "libdevmem.h" /* self */

#ifndef C_ASSERT
//#define C_ASSERT(cond) typedef char foo##__LINE__[1 - (!cond)] foo_t##__LINE__
#define C_ASSERT(cond) /**/
#endif // !C_ASSERT

#ifndef C_INLINE
#define C_INLINE __inline__
#endif // !C_INLINE


#define _MEMACCESS_LIB_VER_MJ 1
#define _MEMACCESS_LIB_VER_MN 0

// Names of environment variables to define the memory window:
// These can be redefined in makefile, ex "PCI_BASE_ADDRESS"
// Base physical address - mandatory
#ifndef ENV_MBASE
#define ENV_MBASE   "DEVMEMBASE"
#endif
// Last phys address - mandatory
#ifndef ENV_MEM_END
#define ENV_MEM_END "DEVMEMEND"
#endif
// Parameters/options - optional
#ifndef ENV_PARAMS
#define ENV_PARAMS  "DEVMEMOPT"
#endif


C_ASSERT( sizeof(dmem_phys_address_t) == sizeof(int32_t) );

#define printerr(fmt,...) while(dbgf){ fprintf(dbgf, fmt, ## __VA_ARGS__); fflush(dbgf); break; }


static int f_dbg = 0;
static FILE *dbgf = NULL;
static unsigned int pagesize;
static dmem_phys_address_t mbase = (dmem_phys_address_t)(-1L); // start of the real address window
static dmem_phys_address_t m_end = (dmem_phys_address_t)(-1L); // end of the real address window
static int g_env_read = 0;
static struct dmem_mapping_s *g_map = NULL; //TODO revise use of g_map & single mapping

static int get_env_params(void);

struct mapping_priv_s {
    int fd;             // 0 when not initialized, -1 = invalid
    dmem_phys_address_t mmap_base; // adjusted phys start addr
    dmem_phys_address_t mmap_end;  // adjusted  phys end addr
    void *mmap_va;      // mapped va
    void *mmap_va_end;  // mapped va last
    size_t mmap_size;   // adjusted mapping size
    size_t mmap_offset; // offset from mmap_va to map_ptr
    int offs_mode;
};

C_ASSERT(sizeof(struct mapping_priv_s) <= sizeof(struct dmem_mapping_s.reserved));


int dmem_mapping_map(struct dmem_mapping_s *param)
{
    if (!param)
        return -1;

    if (g_map) { //TODO revise use of g_map & single mapping
        printerr("Only one mapping supported and it is already in use.\n");
        return ENOTSUP;
    }

    g_map = param;
    struct mapping_priv_s *mp = (struct mapping_priv_s*)&param->reserved[0];

    if (!pagesize)
        pagesize = (unsigned)getpagesize(); /* or sysconf(_SC_PAGESIZE)  */

    dmem_mapping_size_t size = param->map_size;
    if (size < pagesize)
        size = pagesize;

    dmem_phys_address_t pha = param->map_addr;

    // MF_ABSOLUTE flag exists for utilities that use "physical" addresses. New programs should pass offsets in device mem. window instead.
    if (param->flags & MF_ABSOLUTE) {
        mp->offs_mode = 0;
    }  else {
        mp->offs_mode = 1;
        if (!g_env_read) {
            // If user forgot to call dmem_init...
            int ret = dmem_init();
            if (ret) return ret;
        }

        pha += mbase;
        if (pha < param->map_addr) {
            printerr("ERROR: rolling over end of memory\n");
            return ERANGE;
        }
    }

    if ( pha < mbase ) {
        printerr("ERROR: address < allowed window\n");
        return EINVAL;
    }

    // User can specify non-aligned address, we'll find a proper base address:
    mp->mmap_base = pha & ~((typeof(pha))pagesize-1);
    mp->mmap_offset = pha - mp->mmap_base;
    mp->mmap_size = ((mp->mmap_offset + size - 1) / pagesize) * pagesize + pagesize;
    mp->mmap_end = mp->mmap_base + mp->mmap_size - 1;

    if ( mp->mmap_base + mp->mmap_size <= mp->mmap_base) {
        printerr("ERROR: rolling over end of memory\n");
        return ERANGE;
    }

    if ( mp->mmap_end > m_end ) {
        printerr("ERROR: end address > allowed window\n");
        return ERANGE;
    }

#if 0 // for 32-bit version this already covered
    if (sizeof(mp->mmap_base) > sizeof(uint32_t)) {
      // Usermode type off_t can be 32 bit even if kernel phys. address is 64-bit. Consider mmap2() instead of this check
      if ( (sizeof(off_t) < sizeof(int64_t)) && (mp->phys_addr + mp->mmap_size) > UINT32_MAX ) {
        printerr("The address %#" PRIX64 " > 32 bits.\n", mp->phys_addr);
        return E2BIG;
      }
    }
#endif

    int mmap_flags;
    mmap_flags = O_RDWR | O_SYNC;
    if (param->flags & MF_READONLY) mmap_flags = O_RDONLY | O_SYNC;

    mp->fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mp->fd == -1) {
        printerr("Error opening /dev/mem (%d) : %s\n", errno, strerror(errno));
        return errno;
    }

    if (f_dbg) {
        printerr("/dev/mem opened.\n");
    }

    mmap_flags = PROT_READ | PROT_WRITE;
    if (param->flags & MF_READONLY) mmap_flags = PROT_READ;
    mp->mmap_va = mmap(0,
                    mp->mmap_size,
                    mmap_flags,
                    MAP_SHARED,
                    mp->fd,
                    mp->mmap_base);

    if ( (intptr_t)mp->mmap_va == (intptr_t)-1 ) {
        printerr("Error mapping (%d) : %s\n", errno, strerror(errno));
        close(mp->fd);
        return errno;
    }

    *((char**)&param->map_ptr) = mp->mmap_va + mp->mmap_offset;
    mp->mmap_va_end = mp->mmap_va + mp->mmap_size - 1;

    if (f_dbg) {
        printerr("Memory mapped at virt. addr [%p - %p[\n", mp->mmap_va, mp->mmap_va_end);
        printerr("User addr. range: [%p - %p[\n", param->map_ptr, (char*)param->map_ptr + param->map_size - 1);
    }

    return 0;
}


int dmem_mapping_unmap(struct dmem_mapping_s *param)
{
    if (!param) return -1;

    struct mapping_priv_s *mp = (struct mapping_priv_s *)&param->reserved[0];

    if (munmap(mp->mmap_va, mp->mmap_size) != 0) {
        printerr("ERROR munmap (%d) %s\n", errno, strerror(errno));
    }
    mp->mmap_base = (intptr_t)-1;
    mp->mmap_va = NULL;

    if (mp->fd > 0)
        close(mp->fd);
    mp->fd = -1;
    g_map = NULL; // revise!
    return 0;
}

//int dmem_init(void)
int dmem__init_(int addrsize, int mapsize, void *reserved)
{
    if (addrsize != sizeof(int32_t) || mapsize != sizeof(int32_t))
        return -2;
    pagesize = (unsigned)getpagesize(); /* or sysconf(_SC_PAGESIZE)  */
    int err = get_env_params();
    if (err)
        return err;
    return 0;
}


void *dmem_get_pointer( struct dmem_mapping_s *dp, dmem_mapping_size_t off,
                        uint32_t size)
{
    if (((off + size) > dp->map_size) || ((off + size) <= off))
        return NULL;
    if (!dp->map_ptr)
        return NULL;
    return dp->map_ptr + off;
}


int dmem_set_debug(int flags, FILE *dbgfile)
{
    f_dbg = !!(flags & 0x1);
    dbgf = dbgfile;
    return 0;
}

int dmem_finalize(void)
{
    if (g_map) {
        dmem_mapping_unmap(g_map);
        g_map = NULL;
    }
    return 0;
}

int dmem_get_version(void)
{
    return (_MEMACCESS_LIB_VER_MJ << 16) + _MEMACCESS_LIB_VER_MN;
}

int dmem_print_version(void)
{
    if ( !(f_dbg && dbgf) ) return -1;
    printerr("\n\nlibdevmem v.%u.%u\n",
            _MEMACCESS_LIB_VER_MJ, _MEMACCESS_LIB_VER_MN);
    return 0;
}

// Get environment parameters
// -> phys base address, size
static int get_env_params(void)
{
    char *p;
    uint64_t v64;
    char *endp = NULL;
    int errors = 0;
    int fPrint = f_dbg;

    if (!dbgf) {
        dbgf = stderr; // revise?
    }

    p = getenv(ENV_PARAMS);
    if (p) {
        if (strstr(p, "+d")) { // Debug
            f_dbg++;
            fPrint++;
        }

        if (strstr(p, "-NDM")) { // kill switch
            fprintf(stderr,
                "ERROR: Env. parameter in %s forbids use of this memory access module\n", ENV_PARAMS);
            return -1;
        }
    }
    //else if (fPrint)
    //    printf("%s not set\n", ENV_PARAMS);

    if (fPrint)
        printf("\n\nlibdevmem v.%u.%u (32-bit) Environment parameters:\n",
            _MEMACCESS_LIB_VER_MJ, _MEMACCESS_LIB_VER_MN);

    if (fPrint && p)
        printf("%s = \"%s\"\n", ENV_PARAMS, p);

    p = getenv(ENV_MBASE);
    if (p) {
        errno = 0;
        v64 = strtoull(p, &endp, 16);
        if (errno != 0 || (endp && 0 != *endp)) {
            printerr("Error in %s value: %s\n", ENV_MBASE, p);
            errors++;
        }
        else {
            mbase = (dmem_phys_address_t)v64;
            if (fPrint)
                printf("%s = %#" PRIX64 "\n", ENV_MBASE, v64 );
            if (v64 != mbase) {
                printerr("Error: memory base address value is too large!\n");
                errors++;
            }
        }
    }
    else if (fPrint)
        printf("%s not set\n", ENV_MBASE);

    p = getenv(ENV_MEM_END);
    if (p) {
        errno = 0;
        v64 = strtoull(p, &endp, 16);
        if (errno != 0 || (endp && 0 != *endp)) {
            printerr("Error in %s value: %s\n", ENV_MEM_END, p);
            errors++;
        }
        else {
            m_end = (dmem_phys_address_t)v64;
            if (fPrint)
                printf("%s = %#" PRIX64 "\n", ENV_MEM_END, v64 );
            if (v64 != m_end) {
                printerr("Error: memory end address value is too large!\n");
                errors++;
            }
        }
    }
    else if (fPrint)
        printf("%s not set\n", ENV_MEM_END);

    if (m_end <= mbase) {
        printerr("Error: end memory %s <= base %s\n", ENV_MEM_END, ENV_MBASE);
        errors++;
    }

    if (errors) {
        if ( fPrint ) {
            printerr("Errors found in environment parameters\n");
        }
        return -1;
    }

    g_env_read = 1;
    return 0;
}

#ifndef LIBDEVMEM_NO_EXTRAS
//============================================================================
// Helper functions for memory access
//
// Add memory barriers, flushes etc. if needed for specific arch.
//============================================================================

// Error hook:
static void dmem_error(void)
{
    printf("\nlibdevmem: Invalid address or size in dmem... call\n");
    //set debug break here
    abort(); // or send sigsegv to myself? or user callback?
}


// Fast I/O ops via pointer
// Pointers can be obtained from dmem_get_pointer()
void dmem_write32p(void *mp, uint32_t v)
{
    *(volatile uint32_t*)mp = v;
}

void dmem_write16p(void *mp, uint16_t v)
{
    *(volatile uint16_t*)mp = v;
}

void dmem_write8p(void *mp, uint8_t v)
{
    *(volatile uint8_t*)mp = v;
}

uint32_t dmem_read32p(void *mp)
{
    return *(volatile uint32_t*)mp;
}

uint16_t dmem_read16p(void *mp)
{
    return *(volatile uint16_t*)mp;
}

uint8_t dmem_read8p(void *mp)
{
    return *(volatile uint8_t*)mp;
}

void dmem_write_buf32p(void *mp, const uint32_t *buf, unsigned cnt)
{
    uint32_t *p = (uint32_t*)mp;
    for ( ; cnt != 0; cnt--) {
        dmem_write32p( p++, *buf++);
    }
}

void dmem_read_buf32p(void *mp, uint32_t *buf, unsigned cnt)
{
    uint32_t *p = (uint32_t*)mp;
    for ( ;  cnt; cnt-- ) {
        *buf++ = dmem_read32p(p++);
    }
}

void dmem_write_buf8p(void *mp, const uint8_t *buf, unsigned cnt)
{
    uint8_t *p = (uint8_t*)mp;
    for ( ; cnt != 0; cnt--) {
        dmem_write8p( p++, *buf++);
    }
}

void dmem_read_buf8p(void *mp, uint8_t *buf, unsigned cnt)
{
    uint8_t *p = (uint8_t*)mp;
    for ( ;  cnt; cnt-- ) {
        *buf++ = dmem_read8p(p++);
    }
}

void dmem_fill_buf32p(void *mp, unsigned cnt, uint32_t v)
{
    uint32_t *p = (uint32_t*)mp;
    for ( ;  cnt; cnt-- ) {
        dmem_write32p( p++, v);
    }
}

void dmem_fill_buf16p(void *mp, unsigned cnt, uint16_t v)
{
    uint16_t *p = (uint16_t*)mp;
    for ( ;  cnt; cnt-- ) {
        dmem_write16p( p++, v);
    }
}

void dmem_fill_buf8p(void *mp, unsigned cnt, uint8_t v)
{
    uint8_t *p = (uint8_t*)mp;
    for ( ;  cnt; cnt-- ) {
        dmem_write8p( p++, v);
    }
}


// I/O ops with validation, using the param struct:
// TODO fix validation, check for add wraparounds!
// TODO check alignment?

void dmem_write32(struct dmem_mapping_s *dp, dmem_mapping_size_t off, uint32_t v)
{
    if ((off + sizeof(uint32_t)) > dp->map_size)
        dmem_error();
    dmem_write32p((void*)(dp->map_ptr + off), v);
}

uint32_t dmem_read32(struct dmem_mapping_s *dp, dmem_mapping_size_t off)
{
    if ((off + sizeof(uint32_t)) > dp->map_size)
        dmem_error();
    return dmem_read32p((void*)(dp->map_ptr + off));
}

void dmem_write16(struct dmem_mapping_s *dp, dmem_mapping_size_t off, uint16_t v)
{
    if ((off + sizeof(uint16_t)) > dp->map_size)
        dmem_error();
    dmem_write16p((void*)(dp->map_ptr + off), v);
}

uint16_t dmem_read16(struct dmem_mapping_s *dp, dmem_mapping_size_t off)
{
    if ((off + sizeof(uint16_t)) > dp->map_size)
        dmem_error();
    return dmem_read16p((void*)(dp->map_ptr + off));
}

void dmem_write8(struct dmem_mapping_s *dp, dmem_mapping_size_t off, uint8_t v)
{
    if ((off + sizeof(uint8_t)) > dp->map_size)
        dmem_error();
    dmem_write8p((void*)(dp->map_ptr + off), v);
}

uint8_t dmem_read8(struct dmem_mapping_s *dp, dmem_mapping_size_t off)
{
    if ((off + sizeof(uint8_t)) > dp->map_size)
        dmem_error();
    return dmem_read8p((void*)(dp->map_ptr + off));
}

void dmem_write_buf32(struct dmem_mapping_s *dp, const uint32_t *buf, dmem_mapping_size_t off, unsigned cnt)
{
    if ( off > (dp->map_size - cnt * sizeof(uint32_t)) || (off + cnt*sizeof(uint32_t)) > dp->map_size )
        dmem_error();

    dmem_write_buf32p((void*)(dp->map_ptr + off), buf, cnt);
}

void dmem_read_buf32(struct dmem_mapping_s *dp, uint32_t *buf, dmem_mapping_size_t off, unsigned cnt)
{
    if (off >(dp->map_size - cnt * sizeof(uint32_t)) || (off + cnt*sizeof(uint32_t)) > dp->map_size)
        dmem_error();

    dmem_read_buf32p((void*)(dp->map_ptr + off), buf, cnt);
}

void dmem_write_buf8(struct dmem_mapping_s *dp, const uint8_t *buf, dmem_mapping_size_t off, unsigned cnt)
{
    if ( off > (dp->map_size - cnt * sizeof(uint8_t)) || (off + cnt*sizeof(uint8_t)) > dp->map_size )
        dmem_error();

    dmem_write_buf8p((void*)(dp->map_ptr + off), buf, cnt);
}

void dmem_read_buf8(struct dmem_mapping_s *dp, uint8_t *buf, dmem_mapping_size_t off, unsigned cnt)
{
    if (off >(dp->map_size - cnt * sizeof(uint8_t)) || (off + cnt*sizeof(uint32_t)) > dp->map_size)
        dmem_error();

    dmem_read_buf8p((void*)(dp->map_ptr + off), buf, cnt);
}

void dmem_fill_buf32(dmem_mapping_hnd_t dp, dmem_mapping_size_t off, unsigned cnt, uint32_t v)
{
    void *p = dmem_get_pointer(dp, off, cnt*sizeof(uint32_t));
    if (!p)
       dmem_error();
    dmem_fill_buf32p(p, cnt, v);
}

void dmem_fill_buf16(dmem_mapping_hnd_t dp, dmem_mapping_size_t off, unsigned cnt, uint16_t v)
{
    void *p = dmem_get_pointer(dp, off, cnt*sizeof(uint16_t));
    if (!p)
       dmem_error();
    dmem_fill_buf16p(p, cnt, v);
}

void dmem_fill_buf8(dmem_mapping_hnd_t dp, dmem_mapping_size_t off, unsigned cnt, uint8_t v)
{
    void *p = dmem_get_pointer(dp, off, cnt*sizeof(uint8_t));
    if (!p)
       dmem_error();
    dmem_fill_buf8p(p, cnt, v);
}

#endif //LIBDEVMEM_NO_EXTRAS

