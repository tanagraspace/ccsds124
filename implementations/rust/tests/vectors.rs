//! Reference vector validation tests.
//!
//! These tests verify that the Rust implementation produces byte-identical
//! output to the C reference implementation for all test vectors.

use ccsds124::{compress, decompress};
use std::fs;
use std::path::Path;

/// Test vector configuration.
struct TestVector {
    name: &'static str,
    input_file: &'static str,
    expected_file: &'static str,
    packet_size: usize, // in bytes
    pt: usize,
    ft: usize,
    rt: usize,
    robustness: usize,
    expected_ratio: f64,
}

const TEST_VECTORS: &[TestVector] = &[
    TestVector {
        name: "simple",
        input_file: "simple.bin",
        expected_file: "simple.bin.pkt",
        packet_size: 90,
        pt: 10,
        ft: 20,
        rt: 50,
        robustness: 1,
        expected_ratio: 14.04,
    },
    TestVector {
        name: "housekeeping",
        input_file: "housekeeping.bin",
        expected_file: "housekeeping.bin.pkt",
        packet_size: 90,
        pt: 20,
        ft: 50,
        rt: 100,
        robustness: 2,
        expected_ratio: 4.03,
    },
    TestVector {
        name: "edge-cases",
        input_file: "edge-cases.bin",
        expected_file: "edge-cases.bin.pkt",
        packet_size: 90,
        pt: 10,
        ft: 20,
        rt: 50,
        robustness: 1,
        expected_ratio: 4.44,
    },
    TestVector {
        name: "hiro",
        input_file: "hiro.bin",
        expected_file: "hiro.bin.pkt",
        packet_size: 90,
        pt: 10,
        ft: 20,
        rt: 50,
        robustness: 7,
        expected_ratio: 5.87,
    },
    TestVector {
        name: "venus-express",
        input_file: "venus-express.ccsds",
        expected_file: "venus-express.ccsds.pkt",
        packet_size: 90,
        pt: 20,
        ft: 50,
        rt: 100,
        robustness: 2,
        expected_ratio: 2.31,
    },
];

/// Get the path to the test vectors directory.
fn test_vectors_path() -> Option<String> {
    // Try relative paths from different possible test execution directories
    let possible_paths = [
        "../../test-vectors",
        "../../../test-vectors",
        "test-vectors",
    ];

    for path in possible_paths {
        let input_path = format!("{}/input", path);
        if Path::new(&input_path).exists() {
            return Some(path.to_string());
        }
    }

    None
}

/// Read a file and return its contents.
fn read_file(path: &str) -> Vec<u8> {
    fs::read(path).unwrap_or_else(|e| panic!("Failed to read {}: {}", path, e))
}

/// Test compression produces byte-identical output.
fn test_compression(vector: &TestVector, base_path: &str) {
    let input_path = format!("{}/input/{}", base_path, vector.input_file);
    let expected_path = format!("{}/expected-output/{}", base_path, vector.expected_file);

    let input_data = read_file(&input_path);
    let expected_output = read_file(&expected_path);

    let packet_bits = vector.packet_size * 8;
    let compressed = compress(
        &input_data,
        packet_bits,
        vector.robustness,
        vector.pt,
        vector.ft,
        vector.rt,
    )
    .unwrap_or_else(|e| panic!("Compression failed for {}: {}", vector.name, e));

    let actual_ratio = input_data.len() as f64 / compressed.len() as f64;

    // Check compression ratio is within tolerance
    let ratio_diff = (actual_ratio - vector.expected_ratio).abs();
    assert!(
        ratio_diff < 0.02,
        "{}: Compression ratio {:.2}x differs from expected {:.2}x",
        vector.name,
        actual_ratio,
        vector.expected_ratio
    );

    // Check byte-identical output
    assert_eq!(
        compressed.len(),
        expected_output.len(),
        "{}: Compressed size {} differs from expected {}",
        vector.name,
        compressed.len(),
        expected_output.len()
    );

    assert_eq!(
        compressed, expected_output,
        "{}: Compressed output differs from expected",
        vector.name
    );
}

