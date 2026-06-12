# Fuzzing Makefile for CCSDS 124.0-B-1 C Implementation
#
# Supports both libFuzzer (Clang) and AFL++
#
# Usage:
#   make                     # Build all fuzzers with libFuzzer
#   make AFL=1               # Build all fuzzers with AFL++
#   make run-decompress      # Run decompressor fuzzer
#   make run-compress        # Run compressor fuzzer
#   make run-roundtrip       # Run roundtrip fuzzer
#   make corpus              # Create initial corpus from test vectors

# Compiler settings
CC_LIBFUZZER = clang
CC_AFL = afl-clang-fast

# Source files
SRC_DIR = ../src
INCLUDE_DIR = ../include
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Exclude CLI from fuzz builds
SRCS := $(filter-out $(SRC_DIR)/cli.c, $(SRCS))

# Fuzzer harnesses
FUZZ_DECOMPRESS = fuzz_decompress
FUZZ_COMPRESS = fuzz_compress
FUZZ_ROUNDTRIP = fuzz_roundtrip

# Corpus directories
CORPUS_DECOMPRESS = corpus_decompress
CORPUS_COMPRESS = corpus_compress
CORPUS_ROUNDTRIP = corpus_roundtrip

# Build flags
CFLAGS_BASE = -I$(INCLUDE_DIR) -g -O1 -fno-omit-frame-pointer
CFLAGS_LIBFUZZER = $(CFLAGS_BASE) -fsanitize=fuzzer,address,undefined
CFLAGS_AFL = $(CFLAGS_BASE) -DAFL_HARNESS

ifdef AFL
CC = $(CC_AFL)
CFLAGS = $(CFLAGS_AFL)
SUFFIX = _afl
else
CC = $(CC_LIBFUZZER)
CFLAGS = $(CFLAGS_LIBFUZZER)
SUFFIX =
endif

.PHONY: all clean corpus run-decompress run-compress run-roundtrip help

all: $(FUZZ_DECOMPRESS)$(SUFFIX) $(FUZZ_COMPRESS)$(SUFFIX) $(FUZZ_ROUNDTRIP)$(SUFFIX)

$(FUZZ_DECOMPRESS)$(SUFFIX): fuzz_decompress.c $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

$(FUZZ_COMPRESS)$(SUFFIX): fuzz_compress.c $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

$(FUZZ_ROUNDTRIP)$(SUFFIX): fuzz_roundtrip.c $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

corpus: $(CORPUS_DECOMPRESS) $(CORPUS_COMPRESS) $(CORPUS_ROUNDTRIP)

$(CORPUS_DECOMPRESS):
	@mkdir -p $@
	@echo "Creating decompressor corpus from test vectors..."
	@# Generate seed files from compressed test vectors
	@for f in ../../../test-vectors/expected-output/*.pkt; do \
		if [ -f "$$f" ]; then \
			base=$$(basename "$$f" .pkt); \
			# Prepend header bytes (R=1, F=720 bits for housekeeping) \
			printf '\x01\x02\xD0' > "$@/$$base.seed"; \
			cat "$$f" >> "$@/$$base.seed"; \
		fi; \
	done
	@# Add minimal seeds
	@printf '\x00\x00\x08\x80' > $@/minimal_terminator.seed
	@printf '\x03\x00\x40\x80\x10' > $@/minimal_r3.seed
	@echo "Created $$(ls -1 $@ | wc -l | tr -d ' ') seed files"

$(CORPUS_COMPRESS):
	@mkdir -p $@
	@echo "Creating compressor corpus from test vectors..."
	@for f in ../../../test-vectors/input/*.bin; do \
		if [ -f "$$f" ]; then \
			base=$$(basename "$$f" .bin); \
			# Prepend header bytes (R=1, F=720, pt=10, ft=20) \
			printf '\x01\x02\xD0\xA4' > "$@/$$base.seed"; \
			head -c 1000 "$$f" >> "$@/$$base.seed"; \
		fi; \
	done
	@# Add minimal seeds
	@printf '\x00\x00\x08\x11\xAA\xBB\xCC\xDD' > $@/minimal_8bit.seed
	@printf '\x03\x00\x40\x22\x01\x02\x03\x04\x05\x06\x07\x08' > $@/minimal_64bit.seed
	@echo "Created $$(ls -1 $@ | wc -l | tr -d ' ') seed files"

$(CORPUS_ROUNDTRIP):
	@mkdir -p $@
	@echo "Creating roundtrip corpus from test vectors..."
	@for f in ../../../test-vectors/input/*.bin; do \
		if [ -f "$$f" ]; then \
			base=$$(basename "$$f" .bin); \
			printf '\x01\x02\xD0\xA4' > "$@/$$base.seed"; \
			head -c 1000 "$$f" >> "$@/$$base.seed"; \
		fi; \
	done
	@printf '\x00\x00\x08\x11\xAA\xBB\xCC\xDD\xEE\xFF\x00\x11' > $@/minimal.seed
	@echo "Created $$(ls -1 $@ | wc -l | tr -d ' ') seed files"

run-decompress: $(FUZZ_DECOMPRESS) $(CORPUS_DECOMPRESS)
	./$(FUZZ_DECOMPRESS) $(CORPUS_DECOMPRESS) -max_len=4096 -max_total_time=3600

run-compress: $(FUZZ_COMPRESS) $(CORPUS_COMPRESS)
	./$(FUZZ_COMPRESS) $(CORPUS_COMPRESS) -max_len=8192 -max_total_time=3600

run-roundtrip: $(FUZZ_ROUNDTRIP) $(CORPUS_ROUNDTRIP)
	./$(FUZZ_ROUNDTRIP) $(CORPUS_ROUNDTRIP) -max_len=4096 -max_total_time=3600

# Quick fuzz run (1 minute each)
quick: $(FUZZ_DECOMPRESS) $(FUZZ_ROUNDTRIP) corpus
	@echo "Running quick fuzz tests (1 minute each)..."
	./$(FUZZ_DECOMPRESS) $(CORPUS_DECOMPRESS) -max_len=2048 -max_total_time=60
	./$(FUZZ_ROUNDTRIP) $(CORPUS_ROUNDTRIP) -max_len=2048 -max_total_time=60

clean:
	rm -f $(FUZZ_DECOMPRESS) $(FUZZ_COMPRESS) $(FUZZ_ROUNDTRIP)
	rm -f $(FUZZ_DECOMPRESS)_afl $(FUZZ_COMPRESS)_afl $(FUZZ_ROUNDTRIP)_afl
	rm -rf $(CORPUS_DECOMPRESS) $(CORPUS_COMPRESS) $(CORPUS_ROUNDTRIP)
	rm -rf crash-* leak-* timeout-*

help:
	@echo "CCSDS 124.0-B-1 Fuzzing Targets:"
	@echo ""
	@echo "  make              Build all fuzzers with libFuzzer"
	@echo "  make AFL=1        Build all fuzzers with AFL++"
	@echo "  make corpus       Create initial corpus from test vectors"
	@echo "  make run-decompress  Run decompressor fuzzer (1 hour)"
	@echo "  make run-compress    Run compressor fuzzer (1 hour)"
	@echo "  make run-roundtrip   Run roundtrip fuzzer (1 hour)"
	@echo "  make quick           Quick fuzz run (1 minute each)"
	@echo "  make clean           Remove all build artifacts"
	@echo ""
	@echo "Requirements:"
	@echo "  - Clang with libFuzzer support"
	@echo "  - Or AFL++ for AFL=1 builds"
