
# Libdevmem test makefile
PROG = dmtest1

DEVMEM_DIR = $(CURDIR)

CC ?= "gcc"
#CFLAGS ?= -O0 -g
CFLAGS ?= -O2
CFLAGS += -Wall
#CFLAGS += -static
#LDLAGS += -Wl,-static

CFLAGS += $(shell $(DEVMEM_DIR)/libdevmem-config --cflags)
LIBS   += $(shell $(DEVMEM_DIR)/libdevmem-config --libs)


SRC = ex1-libdevmem.c

$(PROG): $(SRC) $(MAKEFILE_LIST) 
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $(SRC)


clean:
	rm -f $(PROG) *.o *~

