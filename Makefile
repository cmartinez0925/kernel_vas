obj-m += kernel_vas.o
#how_all_threads-objs :=

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
MODNAME := how_all_threads

# Enable the pr_debug() as well (rm the coment from one of the lines below)
# (Note: EXTRA_CFLAGS deprecated; use ccflags-y)
ccflags-y += -DDEBUG
#CFLAGS_<filename>.o := -DDEBUG 

all:
	make -C $(KDIR) M=$(PWD) modules
install:
	make -C $(KDIR) M=$(PWD) modules_install
clean:
	make -C $(KDIR) M=$(PWD) clean
load: all
	sudo insmod $(MODNAME).ko
upload:
	sudo rmmod $(MODNAME)
reload: unload load
dmesg:
	sudo dmesg | tail -20
