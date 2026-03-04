obj-m += nulldump.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	sudo insmod nulldump.ko

uninstall:
	sudo rmmod nulldump

test:
	@echo "Testing nulldump..."
	@echo "Writing test data..."
	@echo "Hello, kernel!" | sudo tee /dev/nulldump
	@echo "Reading from nulldump..."
	@sudo cat /dev/nulldump
	@echo "Check dmesg for logs"
