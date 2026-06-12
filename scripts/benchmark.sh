#!/bin/bash
# Cross-implementation benchmark script for CCSDS 124.0-B-1 implementations
# Runs benchmarks for C, C++, Go, Rust, and Java implementations
# Generates docs/BENCHMARK.md with comparison tables

set -e

# Configuration
ITERATIONS=100
WARMUP=1
OUTPUT_FILE="/app/docs/BENCHMARK.md"

# Test vector info (packets, size in bytes, R, pt, ft, rt)
declare -A VECTORS
VECTORS[simple]="100 9000 1 10 20 50"
VECTORS[hiro]="100 9000 7 10 20 50"
VECTORS[housekeeping]="10000 900000 2 20 50 100"
VECTORS[venus-express]="151200 13608000 2 20 50 100"

# Results storage (associative arrays)
declare -A COMPRESS_RESULTS
declare -A DECOMPRESS_RESULTS

# Format a number with commas
format_number() {
    if [ "$1" = "-" ] || [ -z "$1" ]; then
        echo "-"
    else
        printf "%'d" "$1" 2>/dev/null || printf "%d" "$1"
    fi
}

# Calculate packets per second from time (µs) and packet count
# Input: time_us (microseconds, may be decimal), packets
calc_packets_per_sec() {
    local time_us=$1
    local packets=$2
    # Use bc for comparison since time_us may be decimal
    if [ "$(echo "$time_us > 0" | bc)" -eq 1 ]; then
        echo "scale=0; ($packets * 1000000) / $time_us" | bc
    else
        echo "-"
    fi
}

# Calculate throughput in MB/s from size (bytes) and time (µs)
# Input: size_bytes, time_us (microseconds, may be decimal)
# Formula: (bytes / 1048576) / (us / 1000000) = (bytes * 1000000) / (us * 1048576)
calc_throughput_mbs() {
    local size_bytes=$1
    local time_us=$2
    # Use bc for comparison since time_us may be decimal
    if [ "$(echo "$time_us > 0" | bc)" -eq 1 ]; then
        echo "scale=2; ($size_bytes * 1000000) / ($time_us * 1048576)" | bc
    else
        echo "-"
    fi
}

# Run C benchmarks
run_c_bench() {
    echo "Running C benchmarks..."
    cd /app/implementations/c

    if [ -f "build/test_bench" ]; then
        local output
        output=$(./build/test_bench 2>&1) || true

        # Parse output: "test-name    XX.XX µs/iter  X.XX µs/pkt  XXXX.X Kbps  (NNN pkts)"
        while IFS= read -r line; do
            if [[ "$line" =~ ^([a-z-]+)[[:space:]]+([0-9.]+)[[:space:]]+µs/iter ]]; then
                local test_name="${BASH_REMATCH[1]}"
                local us_per_iter="${BASH_REMATCH[2]}"

                # Determine if compress or decompress based on output order
                # C bench runs compress then decompress for each vector
                local vector_info="${VECTORS[$test_name]}"
                if [ -n "$vector_info" ]; then
                    read -r packets size_bytes r pt ft rt <<< "$vector_info"
                    local time_ms us_per_pkt pps throughput
                    time_ms=$(echo "scale=3; $us_per_iter / 1000" | bc)
                    us_per_pkt=$(echo "scale=2; $us_per_iter / $packets" | bc)
                    pps=$(calc_packets_per_sec "$us_per_iter" "$packets")
                    throughput=$(calc_throughput_mbs "$size_bytes" "$us_per_iter")

                    # Store result: time_ms|us_per_pkt|packets_per_sec|throughput_mbs
                    if [ -z "${COMPRESS_RESULTS[c_$test_name]}" ]; then
                        COMPRESS_RESULTS["c_$test_name"]="$time_ms|$us_per_pkt|$pps|$throughput"
                    else
                        DECOMPRESS_RESULTS["c_$test_name"]="$time_ms|$us_per_pkt|$pps|$throughput"
                    fi
                fi
            fi
        done <<< "$output"
    else
        echo "  Warning: C benchmark binary not found"
    fi
}