/// Test round-trip compression and decompression.
fn test_round_trip(vector: &TestVector, base_path: &str) {
    let input_path = format!("{}/input/{}", base_path, vector.input_file);
    let input_data = read_file(&input_path);

    let packet_bits = vector.packet_size * 8;

    // Compress
    let compressed = compress(
        &input_data,
        packet_bits,
        vector.robustness,
        vector.pt,
        vector.ft,
        vector.rt,
    )
    .unwrap_or_else(|e| panic!("Compression failed for {}: {}", vector.name, e));

    // Decompress
    let decompressed = decompress(&compressed, packet_bits, vector.robustness)
        .unwrap_or_else(|e| panic!("Decompression failed for {}: {}", vector.name, e));

    // Verify round-trip
    assert_eq!(
        decompressed.len(),
        input_data.len(),
        "{}: Decompressed size {} differs from original {}",
        vector.name,
        decompressed.len(),
        input_data.len()
    );

    assert_eq!(
        decompressed, input_data,
        "{}: Decompressed data differs from original",
        vector.name
    );
}

#[test]
fn test_simple_compression() {
    if let Some(base_path) = test_vectors_path() {
        test_compression(&TEST_VECTORS[0], &base_path);
    } else {
        eprintln!("Warning: Test vectors not found, skipping simple compression test");
    }
}

#[test]
fn test_simple_round_trip() {
    if let Some(base_path) = test_vectors_path() {
        test_round_trip(&TEST_VECTORS[0], &base_path);
    } else {
        eprintln!("Warning: Test vectors not found, skipping simple round-trip test");
    }
}

#[test]
fn test_housekeeping_compression() {
    if let Some(base_path) = test_vectors_path() {
        test_compression(&TEST_VECTORS[1], &base_path);
    } else {
        eprintln!("Warning: Test vectors not found, skipping housekeeping compression test");
    }
}

#[test]
fn test_housekeeping_round_trip() {
    if let Some(base_path) = test_vectors_path() {
        test_round_trip(&TEST_VECTORS[1], &base_path);
    } else {
        eprintln!("Warning: Test vectors not found, skipping housekeeping round-trip test");
    }
}

#[test]
fn test_edge_cases_compression() {
    if let Some(base_path) = test_vectors_path() {
        test_compression(&TEST_VECTORS[2], &base_path);
    } else {
        eprintln!("Warning: Test vectors not found, skipping edge-cases compression test");
    }
}

#[test]
fn test_edge_cases_round_trip() {
    if let Some(base_path) = test_vectors_path() {
        test_round_trip(&TEST_VECTORS[2], &base_path);
    } else {
        eprintln!("Warning: Test vectors not found, skipping edge-cases round-trip test");
    }
}

#[test]
fn test_hiro_compression() {
    if let Some(base_path) = test_vectors_path() {
        test_compression(&TEST_VECTORS[3], &base_path);
    } else {
        eprintln!("Warning: Test vectors not found, skipping hiro compression test");
    }
}

#[test]
fn test_hiro_round_trip() {
    if let Some(base_path) = test_vectors_path() {
        test_round_trip(&TEST_VECTORS[3], &base_path);
    } else {
        eprintln!("Warning: Test vectors not found, skipping hiro round-trip test");
    }
}

#[test]
fn test_venus_express_compression() {
    if let Some(base_path) = test_vectors_path() {
        test_compression(&TEST_VECTORS[4], &base_path);
    } else {
        eprintln!("Warning: Test vectors not found, skipping venus-express compression test");
    }
}

#[test]
fn test_venus_express_round_trip() {
    if let Some(base_path) = test_vectors_path() {
        test_round_trip(&TEST_VECTORS[4], &base_path);
    } else {
        eprintln!("Warning: Test vectors not found, skipping venus-express round-trip test");
    }
}

#[test]
fn test_all_vectors_available() {
    // This test just ensures we can find and read all test vectors
    if let Some(base_path) = test_vectors_path() {
        for vector in TEST_VECTORS {
            let input_path = format!("{}/input/{}", base_path, vector.input_file);
            let expected_path = format!("{}/expected-output/{}", base_path, vector.expected_file);

            assert!(
                Path::new(&input_path).exists(),
                "Missing input file: {}",
                input_path
            );
            assert!(
                Path::new(&expected_path).exists(),
                "Missing expected output file: {}",
                expected_path
            );

            println!("Found test vector: {}", vector.name);
        }
    } else {
        eprintln!("Warning: Test vectors directory not found");
    }
}
