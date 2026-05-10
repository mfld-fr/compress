# Makefile for the compress program

PROG = Release/compress

.PHONY: test

# Test the SE algorithm

test_se:
	$(PROG) -c -m se data.bin test_out.bin
	$(PROG) -e -m se test_out.bin test_in.bin
	diff data.bin test_in.bin
	du -b data.bin test_out.bin
	$(PROG) -c -m se code.bin test_out.bin
	$(PROG) -e -m se test_out.bin test_in.bin
	diff code.bin test_in.bin
	du -b code.bin test_out.bin
	$(PROG) -c -m se ash.bin test_out.bin
	$(PROG) -e -m se test_out.bin test_in.bin
	diff ash.bin test_in.bin
	du -b ash.bin test_out.bin

# Test the SI algorithm

test_si:
	$(PROG) -c -m si data.bin test_out.bin
	$(PROG) -e -m si test_out.bin test_in.bin
	diff data.bin test_in.bin
	du -b data.bin test_out.bin
	$(PROG) -c -m si code.bin test_out.bin
	$(PROG) -e -m si test_out.bin test_in.bin
	diff code.bin test_in.bin
	du -b code.bin test_out.bin
	$(PROG) -c -m si ash.bin test_out.bin
	$(PROG) -e -m si test_out.bin test_in.bin
	diff ash.bin test_in.bin
	du -b ash.bin test_out.bin

test: test_se test_si
