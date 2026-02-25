TARGET = lama-interpreter
CC = gcc
COMMON_FLAGS = -m32 -g2 -fstack-protector-all

RUNTIME_DIR = src/runtime

all: $(TARGET)

$(TARGET): gc_runtime.o runtime.o interpreter.o main.o
	$(CC) $(COMMON_FLAGS) $^ -o $@

gc_runtime.o: $(RUNTIME_DIR)/gc_runtime.s
	$(CC) $(COMMON_FLAGS) -c $< -o $@

runtime.o: $(RUNTIME_DIR)/runtime.c $(RUNTIME_DIR)/runtime.h
	$(CC) $(COMMON_FLAGS) -c $< -o $@

interpreter.o: src/interpreter.c src/interpreter.h
	$(CC) $(COMMON_FLAGS) -c $< -o $@

main.o: src/main.c src/byte_file.h src/bytecode_decoder.h
	$(CC) $(COMMON_FLAGS) -c $< -o $@

clean:
	rm -f *.a *.o *~ $(TARGET)
	rm -f regression/*.bc custom-tests/*.bc
