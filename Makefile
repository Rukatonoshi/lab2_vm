TARGET = lama-interpreter
FREQ_ANALYZER = frequency_analyzer
CC = gcc
COMMON_FLAGS = -m32 -g2 -fstack-protector-all -I./include -I./src/runtime

RUNTIME_DIR = src/runtime

all: $(TARGET)

# Frequency analyzer target
$(FREQ_ANALYZER): frequency_analyzer.o instructions.o frequency_analyzer_main.o
	$(CC) $(COMMON_FLAGS) $^ -o $@

# Main interpreter target
$(TARGET): gc_runtime.o runtime.o interpreter.o instructions.o main.o
	$(CC) $(COMMON_FLAGS) $^ -o $@

gc_runtime.o: $(RUNTIME_DIR)/gc_runtime.s
	$(CC) $(COMMON_FLAGS) -c $< -o $@

runtime.o: $(RUNTIME_DIR)/runtime.c $(RUNTIME_DIR)/runtime.h
	$(CC) $(COMMON_FLAGS) -c $< -o $@

instructions.o: src/instructions.c include/instructions.h include/opcodes.def
	$(CC) $(COMMON_FLAGS) -c $< -o $@

frequency_analyzer.o: src/frequency_analyzer.c src/frequency_analyzer.h src/uthash.h include/instructions.h
	$(CC) $(COMMON_FLAGS) -c $< -o $@

frequency_analyzer_main.o: src/frequency_analyzer_main.c src/byte_file.h src/frequency_analyzer.h
	$(CC) $(COMMON_FLAGS) -c $< -o $@

interpreter.o: src/interpreter.c src/interpreter.h include/instructions.h
	$(CC) $(COMMON_FLAGS) -c $< -o $@

main.o: src/main.c src/byte_file.h include/instructions.h
	$(CC) $(COMMON_FLAGS) -c $< -o $@

clean:
	rm -f *.a *.o *~ $(TARGET) $(FREQ_ANALYZER)
	rm -f regression/*.bc custom-tests/*.bc