# Run C++ benchmarks
run_cpp_bench() {
    echo "Running C++ benchmarks..."
    cd /app/implementations/cpp

    if [ -f "build/bench" ]; then
        local output
        output=$(./build/bench 2>&1) || true

        # Parse output - same format as C
        while IFS= read -r line; do
            if [[ "$line" =~ ^([a-z-]+)[[:space:]]+([0-9.]+)[[:space:]]+µs/iter ]]; then
                local test_name="${BASH_REMATCH[1]}"
                local us_per_iter="${BASH_REMATCH[2]}"

                local vector_info="${VECTORS[$test_name]}"
                if [ -n "$vector_info" ]; then
                    read -r packets size_bytes r pt ft rt <<< "$vector_info"
                    local time_ms us_per_pkt pps throughput
                    time_ms=$(echo "scale=3; $us_per_iter / 1000" | bc)
                    us_per_pkt=$(echo "scale=2; $us_per_iter / $packets" | bc)
                    pps=$(calc_packets_per_sec "$us_per_iter" "$packets")
                    throughput=$(calc_throughput_mbs "$size_bytes" "$us_per_iter")

                    if [ -z "${COMPRESS_RESULTS[cpp_$test_name]}" ]; then
                        COMPRESS_RESULTS["cpp_$test_name"]="$time_ms|$us_per_pkt|$pps|$throughput"
                    else
                        DECOMPRESS_RESULTS["cpp_$test_name"]="$time_ms|$us_per_pkt|$pps|$throughput"
                    fi
                fi
            fi
        done <<< "$output"
    else
        echo "  Warning: C++ benchmark binary not found"
    fi
}

# Run Go benchmarks
run_go_bench() {
    echo "Running Go benchmarks..."
    cd /app/implementations/go

    local output
    output=$(go test -bench=. -benchtime=100x ./ccsds124 2>&1) || true

    # Parse Go output: "BenchmarkCompressSimple-N    100    XXXX ns/op    XX.XX MB/s"
    while IFS= read -r line; do
        if [[ "$line" =~ Benchmark(Compress|Decompress)([A-Za-z]+)-[0-9]+[[:space:]]+[0-9]+[[:space:]]+([0-9]+)[[:space:]]ns/op ]]; then
            local op_type="${BASH_REMATCH[1]}"
            local vector_name="${BASH_REMATCH[2]}"
            local ns_per_op="${BASH_REMATCH[3]}"

            # Convert vector name to lowercase with hyphens
            local test_name
            test_name=$(echo "$vector_name" | sed 's/\([A-Z]\)/-\1/g' | sed 's/^-//' | tr '[:upper:]' '[:lower:]')
            # Handle special cases
            test_name="${test_name/venus-express/venus-express}"

            local vector_info="${VECTORS[$test_name]}"
            if [ -n "$vector_info" ]; then
                read -r packets size_bytes r pt ft rt <<< "$vector_info"
                # Convert ns to µs for calculations
                local us_per_iter time_ms us_per_pkt pps throughput
                us_per_iter=$(echo "scale=2; $ns_per_op / 1000" | bc)
                time_ms=$(echo "scale=3; $ns_per_op / 1000000" | bc)
                us_per_pkt=$(echo "scale=2; $us_per_iter / $packets" | bc)
                pps=$(calc_packets_per_sec "$us_per_iter" "$packets")
                throughput=$(calc_throughput_mbs "$size_bytes" "$us_per_iter")

                if [ "$op_type" = "Compress" ]; then
                    COMPRESS_RESULTS["go_$test_name"]="$time_ms|$us_per_pkt|$pps|$throughput"
                else
                    DECOMPRESS_RESULTS["go_$test_name"]="$time_ms|$us_per_pkt|$pps|$throughput"
                fi
            fi
        fi
    done <<< "$output"
}

