all: penn-sh

default: penn-sh

penn-sh.o: penn-sh.c penn-sh.h
	clang -c -Wall penn-sh.c tokenizer.c jobs.c

penn-sh: penn-sh.o tokenizer.o
	clang -o penn-sh penn-sh.o tokenizer.o jobs.o

tokenizer.o: tokenizer.c tokenizer.h
	clang -c -Wall tokenizer.c

jobs.o: jobs.c jobs.h
	clang -c -Wall jobs.c

clean:
	rm -rf penn-sh  *.o