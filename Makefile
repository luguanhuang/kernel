KERNELDIR=/lib/modules/`uname -r`/build
#KERNELDIR=/lib/modules/5.5.0+/build
#ARCH=i386
#KERNELDIR=/usr/src/kernels/`uname -r`-i686

EXTRA_CFLAGS += -I$(PWD)
MODULES = Kernal.ko 
obj-m += Kernal.o 


all: $(MODULES)

Kernal.ko: Kernal.c
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -f *.o *.o.rc


