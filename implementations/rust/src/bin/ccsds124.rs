//! CCSDS 124.0-B-1 Command Line Interface
//!
//! A unified command-line interface for CCSDS 124.0-B-1 compression and decompression.
//!
//! Usage:
//!   ccsds124 input packet_size pt ft rt robustness    # compress
//!   ccsds124 -d input.pkt packet_size robustness      # decompress
//!   ccsds124 --version
//!   ccsds124 --help

#![allow(clippy::cast_possible_truncation)]
#![allow(clippy::cast_precision_loss)]
#![allow(clippy::doc_markdown)]

use ccsds124::{compress, decompress};
use std::env;
use std::fs::{self, File};
use std::io::{Read, Write};
use std::path::Path;
use std::process;

const VERSION: &str = env!("CARGO_PKG_VERSION");

/// ASCII art banner for help output.
const BANNER: &str = r"
  ____ ____ ____  ____  ____    _ ____  _  _
 / ___/ ___/ ___||  _ \/ ___|  / |___ \| || |
| |  | |   \___ \| | | \___ \  | | __) | || |_
| |__| |___ ___) | |_| |___) | | |/ __/|__   _|
 \____\____|____/|____/|____/  |_|_____|  |_|

         by  T A N A G R A  S P A C E
";

/// Print version information.
fn print_version() {
    println!("ccsds124 {VERSION}");
}

/// Print help message with usage information.
fn print_help(prog_name: &str) {
    println!("{BANNER}");
    println!("CCSDS 124.0-B-1 Lossless Compression (v{VERSION})");
    println!("=================================================\n");
    println!("References:");
    println!("  CCSDS 124.0-B-1: https://ccsds.org/Pubs/124x0b1.pdf");
    println!("  Documentation: https://tanagraspace.com/ccsds124\n");
    println!("Citation:");
    println!("  D. Evans, G. Labreche, D. Marszk, S. Bammens, M. Hernandez-Cabronero,");
    println!("  V. Zelenevskiy, V. Shiradhonkar, M. Starcik, and M. Henkel. 2022.");
    println!("  \"Implementing the New CCSDS Housekeeping Data Compression Standard");
    println!("  124.0-B-1 (based on POCKET+) on OPS-SAT-1,\" Proceedings of the");
    println!("  Small Satellite Conference, Communications, SSC22-XII-03.");
    println!("  https://digitalcommons.usu.edu/smallsat/2022/all2022/133/\n");
    println!("Usage:");
    println!("  {prog_name} <input> <packet_size> <pt> <ft> <rt> <robustness>");
    println!("  {prog_name} -d <input.pkt> <packet_size> <robustness>\n");
    println!("Options:");
    println!("  -d             Decompress (default is compress)");
    println!("  -h, --help     Show this help message");
    println!("  -v, --version  Show version information\n");
    println!("Compress arguments:");
    println!("  input          Input file to compress");
    println!("  packet_size    Packet size in bytes (e.g., 90)");
    println!("  pt             New mask period (e.g., 10, 20)");
    println!("  ft             Send mask period (e.g., 20, 50)");
    println!("  rt             Uncompressed period (e.g., 50, 100)");
    println!("  robustness     Robustness level 0-7 (e.g., 1, 2)\n");
    println!("Decompress arguments:");
    println!("  input.pkt      Compressed input file");
    println!("  packet_size    Original packet size in bytes");
    println!("  robustness     Robustness level (must match compression)\n");
    println!("Output:");
    println!("  Compress:   <input>.pkt");
    println!("  Decompress: <input>.depkt (or <base>.depkt if input ends in .pkt)\n");
    println!("Examples:");
    println!("  {prog_name} data.bin 90 10 20 50 1        # compress");
    println!("  {prog_name} -d data.bin.pkt 90 1          # decompress");
}

/// Create output filename for decompression.
///
/// Removes .pkt extension if present, then appends .depkt.
fn make_decompress_filename(input: &str) -> String {
    if let Some(stripped) = input.strip_suffix(".pkt") {
        format!("{stripped}.depkt")
    } else if let Some(stripped) = input.strip_suffix(".PKT") {
        format!("{stripped}.depkt")
    } else {
        format!("{input}.depkt")
    }
}

/// Read a file into a byte vector.
fn read_file(path: &str) -> Result<Vec<u8>, String> {
    let mut file = File::open(path).map_err(|e| format!("Cannot open input file: {e}"))?;

    let metadata = fs::metadata(path).map_err(|e| format!("Cannot read file metadata: {e}"))?;

    let mut buffer = Vec::with_capacity(metadata.len() as usize);
    file.read_to_end(&mut buffer)
        .map_err(|e| format!("Failed to read input file: {e}"))?;

    if buffer.is_empty() {
        return Err("Input file is empty".to_string());
    }

    Ok(buffer)
}

/// Write a byte vector to a file.
fn write_file(path: &str, data: &[u8]) -> Result<(), String> {
    let mut file = File::create(path).map_err(|e| format!("Cannot create output file: {e}"))?;

    file.write_all(data)
        .map_err(|e| format!("Failed to write output file: {e}"))?;

    Ok(())
}

