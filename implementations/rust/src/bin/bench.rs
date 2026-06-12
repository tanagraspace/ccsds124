//! Performance benchmarks for CCSDS 124.0-B-1 compression.
//!
//! Measures compression throughput for regression testing during development.
//! Note: Desktop performance differs from embedded targets - use for relative
//! comparisons only.
//!
//! Usage:
//!   cargo run --release --bin bench          # Run with default 100 iterations
//!   cargo run --release --bin bench -- 1000  # Run with custom iteration count

#![allow(clippy::cast_precision_loss)]

use ccsds124::{compress, decompress};
use std::env;
use std::fs;
use std::path::Path;
use std::time::Instant;

const DEFAULT_ITERATIONS: usize = 100;
const PACKET_SIZE_BYTES: usize = 90;
const PACKET_SIZE_BITS: usize = PACKET_SIZE_BYTES * 8;

struct BenchConfig {
    name: &'static str,
    path: &'static str,
    robustness: usize,
    pt: usize,
    ft: usize,
    rt: usize,
}

const BENCHMARKS: &[BenchConfig] = &[
    BenchConfig {
        name: "simple",
        path: "../../test-vectors/input/simple.bin",
        robustness: 1,
        pt: 10,
        ft: 20,
        rt: 50,
    },
    BenchConfig {
        name: "hiro",
        path: "../../test-vectors/input/hiro.bin",
        robustness: 7,
        pt: 10,
        ft: 20,
        rt: 50,
    },
    BenchConfig {
        name: "housekeeping",
        path: "../../test-vectors/input/housekeeping.bin",
        robustness: 2,
        pt: 20,
        ft: 50,
        rt: 100,
    },
    BenchConfig {
        name: "venus-express",
        path: "../../test-vectors/input/venus-express.ccsds",
        robustness: 2,
        pt: 20,
        ft: 50,
        rt: 100,
    },
];

fn bench_compress(config: &BenchConfig, iterations: usize) {
    let path = Path::new(config.path);
    let Ok(input) = fs::read(path) else {
        println!("{:<20} SKIP (file not found)", config.name);
        return;
    };

    let num_packets = input.len() / PACKET_SIZE_BYTES;

    // Warmup run
    let _ = compress(
        &input,
        PACKET_SIZE_BITS,
        config.robustness,
        config.pt,
        config.ft,
        config.rt,
    );

    // Benchmark
    let start = Instant::now();

    for _ in 0..iterations {
        let _ = compress(
            &input,
            PACKET_SIZE_BITS,
            config.robustness,
            config.pt,
            config.ft,
            config.rt,
        );
    }

    let elapsed = start.elapsed();
    let total_us = elapsed.as_secs_f64() * 1_000_000.0;
    let per_iter_us = total_us / iterations as f64;
    let per_packet_us = per_iter_us / num_packets as f64;
    let throughput_kbps = (input.len() as f64 * 8.0 * 1000.0) / per_iter_us;

    println!(
        "{:<20} {:>8.2} µs/iter  {:>6.2} µs/pkt  {:>8.1} Kbps  ({} pkts)",
        config.name, per_iter_us, per_packet_us, throughput_kbps, num_packets
    );
}

fn bench_decompress(config: &BenchConfig, iterations: usize) {
    let path = Path::new(config.path);
    let Ok(input) = fs::read(path) else {
        println!("{:<20} SKIP (file not found)", config.name);
        return;
    };

    let num_packets = input.len() / PACKET_SIZE_BYTES;

    // First compress the data
    let compressed = match compress(
        &input,
        PACKET_SIZE_BITS,
        config.robustness,
        config.pt,
        config.ft,
        config.rt,
    ) {
        Ok(data) => data,
        Err(e) => {
            println!("{:<20} SKIP (compression failed: {:?})", config.name, e);
            return;
        }
    };

    // Warmup run
    let _ = decompress(&compressed, PACKET_SIZE_BITS, config.robustness);

    // Benchmark
    let start = Instant::now();

    for _ in 0..iterations {
        let _ = decompress(&compressed, PACKET_SIZE_BITS, config.robustness);
    }

    let elapsed = start.elapsed();
    let total_us = elapsed.as_secs_f64() * 1_000_000.0;
    let per_iter_us = total_us / iterations as f64;
    let per_packet_us = per_iter_us / num_packets as f64;
    let throughput_kbps = (input.len() as f64 * 8.0 * 1000.0) / per_iter_us;

    println!(
        "{:<20} {:>8.2} µs/iter  {:>6.2} µs/pkt  {:>8.1} Kbps  ({} pkts)",
        config.name, per_iter_us, per_packet_us, throughput_kbps, num_packets
    );
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let iterations = if args.len() >= 2 {
        args[1].parse().unwrap_or(DEFAULT_ITERATIONS)
    } else {
        DEFAULT_ITERATIONS
    };

    println!("CCSDS 124.0-B-1 Benchmarks (Rust Implementation)");
    println!("========================================");
    println!("Iterations: {iterations}");
    println!("Packet size: {PACKET_SIZE_BITS} bits ({PACKET_SIZE_BYTES} bytes)\n");

    println!(
        "{:<20} {:>14}  {:>13}  {:>12}  Packets",
        "Test", "Time", "Per-Packet", "Throughput"
    );
    println!(
        "{:<20} {:>14}  {:>13}  {:>12}  -------",
        "----", "----", "----------", "----------"
    );

    // Compression benchmarks
    println!("\nCompression:");
    for config in BENCHMARKS {
        bench_compress(config, iterations);
    }

    // Decompression benchmarks
    println!("\nDecompression:");
    for config in BENCHMARKS {
        bench_decompress(config, iterations);
    }

    println!("\nNote: Desktop performance differs from embedded targets.");
    println!("Use these results for relative comparisons only.");
}
