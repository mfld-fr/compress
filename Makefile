
PROG=Release/compress

.PHONY: test

test_se:
	$(PROG) -c -m se data.bin test_out.bin
	$(PROG) -e -m se test_out.bin test_in.bin
	diff data.bin test_in.bin
	$(PROG) -c -m se code.bin test_out.bin
	$(PROG) -e -m se test_out.bin test_in.bin
	diff code.bin test_in.bin
	$(PROG) -c -m se ash.bin test_out.bin
	$(PROG) -e -m se test_out.bin test_in.bin
	diff ash.bin test_in.bin

test: test_se