# Run Rust benchmarks
run_rust_bench() {
    echo "Running Rust benchmarks..."
    cd /app/implementations/rust

    if [ -f "target/release/bench" ]; then
        local output
        output=$(./target/release/bench 2>&1) || true

        # Parse output - same format as C/C++
        while IFS= read -r line; do
            if [[ "$line" =~ ^([a-z-]+)[[:space:]]+([0-9.]+)[[:space:]]+µs/iter ]]; then
                local test_name="${BASH_REMATCH[1]}"
                local us_per_iter="${BASH_REMATCH[2]}"

                local vector_info="${VECTORS[$test_name]}"
                if [ -n "$vector_info" ]; then
                    read -r packets size_bytes r pt ft rt <<< "$vector_info"
                    local time_ms us_per_pkt pps throughput
                    time_ms=$(echo "scale=3; $us_per_iter / 1000" | bc)
                    us_per_pkt=$(echo "scale=2; $us_per_iter / $packets" | bc)
                    pps=$(calc_packets_per_sec "$us_per_iter" "$packets")
                    throughput=$(calc_throughput_mbs "$size_bytes" "$us_per_iter")

                    if [ -z "${COMPRESS_RESULTS[rust_$test_name]}" ]; then
                        COMPRESS_RESULTS["rust_$test_name"]="$time_ms|$us_per_pkt|$pps|$throughput"
                    else
                        DECOMPRESS_RESULTS["rust_$test_name"]="$time_ms|$us_per_pkt|$pps|$throughput"
                    fi
                fi
            fi
        done <<< "$output"
    else
        echo "  Warning: Rust benchmark binary not found"
    fi
}

# Run Java benchmarks
run_java_bench() {
    echo "Running Java benchmarks..."
    cd /app/implementations/java

    local output
    output=$(java -cp target/classes space.tanagra.ccsds124.cli.Bench 2>&1) || true

    # Parse output - same format as C/C++
    while IFS= read -r line; do
        if [[ "$line" =~ ^([a-z-]+)[[:space:]]+([0-9.]+)[[:space:]]+µs/iter ]]; then
            local test_name="${BASH_REMATCH[1]}"
            local us_per_iter="${BASH_REMATCH[2]}"

            local vector_info="${VECTORS[$test_name]}"
            if [ -n "$vector_info" ]; then
                read -r packets size_bytes r pt ft rt <<< "$vector_info"
                local time_ms us_per_pkt pps throughput
                time_ms=$(echo "scale=3; $us_per_iter / 1000" | bc)
                us_per_pkt=$(echo "scale=2; $us_per_iter / $packets" | bc)
                pps=$(calc_packets_per_sec "$us_per_iter" "$packets")
                throughput=$(calc_throughput_mbs "$size_bytes" "$us_per_iter")

                if [ -z "${COMPRESS_RESULTS[java_$test_name]}" ]; then
                    COMPRESS_RESULTS["java_$test_name"]="$time_ms|$us_per_pkt|$pps|$throughput"
                else
                    DECOMPRESS_RESULTS["java_$test_name"]="$time_ms|$us_per_pkt|$pps|$throughput"
                fi
            fi
        fi
    done <<< "$output"
}

# Get result value or placeholder
get_result() {
    local key=$1
    local field=$2
    local result="${!key}"

    if [ -n "$result" ]; then
        echo "$result" | cut -d'|' -f"$field"
    else
        echo "-"
    fi
}

# Generate comparison table for a test vector
generate_vector_table() {
    local vector=$1
    local op_type=$2  # "compress" or "decompress"
    local results_array=$3

    local vector_info="${VECTORS[$vector]}"
    read -r packets size_bytes r pt ft rt <<< "$vector_info"
    local size_kb
    size_kb=$(echo "scale=1; $size_bytes / 1000" | bc -l)

    if [ "$op_type" = "compress" ]; then
        echo "### Compression: $vector ($packets packets, ${size_kb} KB)"
    else
        echo "### Decompression: $vector ($packets packets, ${size_kb} KB)"
    fi
    echo ""
    echo "| Implementation | Time (ms) | Packets/sec | µs/pkt | Throughput (MB/s) |"
    echo "|----------------|-----------|-------------|--------|-------------------|"

    for lang in c cpp go rust java; do
        local key
        if [ "$op_type" = "compress" ]; then
            key="COMPRESS_RESULTS[${lang}_${vector}]"
        else
            key="DECOMPRESS_RESULTS[${lang}_${vector}]"
        fi

        local result="${!key}"
        local lang_name
        case $lang in
            c) lang_name="C" ;;
            cpp) lang_name="C++" ;;
            go) lang_name="Go" ;;
            rust) lang_name="Rust" ;;
            java) lang_name="Java" ;;
        esac

        if [ -n "$result" ]; then
            local time_ms us_pkt pps throughput
            time_ms=$(echo "$result" | cut -d'|' -f1)
            us_pkt=$(echo "$result" | cut -d'|' -f2)
            pps=$(echo "$result" | cut -d'|' -f3)
            throughput=$(echo "$result" | cut -d'|' -f4)

            # Format packets/sec with commas
            pps_formatted=$(format_number "$pps")

            printf "| %-14s | %9s | %11s | %6s | %17s |\n" \
                "$lang_name" "$time_ms" "$pps_formatted" "$us_pkt" "$throughput"
        else
            printf "| %-14s | %9s | %11s | %6s | %17s |\n" \
                "$lang_name" "-" "-" "-" "-"
        fi
    done
    echo ""
}

