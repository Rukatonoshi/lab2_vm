TARGET = lama-interpreter
CC=gcc
COMMON_FLAGS=-m32 -g2 -fstack-protector-all

all: gc_runtime.o runtime.o vm.o
	$(CC) $(COMMON_FLAGS) gc_runtime.o runtime.o main.o -o $(TARGET)

gc_runtime.o: runtime/gc_runtime.s
	$(CC) $(COMMON_FLAGS) -c runtime/gc_runtime.s

runtime.o: runtime/runtime.c runtime/runtime.h
	$(CC) $(COMMON_FLAGS) -c runtime/runtime.c

vm.o: main.c byte_file.h bytecode_decoder.h interpreter.h
	$(CC) $(COMMON_FLAGS) -Wall -c main.c

clean:
	$(RM) *.a *.o *~ $(TARGET)

