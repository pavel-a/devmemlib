"""
Module for accessing chip shared memory via the PCI BAR

Requires root.
All reads and writes should be in one hardware cycle, like in C.
This is ensured by using ctypes pointers, it should behave the closest to plain C.

27-jun-2017 pa04 renamed memr-> readxx
19-apr-2016 pa02
"""

from __future__ import print_function
import os, io
from mmap import *
from ctypes import *
import pcidev_sysfs as mypci

class Cmmdev:
    """ Represents device memory mapping """

    def __init__(self, sysfsPath, barNum, winsize):
        """ Create a mapping """
        self.mm_base = 0    # Phys. addr.
        self.mm_winsize = 0 # Mapped memory window size
        self.mm_devf = None # File handle
        self.mm = None      # mmap handle
        self.map_va = 0
        self.ptr4 = None
        self.ptr2 = None
        self.ptr1 = None

        bars = mypci.getPCIdeviceBars(sysfsPath)
        self.mm_base,barsize = bars[barNum]
        assert (barsize != 0) and (self.mm_base != 0)
        assert barsize >= winsize    # barsize is size from the resource list
        self.mm_devf, self.mm_winsize = mypci.openDeviceBar(sysfsPath, bar=barNum, access='w')
        assert self.mm_winsize >= winsize # mm_winsize is actually mapped size
        self.mm = mmap(self.mm_devf.fileno(), winsize, MAP_SHARED, PROT_READ | PROT_WRITE)
        self.ptr4 = POINTER(c_uint32)( c_uint32.from_buffer(self.mm) )
        self.map_va = cast(self.ptr4, c_void_p).value
        self.ptr2 = cast(self.ptr4, POINTER(c_uint16))
        self.ptr1 = cast(self.ptr4, POINTER(c_uint8))
        self.memr = self.read32
        self.memw = self.write32

    def mm_close(self) :
        if self.mm is not None :
            self.mm.close()
        if self.mm_devf is not None :
            self.mm_devf.close()
            self.mm_devf = None
        self.mm = None
        self.mm_base = 0

    def mm_unmap(self) :
        if self.map_va != 0 :
            self.mm_close()
            self.map_va = 0
        self.ptr4 = None
        self.ptr2 = None
        self.ptr1 = None

    def _mm_seek(self, off, size):
        """ Seek to phys address and return element index """
        if off % size :
            raise ValueError("Mem address not aligned on access size")
        if 0 <= off <= (self.mm_winsize - size):
            return (off // size)
        raise ValueError("Mem access [%#X] outside of window %#X" % (off, self.mm_winsize))

    """ access functions:
        Read:  u8|u16|u32 (offs)
        Write: u8|u16|u32 (offs, val)
    """
    def read32(self, addr):
        return self.ptr4[ self._mm_seek(addr,4) ]

    def write32(self, addr,val):
        self.ptr4[ self._mm_seek(addr,4) ] = (val & 0xFFFFFFFF)

    def write8(self, addr,val):
        self.ptr1[ self._mm_seek(addr,1) ] = (val & 0xFF)

    def write16(self, addr,val):
        self.ptr2[ self._mm_seek(addr,2) ] = (val & 0xFFFF)

    def read8(self, addr):
        return self.ptr1[ self._mm_seek(addr,1) ]

    def read16(self, addr):
        return self.ptr2[ self._mm_seek(addr,2) ]

    # Extras ...
    def __repr__(self):
        if self.mm:
            return("PCI BAR mapping @0x%X, size=0x%X" % (self.mm_base, self.mm_winsize))
        return("mmdev - not mapped")

    def getPhysAddr(self): return self.mm_base

    def getSize(self): return self.mm_winsize

    def printx(self, offs, cnt=1):
        """ Print cnt words from given offset, in hex """
        for i in range (cnt):
            print("%8.8X" % self.read32(i*4 + offs))

    def memfill32(self, offs, val, cnt=1):
        """ Fill memory with 32-bit value from offs, cnt words"""
        for i in range (cnt):
            self.write32(i*4 + offs, val)

############################# Test #####################################


def mm_test() :
  VENDORID = 0x1234
  DEVID    = 0x5678
  DEVID2   = 0x2400
  WIN_SIZE = 0x800000
  WIN_BAR = 0
  
  MM = None

  """ Find the PCI device """
  path = mypci.getPCIdeviceSysfsPath(vendor_id=VENDORID, dev_id=DEVID)
  if not path :
     print("Cannot find the PCI device")
     return

  MM = Cmmdev(path, WIN_BAR, WIN_SIZE)
  #path2 = mypci.getPCIdeviceSysfsPath(vendor_id=VENDORID, dev_id=DEVID2)
  #MM2 = Cmmdev(path2, 0, WIN_SIZE)

  print( MM )
  # Do some reads/writes...
  #print( hex( MM.read16(0x1000) ) )
  #MM.write16(0x1000, 1)

if __name__ == "__main__":
    mm_test()
