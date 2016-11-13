
obj-m += test-driver.o
#obj-m += test_morteza.o

all: 
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C ../linux-aarch64-gem5-20140821 M=$(PWD) modules

clean: 
	make -C ../linux-aarch64-gem5-20140821 M=$(PWD) clean
