CFLAGS = -g3 -pedantic -ansi -Wall -Os -D_POSIX_C_SOURCE=2

rid3: rid3.o ConvertUTF.o

.PHONY: clean
clean:
	rm rid3 rid3.o ConvertUTF.o
