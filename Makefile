# Makefile for the compress program

PROG = Release/compress

CC = gcc
CFLAGS = -O3 -Wall

SRCS = src/compress.c src/list.c src/stream.c src/symbol.c
OBJS = Release/src/compress.o Release/src/list.o Release/src/stream.o Release/src/symbol.o

.PHONY: all build test clean

# Build rules
all: build

build: $(PROG)

$(PROG): $(OBJS)
	@echo "Linking $@"
	mkdir -p Release
	$(CC) $(CFLAGS) -o $@ $^ -lm

Release/src/%.o: src/%.c
	@echo "Compiling $<"
	mkdir -p Release/src
	$(CC) $(CFLAGS) -c -o $@ $<

# Macro to test a file
# $(1): algorithm name
# $(2): input file
define TEST_FILE
	echo "Testing $(1) algo on $(2)"
	$(PROG) -ct -m $(1) $(2) test_out.bin
	$(PROG) -et -m $(1) test_out.bin test_in.bin
	diff $(2) test_in.bin
	du -b $(2) test_out.bin
endef

# Macro to test a compression algorithm
# $(1) = algorithm name (se or si)
define TEST_ALGO
	echo "Testing $(1) algorithm"
	$(call TEST_FILE,$(1),data.bin)
	$(call TEST_FILE,$(1),code.bin)
	$(call TEST_FILE,$(1),ash.bin)
	echo
endef

# Test the B algorithm
test_b:
	$(call TEST_ALGO,b)

# Test the RB algorithm
test_rb:
	$(call TEST_ALGO,rb)

# Test the PB algorithm
test_pb:
	$(call TEST_ALGO,pb)

# Test the RPB algorithm
test_rpb:
	$(call TEST_ALGO,rpb)

# Test the SE algorithm
test_se:
	$(call TEST_ALGO,se)

# Test the SI algorithm
test_si:
	$(call TEST_ALGO,si)

# Test the RSE algorithm
test_rse:
	$(call TEST_ALGO,rse)

test: build test_b test_rb test_pb test_rpb test_se test_si test_rse

clean:
	rm -rf Release/src/*.o Release/src/*.d $(PROG)
	rm -f test_out.bin test_in.bin