# Gather system information
get_system_info() {
    local os_info cpu_info mem_info arch_info cpu_cores

    # OS information
    if [ -f /etc/os-release ]; then
        os_info=$(grep PRETTY_NAME /etc/os-release | cut -d'"' -f2)
    else
        os_info=$(uname -s -r)
    fi

    # Architecture
    arch_info=$(uname -m)

    # CPU information
    if [ -f /proc/cpuinfo ]; then
        # Try x86 format first (model name)
        cpu_info=$(grep "model name" /proc/cpuinfo | head -1 | cut -d':' -f2 | sed 's/^ //')
        # If empty, try ARM format (CPU architecture + features summary)
        if [ -z "$cpu_info" ]; then
            local cpu_arch cpu_impl
            cpu_arch=$(grep "CPU architecture" /proc/cpuinfo | head -1 | cut -d':' -f2 | sed 's/^ //')
            cpu_impl=$(grep "CPU implementer" /proc/cpuinfo | head -1 | cut -d':' -f2 | sed 's/^ //')
            if [ -n "$cpu_arch" ]; then
                cpu_info="ARMv${cpu_arch}"
            fi
        fi
    fi
    if [ -z "$cpu_info" ]; then
        cpu_info=$(uname -m)
    fi

    # CPU cores
    cpu_cores=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "unknown")

    # Memory information
    if [ -f /proc/meminfo ]; then
        local mem_kb
        mem_kb=$(grep MemTotal /proc/meminfo | awk '{print $2}')
        mem_info=$(echo "scale=1; $mem_kb / 1048576" | bc)" GB"
    else
        mem_info="unknown"
    fi

    printf 'OS_INFO=%q\n' "$os_info"
    printf 'ARCH_INFO=%q\n' "$arch_info"
    printf 'CPU_INFO=%q\n' "$cpu_info"
    printf 'CPU_CORES=%q\n' "$cpu_cores"
    printf 'MEM_INFO=%q\n' "$mem_info"
}

# Gather compiler/runtime versions
get_compiler_info() {
    local gcc_version gxx_version go_version rust_version java_version

    # GCC version (for C) - extract just X.Y.Z version number
    gcc_version=$(gcc --version 2>/dev/null | head -1 | sed 's/.*) //' | awk '{print $1}' | cut -d'_' -f1 || echo "unknown")

    # G++ version (for C++) - extract just X.Y.Z version number
    gxx_version=$(g++ --version 2>/dev/null | head -1 | sed 's/.*) //' | awk '{print $1}' | cut -d'_' -f1 || echo "unknown")

    # Go version
    go_version=$(go version 2>/dev/null | awk '{print $3}' | sed 's/go//' || echo "unknown")

    # Rust version
    rust_version=$(rustc --version 2>/dev/null | awk '{print $2}' || echo "unknown")

    # Java version
    java_version=$(java -version 2>&1 | head -1 | awk -F '"' '{print $2}' || echo "unknown")

    printf 'GCC_VERSION=%q\n' "$gcc_version"
    printf 'GXX_VERSION=%q\n' "$gxx_version"
    printf 'GO_VERSION=%q\n' "$go_version"
    printf 'RUST_VERSION=%q\n' "$rust_version"
    printf 'JAVA_VERSION=%q\n' "$java_version"
}

