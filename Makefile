obj-m += ds3231.o
export KDIR := /lib/modules/$(shell uname -r)/build
allofit: modules
modules:
	@$(MAKE) -C $(KDIR) M=$(PWD) modules
modules_install:
	@$(MAKE) -C $(KDIR) M=$(PWD) modules_install
kernel_clean:
	@$(MAKE) -C $(KDIR) M=$(PWD) clean
clean: kernel_clean
	rm -rf Module.symvers modules.order
