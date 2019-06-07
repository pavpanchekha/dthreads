
all: libdthread.so

dthread.o: dthread.c
	gcc -c $^ -fPIC -o $@

libdthread.so: dthread.o
	gcc --shared $^ -ldl -o $@

test: test.c
	gcc -o $@ $< -lpthread

run: libdthread.so test
	yes | ./test
	yes | LD_PRELOAD=$(shell pwd)/libdthread.so ./test