# Generate the markdown report
generate_markdown() {
    echo "Generating benchmark report..."

    mkdir -p /app/docs

    # Gather system info
    eval "$(get_system_info)"
    eval "$(get_compiler_info)"

    cat > "$OUTPUT_FILE" << EOF
# CCSDS 124.0-B-1 Benchmark Results

Performance comparison across CCSDS 124.0-B-1 implementations.

## Environment

| Property | Value |
|----------|-------|
| OS | $OS_INFO |
| Architecture | $ARCH_INFO |
| CPU | $CPU_INFO |
| CPU Cores | $CPU_CORES |
| Memory | $MEM_INFO |
| Container | Docker (Alpine 3.20) |
| Date | $(date -u '+%Y-%m-%d %H:%M:%S UTC') |

## Build Settings

| Language | Compiler/Runtime | Version | Optimization Flags |
|----------|------------------|---------|-------------------|
| C | GCC | $GCC_VERSION | \`-O3 -flto\` |
| C++ | G++ | $GXX_VERSION | \`-O3\` (CMake Release) |
| Go | Go | $GO_VERSION | Default |
| Rust | rustc | $RUST_VERSION | \`--release\` (opt-level=3) |
| Java | OpenJDK | $JAVA_VERSION | Default (JIT) |

## Test Vectors

All vectors are synthetic test data except for venus-express, which contains real housekeeping telemetry from ESA's [Venus Express](https://www.esa.int/Science_Exploration/Space_Science/Venus_Express) mission.

| Vector | Packets | Size | R | pt | ft | rt |
|--------|---------|------|---|----|----|-----|
EOF

    # Add test vector rows
    for vector in simple hiro housekeeping venus-express; do
        local vector_info="${VECTORS[$vector]}"
        read -r packets size_bytes r pt ft rt <<< "$vector_info"
        local size_display
        if [ "$size_bytes" -ge 1000000 ]; then
            size_display=$(echo "scale=1; $size_bytes / 1000000" | bc -l)" MB"
        else
            size_display=$(echo "scale=1; $size_bytes / 1000" | bc -l)" KB"
        fi
        echo "| $vector | $(format_number "$packets") | $size_display | $r | $pt | $ft | $rt |" >> "$OUTPUT_FILE"
    done

    cat >> "$OUTPUT_FILE" << 'SECTION'

## Compression Results

SECTION

    # Generate compression tables for each vector
    for vector in simple hiro housekeeping venus-express; do
        generate_vector_table "$vector" "compress" >> "$OUTPUT_FILE"
    done

    cat >> "$OUTPUT_FILE" << 'SECTION'
## Decompression Results

SECTION

    # Generate decompression tables for each vector
    for vector in simple hiro housekeeping venus-express; do
        generate_vector_table "$vector" "decompress" >> "$OUTPUT_FILE"
    done

    cat >> "$OUTPUT_FILE" << 'FOOTER'
## Methodology

- **Iterations**: 100 (with 1 warmup iteration)
- **Packet size**: 90 bytes (720 bits)
- **Timing**: Wall-clock time for complete file processing

## Notes

- **Python** is excluded from benchmarks as it is not performance-focused.
  The Python implementation prioritizes readability and correctness over speed.
- Results may vary based on system load and hardware.
- Go benchmarks use the native `go test -bench` framework.
- All other implementations use custom benchmark binaries with consistent output format.
FOOTER

    echo "Report generated: $OUTPUT_FILE"
}

# Main execution
main() {
    echo "=========================================="
    echo "CCSDS 124.0-B-1 Cross-Implementation Benchmarks"
    echo "=========================================="
    echo ""

    # Run all benchmarks
    run_c_bench
    run_cpp_bench
    run_go_bench
    run_rust_bench
    run_java_bench

    echo ""
    echo "=========================================="
    echo "Generating Report"
    echo "=========================================="

    # Generate markdown report
    generate_markdown

    echo ""
    echo "=========================================="
    echo "Benchmark Complete"
    echo "=========================================="

    # Display results summary
    cat "$OUTPUT_FILE"
}

main "$@"
