GCC=gcc
CFLAGS=-g -Wall -Wextra
OBJ=lib.o derby.o list.o
SRC=src
BIN=bin
LIB=-lpthread
ELF=derby

${ELF}: ${OBJ}
	${GCC} -o ${ELF} ${CFLAGS} ${OBJ} ${LIB}
	@mv *.o ${BIN}
	@mv ${ELF} ${BIN}

derby.o: ${SRC}/derby.c
	${GCC} -c ${CFLAGS} ${SRC}/derby.c

lib.o: ${SRC}/lib.c
	${GCC} -c ${CFLAGS} ${SRC}/lib.c

list.o: ${SRC}/list.c
	${GCC} -c ${CFLAGS} ${SRC}/list.c

.PHONY: clean
clean:
	@rm -f ${BIN}/*.o

.PHONY: realclean
realclean: clean
	@rm -f ${BIN}/${ELF}
