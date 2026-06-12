# Multi-stage Dockerfile for deterministic CCSDS 124.0-B-1 test vector generation
# Pinned versions ensure reproducible builds across different systems

# Stage 1: Builder - Compile the C reference implementation
FROM alpine:3.18 AS builder

# Install build dependencies (versions determined by Alpine 3.18)
RUN apk add --no-cache \
    gcc \
    musl-dev \
    make

# Copy C reference implementation source files
WORKDIR /build
COPY test-vector-generator/c-reference/pocket_compress.c test-vector-generator/c-reference/pocket_decompress.c test-vector-generator/c-reference/Makefile ./

# Compile with static linking for determinism and portability
RUN make clean && make all CFLAGS="-O2 -static -no-pie"

# Verify binaries were built
RUN ls -lh pocket_compress pocket_decompress

# Stage 2: Generator - Python environment for test vector generation
FROM alpine:3.18 AS generator

# Install Python and runtime dependencies (versions determined by Alpine 3.18)
RUN apk add --no-cache \
    python3 \
    py3-pip \
    py3-numpy \
    py3-yaml \
    bash \
    coreutils

# Copy compiled binaries from builder stage
COPY --from=builder /build/pocket_compress /usr/local/bin/
COPY --from=builder /build/pocket_decompress /usr/local/bin/

# Set up working directory
WORKDIR /workspace

# Copy Venus Express reference data from test-vectors
COPY test-vectors/input/venus-express.ccsds /workspace/input-data/1028packets.ccsds
RUN md5sum /workspace/input-data/1028packets.ccsds

# Copy Python generators and configuration
COPY test-vector-generator/input-generators/ /workspace/input-generators/
COPY test-vector-generator/configs/ /workspace/configs/
COPY test-vector-generator/scripts/ /workspace/scripts/

# Make scripts executable
RUN chmod +x /workspace/scripts/*.sh

# Set environment variables for reproducibility
ENV PYTHONHASHSEED=0
ENV LC_ALL=C

# Default command
CMD ["/workspace/scripts/generate-all.sh"]
