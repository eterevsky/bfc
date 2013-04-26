CFLAGS=-g -O2 -march=k8 -ffast-math -W -Wall -std=c99
LDFLAGS=-g

bfc: *.c

clean: 
	rm -f bfc *.o
