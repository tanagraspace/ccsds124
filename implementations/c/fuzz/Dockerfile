# Dockerfile for CCSDS 124.0-B-1 fuzzing with libFuzzer
# Based on alpine:edge with Clang/libFuzzer
FROM alpine:edge

# Install Clang with libFuzzer and compiler-rt
RUN apk add --no-cache \
    clang \
    compiler-rt \
    make \
    --repository=http://dl-cdn.alpinelinux.org/alpine/edge/main \
    --repository=http://dl-cdn.alpinelinux.org/alpine/edge/community

WORKDIR /app

# Copy source files
COPY include/ include/
COPY src/ src/
COPY fuzz/ fuzz/

# Build fuzz harnesses
WORKDIR /app/fuzz
RUN make

# Create corpus directories with seed files
RUN mkdir -p corpus_decompress corpus_compress corpus_roundtrip && \
    printf '\x00\x00\x08\x80' > corpus_decompress/minimal_terminator.seed && \
    printf '\x03\x00\x40\x80\x10' > corpus_decompress/minimal_r3.seed && \
    printf '\x00\x00\x08\x11\xAA\xBB\xCC\xDD' > corpus_compress/minimal_8bit.seed && \
    printf '\x03\x00\x40\x22\x01\x02\x03\x04\x05\x06\x07\x08' > corpus_compress/minimal_64bit.seed && \
    printf '\x00\x00\x08\x11\xAA\xBB\xCC\xDD\xEE\xFF\x00\x11' > corpus_roundtrip/minimal.seed

# Default: run all fuzzers for 60 seconds each
CMD ["sh", "-c", "\
    echo '=== Running Decompress Fuzzer ===' && \
    ./fuzz_decompress corpus_decompress/ -max_len=4096 -max_total_time=${FUZZ_DURATION:-60} && \
    echo '' && \
    echo '=== Running Compress Fuzzer ===' && \
    ./fuzz_compress corpus_compress/ -max_len=8192 -max_total_time=${FUZZ_DURATION:-60} && \
    echo '' && \
    echo '=== Running Roundtrip Fuzzer ===' && \
    ./fuzz_roundtrip corpus_roundtrip/ -max_len=4096 -max_total_time=${FUZZ_DURATION:-60} && \
    echo '' && \
    echo '=== Fuzzing Complete ===' && \
    echo 'Crashes:' && find . -name 'crash-*' -type f 2>/dev/null | wc -l && \
    echo 'Leaks:' && find . -name 'leak-*' -type f 2>/dev/null | wc -l && \
    echo 'Timeouts:' && find . -name 'timeout-*' -type f 2>/dev/null | wc -l \
"]
