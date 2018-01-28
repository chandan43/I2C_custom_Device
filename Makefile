#obj-m := i2c_arduino_custom_slave.o
obj-m += arduinoi2c.o
#arduinoi2c-m += i2c_arduino_custom_slave.o
#arduinoi2c-m += i2c_arduino_custom_slave.o

KDIR =  /home/elinux/linux-4.4.96

PWD := $(shell pwd)

default:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

#make ARCH=arm CROSS_COMPILE=arm-linux-
