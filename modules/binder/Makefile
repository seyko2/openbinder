ifneq ($(KERNELRELEASE),)

	obj-m += binderdev.o

	binderdev-objs := iobuffer.o binder.o binder_node.o binder_proc.o binder_thread.o binder_transaction.o

	ifeq ($(strip $(CUSTOM_CFLAGS)),)
	CUSTOM_CFLAGS:= -I $(PWD)/../../headers
	endif
	CFLAGS += $(CUSTOM_CFLAGS)

else

	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

modules default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

mi modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

unloadmodules:
	@echo "Unloading Binder module..."
	@echo "You may need to enter your password"
	@sudo rmmod binderdev ; true
	@sleep 1
	@if [ "$$(lsmod | grep binderdev)" = "" ]; then \
		echo "Binder module successfully unloaded."; \
	fi

loadmodules: modules
	@echo "Loading Binder module..."
	@echo "You may need to enter your password"
	@sudo rmmod binderdev ; true
	sudo insmod $(PWD)/binderdev.ko
	@sleep 1
	@if [ "$$(lsmod | grep binderdev)" = "" ]; then \
		echo "Error loading binder module."; \
		echo "Multiprocess will not be available."; \
	else \
		sudo chmod a+rw /dev/binder; \
		echo "Binder module successfully installed."; \
	fi

	
$(KERNELDIR)/extra:
	mkdir $(KERNELDIR)/extra

buggy_ubuntu: modules_install $(KERNELDIR)/extra
	mkdir -p /lib/modules/$(shell uname -r)/extra
	cp /lib/modules/$(shell uname -r | sed "s/-.*//")/extra/* /lib/modules/$(shell uname -r)/extra
	if [ -d /etc/udev/rules.d ] ; then 	\
		echo "Installing binderdev.rules for udev" ;	\
		cp -f binderdev.rules /etc/udev/rules.d/ ;	\
	fi

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
