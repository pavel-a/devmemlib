# Find a PCI device in Linux sysfs.
# Get BAR and the config space addresses & size
# If there are several instances of PCI device, use 'skip' parameter.
# Ref: PCI sysfs docum:
#    http://lxr.free-electrons.com/source/Documentation/filesystems/sysfs-pci.txt
# 23-nov-2015 possible empty bar (addr, len=0)

from __future__ import print_function
import os, io, string

def getPCIdeviceSysfsPath(vendor_id=0xAAAA, dev_id=0x0001, skip=0) :
  """ Lookup a PCI device by VEN & DEV id.
      If you have more than one instance, specify the skip parameter.
      Returns sysfs path of the device """
  assert os.uname()[0] == 'Linux'
  for basedir,dirs,_ in os.walk("/sys/bus/pci/devices/"):
    break # should be one level deep only!

  for dev in dirs:
    path = basedir + dev
    with open(path + "/device") as f:
       if int(f.read(),0) != dev_id : continue
    with open(path + "/vendor") as f:
       if int(f.read(),0) != vendor_id : continue
    if skip > 0 : skip = skip-1; continue
    return path
  else:
    #print("Not found ven=%#X dev=%#X" % vendor_id, dev_id)
    return None


def getPCIdeviceBars(path) :
   """ From sysfs path, get BARs phys addresses, sizes.
       Returns list of tuples (BAR start, BAR size)
   """
   barlist = []
   with open(path + "/resource", 'r') as f:
     for s in f:
       a = string.split(s)
       bar_start, bar_end = long(a[0],0), long(a[1],0)
       # no idea what is in a[2]
       #can be empty bar! if bar_start == 0 : break
       if bar_start == 0 and bar_end == 0:
          barlist.append( (0, 0) )
          continue
       if bar_end <= bar_start :
         raise Exception("BAR_end < BAR_start ??")
       barlist.append( (bar_start, bar_end-bar_start+1) )
   return barlist


def getPCIdeviceBarPath(path, barNum) :
    """ From sysfs path, get BAR or configspace file path. """
    if barNum == 'config':
        return path + '/config'
    assert 0 <= barNum < 6
    return path + '/resource' + str(barNum)


# Open a PCI BAR resource or config space as file, return a python file object
# Caller can then mmap it.
def openDeviceBar(path, bar=0, access='r') :
  s = getPCIdeviceBarPath(path, bar)
  acc='rb'
  if 'w' in access:
    acc = 'r+b'
  f = open(s, acc)
  f.seek(0,io.SEEK_END)
  msize = f.tell()
  assert msize >= 0
  return f,msize

##############################################################
def _test():
  ven, dev = 0x1D69, 0x2400
  print("Checking on PCI: ven %#x dev %#x" % (ven,dev))
  s = getPCIdeviceSysfsPath(vendor_id=ven, dev_id=dev)
  if s:
    print("Found: %s" % s)
    if getPCIdeviceSysfsPath(ven,dev,skip=1):
      print("WARNING. You've got two or more instances of this thing")
  else:
    print("Device not found")
    return -1
  #
  bars = getPCIdeviceBars(s)
  print("It has %d BAR%s" % (len(bars), '' if len(bars) == 1 else 's')   )
  for p in bars:
    print("Phys addr %#X size %#X" % (p[0],p[1]))
  #
  barfile, bar_size = openDeviceBar(s,0,'rw')
  print("BAR0 size=%X" % bar_size)
  pagesize = os.sysconf("SC_PAGE_SIZE")
  if (bar_size <= 0) or (bar_size % pagesize ):
    print("ERROR! BAR size is bad!")
    return -1
  barfile.close()
  #
  cfg,cfg_size = openDeviceBar(s, 'config')
  print("Config space size=%#X" % cfg_size)
  cfg.seek(0, io.SEEK_SET)
  id_bytes=cfg.read(4)
  assert len(id_bytes) == 4
  #print( ("id="+" {:x}"*len(id_bytes)).format( *[ord(b) for b in id_bytes] ) )
  print( "id=", " ".join( [hex(ord(b)) for b in id_bytes] ) )
  cfg.close()
  assert id_bytes == bytearray([ven&0xFF, ven >> 8, dev & 0xFF, dev >> 8])
  #
  print ("PASS")
  return 0

if __name__ == "__main__":
  _test()