/// Compress a file.
fn do_compress(
    input_path: &str,
    packet_size: usize,
    pt_period: usize,
    ft_period: usize,
    rt_period: usize,
    robustness: usize,
) -> Result<(), String> {
    // Read input file
    let input_data = read_file(input_path)?;
    let input_size = input_data.len();

    // Validate input size
    if input_size % packet_size != 0 {
        return Err(format!(
            "Input size ({input_size}) not divisible by packet size ({packet_size})"
        ));
    }

    // Create output filename
    let output_path = format!("{input_path}.pkt");

    // Compress
    let packet_bits = packet_size * 8;
    let output_data = compress(
        &input_data,
        packet_bits,
        robustness,
        pt_period,
        ft_period,
        rt_period,
    )
    .map_err(|e| format!("Compression failed: {e}"))?;

    let output_size = output_data.len();

    // Write output
    write_file(&output_path, &output_data)?;

    // Print summary
    let num_packets = input_size / packet_size;
    let ratio = input_size as f64 / output_size as f64;
    println!("Input:       {input_path} ({input_size} bytes, {num_packets} packets)");
    println!("Output:      {output_path} ({output_size} bytes)");
    println!("Ratio:       {ratio:.2}x");
    println!("Parameters:  R={robustness}, pt={pt_period}, ft={ft_period}, rt={rt_period}");

    Ok(())
}

/// Decompress a file.
fn do_decompress(input_path: &str, packet_size: usize, robustness: usize) -> Result<(), String> {
    // Read input file
    let input_data = read_file(input_path)?;
    let input_size = input_data.len();

    // Create output filename
    let output_path = make_decompress_filename(input_path);

    // Decompress
    let packet_bits = packet_size * 8;
    let output_data = decompress(&input_data, packet_bits, robustness)
        .map_err(|e| format!("Decompression failed: {e}"))?;

    let output_size = output_data.len();

    // Write output
    write_file(&output_path, &output_data)?;

    // Print summary
    let num_packets = output_size / packet_size;
    let ratio = output_size as f64 / input_size as f64;
    println!("Input:       {input_path} ({input_size} bytes)");
    println!("Output:      {output_path} ({output_size} bytes, {num_packets} packets)");
    println!("Expansion:   {ratio:.2}x");
    println!("Parameters:  packet_size={packet_size}, R={robustness}");

    Ok(())
}

/// Parse a positive integer from a string argument.
fn parse_positive(s: &str, name: &str) -> Result<usize, String> {
    s.parse::<usize>()
        .map_err(|_| format!("{name} must be a positive integer"))
        .and_then(|v| {
            if v == 0 {
                Err(format!("{name} must be positive"))
            } else {
                Ok(v)
            }
        })
}

/// Parse robustness value (0-7).
fn parse_robustness(s: &str) -> Result<usize, String> {
    let value = s
        .parse::<usize>()
        .map_err(|_| "robustness must be a number".to_string())?;

    if value > 7 {
        return Err("robustness must be 0-7".to_string());
    }

    Ok(value)
}

/// Handle decompress mode.
fn handle_decompress(args: &[String], prog_name: &str) {
    if args.len() != 5 {
        eprintln!("Error: Decompress requires 3 arguments after -d");
        eprintln!("Usage: {prog_name} -d <input.pkt> <packet_size> <robustness>");
        process::exit(1);
    }

    let input_path = &args[2];
    let packet_size = match parse_positive(&args[3], "packet_size") {
        Ok(v) => v,
        Err(e) => {
            eprintln!("Error: {e}");
            process::exit(1);
        }
    };
    let robustness = match parse_robustness(&args[4]) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("Error: {e}");
            process::exit(1);
        }
    };

    if packet_size > 8192 {
        eprintln!("Error: packet_size must be 1-8192 bytes");
        process::exit(1);
    }

    if let Err(e) = do_decompress(input_path, packet_size, robustness) {
        eprintln!("Error: {e}");
        process::exit(1);
    }
}

/// Handle compress mode.
fn handle_compress(args: &[String], prog_name: &str) {
    if args.len() != 7 {
        eprintln!("Error: Compress requires 6 arguments");
        eprintln!("Usage: {prog_name} <input> <packet_size> <pt> <ft> <rt> <robustness>");
        process::exit(1);
    }

    let input_path = &args[1];
    let packet_size = match parse_positive(&args[2], "packet_size") {
        Ok(v) => v,
        Err(e) => {
            eprintln!("Error: {e}");
            process::exit(1);
        }
    };
    let pt_period = match parse_positive(&args[3], "pt") {
        Ok(v) => v,
        Err(e) => {
            eprintln!("Error: {e}");
            process::exit(1);
        }
    };
    let ft_period = match parse_positive(&args[4], "ft") {
        Ok(v) => v,
        Err(e) => {
            eprintln!("Error: {e}");
            process::exit(1);
        }
    };
    let rt_period = match parse_positive(&args[5], "rt") {
        Ok(v) => v,
        Err(e) => {
            eprintln!("Error: {e}");
            process::exit(1);
        }
    };
    let robustness = match parse_robustness(&args[6]) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("Error: {e}");
            process::exit(1);
        }
    };

    if packet_size > 8192 {
        eprintln!("Error: packet_size must be 1-8192 bytes");
        process::exit(1);
    }

    if let Err(e) = do_compress(
        input_path,
        packet_size,
        pt_period,
        ft_period,
        rt_period,
        robustness,
    ) {
        eprintln!("Error: {e}");
        process::exit(1);
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let prog_name = Path::new(&args[0])
        .file_name()
        .map_or("ccsds124", |s| s.to_str().unwrap_or("ccsds124"));

    if args.len() < 2 {
        print_help(prog_name);
        process::exit(1);
    }

    match args[1].as_str() {
        "-h" | "--help" => print_help(prog_name),
        "-v" | "--version" => print_version(),
        "-d" => handle_decompress(&args, prog_name),
        _ => handle_compress(&args, prog_name),
    }
}
