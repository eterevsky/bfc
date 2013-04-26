CFLAGS=-g -O2 -march=k8 -ffast-math -W -Wall -std=c99
LDFLAGS=-g

bfc: bfc.o tools.o statement.o parse.o gencode.o optimize.o

bfc.o: bfc.c statement.h parse.h gencode.h optimize.h Makefile

tools.o: tools.c tools.h Makefile

statement.o: statement.c statement.h Makefile

parse.o: parse.c statement.h tools.h Makefile

gencode.o: gencode.c gencode.h statement.h Makefile

optimize.o: optimize.c optimize.h statement.h Makefile


clean: 
	rm -f bfc *.o
