CC=g++
CFLAGS=-Wall -g -std=c++0x

.SUFFIXES: .o .c .h

SRC_DIRS = ./ 
DEPS = -I. 

CFLAGS += $(DEPS) #-D  -Werror
LDFLAGS = -Wall -L. -pthread -g -lrt -std=c++0x 
LDFLAGS += $(CFLAGS)

CPPS = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)*.c)) 
ASMS = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)*.S)) 

OBJS = $(addprefix obj/, $(notdir $(CPPS:.c=.o))) $(addprefix obj/, $(notdir $(ASMS:.S=.o))) 

all:llubenchmark

.PHONY: deps
deps:$(CPPS) $(ASMS) 
	$(CC) $(CFLAGS) -MM $^ > obj/deps
	sed '/^[^ ]/s/^/obj\//g' obj/deps > obj/deps.tmp
	mv obj/deps.tmp obj/deps
-include obj/deps

llubenchmark : $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
./obj/%.o: %.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<
./obj/%.o: %.S
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<


.PHONY: clean
clean:
	rm -f obj/*.o obj/.depend llubenchmark
