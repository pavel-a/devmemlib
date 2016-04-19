"""
Module for accessing the chip shared memory via the PCI BAR

Existing code assumes the memory always begins at PHY_DEF_BASE
so we simulate this even when it is not.
If you want a different base address, call mm_setBaseAddr() to change.
The real phys. address is curr_base, you cannot change it.

Note: All reads and writes must be done in one hit, like in C.
This is ensured by using ctypes pointers.
Assume we are root.

19-apr-2016 pa02
"""

from __future__ import print_function
import os, io
from mmap import *
from ctypes import *
import pcidev_sysfs as mypci


# Dini 6 FPGA...
print("USING FPGA base addr 0xD0000000 ...")
VENDORID=0x17DF
DEVID=0x1907
BAR_NUM=4
PHY_DEF_BASE=0xD0000000

mm_baseX = PHY_DEF_BASE # Phys address simulated for user. User can change it.
mm_base=0  # Real phys. addr. User cannot change it.
mm_winsize = 0 # Mapped memory window size. User cannot change it.
mm_devf = None
mm = None
curr_va = 0
ptr4 = None
ptr2 = None
ptr1 = None

def mm_init() :
  """ Find and map the PCI device BAR. """
  global mm, mm_base,curr_va,mm_winsize,mm_devf
  global ptr4,ptr2,ptr1
  if mm: print("pymem - already initialized"); return

  path = mypci.getPCIdeviceSysfsPath(vendor_id=VENDORID, dev_id=DEVID)
  if not path or len(path) == 0 :
     raise IOError("Cannot find the PCI device")
  # Get the phys. address
  bars = mypci.getPCIdeviceBars(path)
  mm_base,bar0size = bars[BAR_NUM]
  assert (bar0size != 0) and (mm_base != 0)

  mm_devf, mm_winsize = mypci.openDeviceBar(path, bar=BAR_NUM, access='w')
  # will raise exception if cannot open
  assert bar0size == mm_winsize

  # Map the whole BAR, it is not too large:
  mm = mmap(mm_devf.fileno(), mm_winsize, MAP_SHARED, PROT_READ | PROT_WRITE)
  # will raise exception if cannot mmap

  ptr4 = POINTER(c_uint32)( c_uint32.from_buffer(mm) )
  curr_va = cast(ptr4, c_void_p).value
  ptr2 = cast(ptr4, POINTER(c_uint16))
  ptr1 = cast(ptr4, POINTER(c_uint8))

  print("pymem: Phys mem base=%#X size=%#X" % (mm_base, mm_winsize))
  if mm_base != mm_baseX:
     print("WARNING! on this platform the real phys addr is not %X!" % mm_baseX)


def mm_close() :
   global mm_devf
   mm.close()
   if mm_devf is not None :
      mm_devf.close()
      mm_devf = None

def mm_unmap() :
  global mm_base,curr_va
  if curr_va != 0 :
    if mm : mm.close()
  curr_va = 0
  mm_base = 0

# Seek to phys address and return element index
def _mm_seek(addr, size):
   assert curr_va != 0
   if addr % size :
       raise ValueError("Mem address not aligned on access size")
   off = addr - mm_baseX
   if 0 <= off <= (mm_winsize - size):
       return (off // size)
   raise ValueError("Mem access outside of window!" + hex(addr) + ' ' + hex(mm_baseX))
   #? if needed, move mmap window?



""" Exported functions:
  Read:  u8|u16|u32 (offs)
  Write: u8|u16|u32 (offs, val)
"""
def memr1(addr):
  return ptr1[ _mm_seek(addr,1) ]

def memr2(addr):
  return ptr2[ _mm_seek(addr,2) ]

def memr4(addr):
  return ptr4[ _mm_seek(addr,4) ]

def memw1(addr,val):
  ptr1[ _mm_seek(addr,1) ] = (val & 0xFF)

def memw2(addr,val):
  ptr2[ _mm_seek(addr,2) ] = (val & 0xFFFF)

def memw4(addr,val):
  ptr4[ _mm_seek(addr,4) ] = (val & 0xFFFFFFFF)


# Some extras ...

def mm_getBaseAddr(): return mm_baseX

def mm_setBaseAddr(a):
    """ Set the simulated base address """
    global mm_baseX
    assert 0 <= a
    mm_baseX = a

def mm_getPhysAddr(): return mm_base

def mm_getMappedSize(): return mm_winsize

# and more extras ...

def printx(addr, cnt=1):
  """ nice hex print """
  for i in range (0,cnt):
    print("%8.8X" % memr4(i*4 + addr))

def memfill4(addr,val,cnt=1):
  """ Fill memory with u32 value """
  for i in range (0,cnt):
    memw4(i*4 + addr, val)


# MAIN
if 1 : # __name__ == "__main__":
    mm_init()

