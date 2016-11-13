
obj-m += test-driver.o
#obj-m += test_morteza.o

all: 
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /home/khsieh/linux-aarch64-gem5-20140821 M=$(PWD) modules

clean: 
	make -C /home/khsieh/linux-aarch64-gem5-20140821 M=$(PWD) clean
