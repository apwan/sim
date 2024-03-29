# Modify this line to indicate the default version to build

VERSION=full#std

# Comment this out if you don't have Tcl/Tk on your system

GUIMODE=-DHAS_GUI

# Modify the following line so that gcc can find the libtcl.so and
# libtk.so libraries on your system. You may need to use the -L option
# to tell gcc which directory to look in. Comment this out if you
# don't have Tcl/Tk.

TKLIBS=-L/usr/lib -ltk -ltcl

# Modify the following line so that gcc can find the tcl.h and tk.h
# header files on your system. Comment this out if you don't have
# Tcl/Tk.

TKINC=-isystem /usr/include

# Modify these two lines to choose your compiler and compile time
# flags.

CC=g++
CFLAGS=-Wall -O2 -DBIG_MEM

##################################################
# You shouldn't need to modify anything below here
##################################################

MISCDIR=../misc
HCL2C=$(MISCDIR)/hcl2c
INC=$(TKINC) -I$(MISCDIR) $(GUIMODE)
LIBS=$(TKLIBS) -lm
YAS = ../misc/yas
SOURCES = psim.c psim_main.c utils.c simulator.c psim_gui.c # need to compile using -DHAS_GUI
HEADERS = utils.h sim.h simulator.h #

all: psim drivers

# This rule builds the PIPE simulator
psim: psim.c $(HEADERS) pipe-$(VERSION).hcl $(MISCDIR)/isa.c $(MISCDIR)/isa.h
	# Building the pipe-$(VERSION).hcl version of PIPE
	$(HCL2C) -n pipe-$(VERSION).hcl < pipe-$(VERSION).hcl > pipe-$(VERSION).c
	$(CC) $(CFLAGS) $(INC) -o psim $(SOURCES) pipe-$(VERSION).c \
		$(MISCDIR)/isa.c $(MISCDIR)/mem.c $(LIBS)

# This rule builds driver programs for Part C of the Architecture Lab
drivers:
	./gen-driver.pl -n 4 -f ncopy.ys > sdriver.ys
	../misc/yas sdriver.ys
	./gen-driver.pl -n 63 -f ncopy.ys > ldriver.ys
	../misc/yas ldriver.ys

# These are implicit rules for assembling .yo files from .ys files.
.SUFFIXES: .ys .yo
.ys.yo:
	$(YAS) $*.ys

run:
	./psim -tcu -o ./core1.txt -d ./core2.txt -i ../y86-code/asum.yo
clean:
	rm -f psim pipe-*.c *.o *.exe *~
