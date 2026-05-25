# Makefile for the compress program

PROG = Release/compress

.PHONY: test

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

test: test_b test_rb test_pb test_rpb test_se test_si test_rse
