#!/usr/bin/env python3
"""
CCSDS 124.0-B-1 command line interface.

Provides compression and decompression for CCSDS 124.0-B-1.

Note: This CLI uses sys.argv instead of argparse for MicroPython compatibility.
MicroPython does not include argparse in its standard library, so we use
manual argument parsing to ensure the CLI works on embedded systems
(ESP32, RP2040, etc.) as well as standard Python.

Usage:
    python cli.py <input> <packet_size> <pt> <ft> <rt> <robustness>
    python cli.py -d <input.pkt> <packet_size> <robustness>

Examples:
    python cli.py data.bin 90 10 20 50 1        # compress
    python cli.py -d data.bin.pkt 90 1          # decompress
"""

import sys

from ccsds124 import __version__, compress, decompress

BANNER = """
  ____ ____ ____  ____  ____    _ ____  _  _
 / ___/ ___/ ___||  _ \\/ ___|  / |___ \\| || |
| |  | |   \\___ \\| | | \\___ \\  | | __) | || |_
| |__| |___ ___) | |_| |___) | | |/ __/|__   _|
 \\____\\____|____/|____/|____/  |_|_____|  |_|

         by  T A N A G R A  S P A C E
"""


def print_version() -> None:
    """Print version information."""
    print(f"ccsds124 {__version__}")


def print_help(prog_name: str) -> None:
    """Print help message."""
    print(BANNER)
    print(f"CCSDS 124.0-B-1 Lossless Compression (v{__version__})")
    print("=" * 49)
    print()
    print("References:")
    print("  CCSDS 124.0-B-1: https://ccsds.org/Pubs/124x0b1.pdf")
    print("  Documentation: https://tanagraspace.com/ccsds124")
    print()
    print("Citation:")
    print("  D. Evans, G. Labreche, D. Marszk, S. Bammens, M. Hernandez-Cabronero,")
    print("  V. Zelenevskiy, V. Shiradhonkar, M. Starcik, and M. Henkel. 2022.")
    print('  "Implementing the New CCSDS Housekeeping Data Compression Standard')
    print('  124.0-B-1 (based on POCKET+) on OPS-SAT-1," Proceedings of the')
    print("  Small Satellite Conference, Communications, SSC22-XII-03.")
    print("  https://digitalcommons.usu.edu/smallsat/2022/all2022/133/")
    print()
    print("Usage:")
    print(f"  {prog_name} <input> <packet_size> <pt> <ft> <rt> <robustness>")
    print(f"  {prog_name} -d <input.pkt> <packet_size> <robustness>")
    print()
    print("Options:")
    print("  -d             Decompress (default is compress)")
    print("  -h, --help     Show this help message")
    print("  -v, --version  Show version information")
    print()
    print("Compress arguments:")
    print("  input          Input file to compress")
    print("  packet_size    Packet size in bytes (e.g., 90)")
    print("  pt             New mask period (e.g., 10, 20)")
    print("  ft             Send mask period (e.g., 20, 50)")
    print("  rt             Uncompressed period (e.g., 50, 100)")
    print("  robustness     Robustness level 0-7 (e.g., 1, 2)")
    print()
    print("Decompress arguments:")
    print("  input.pkt      Compressed input file")
    print("  packet_size    Original packet size in bytes")
    print("  robustness     Robustness level (must match compression)")
    print()
    print("Output:")
    print("  Compress:   <input>.pkt")
    print("  Decompress: <input>.depkt (or <base>.depkt if input ends in .pkt)")
    print()
    print("Examples:")
    print(f"  {prog_name} data.bin 90 10 20 50 1        # compress")
    print(f"  {prog_name} -d data.bin.pkt 90 1          # decompress")
    print()


def make_decompress_filename(input_path: str) -> str:
    """Create output filename for decompression.

    Removes .pkt extension if present, then appends .depkt.
    """
    if input_path.endswith(".pkt"):
        return input_path[:-4] + ".depkt"
    return input_path + ".depkt"


def do_compress(
    input_path: str,
    packet_size: int,
    pt_period: int,
    ft_period: int,
    rt_period: int,
    robustness: int,
) -> int:
    """Compress a file.

    Args:
        input_path: Input file path.
        packet_size: Packet size in bytes.
        pt_period: New mask period.
        ft_period: Send mask period.
        rt_period: Uncompressed period.
        robustness: Robustness level (0-7).

    Returns:
        0 on success, 1 on error.
    """
    # Read input file
    try:
        with open(input_path, "rb") as f:
            input_data = f.read()
    except OSError as e:
        print(f"Error: Cannot open input file: {input_path} ({e})", file=sys.stderr)
        return 1

    if len(input_data) == 0:
        print("Error: Input file is empty", file=sys.stderr)
        return 1

    if len(input_data) % packet_size != 0:
        print(
            f"Error: Input size ({len(input_data)}) not divisible by "
            f"packet size ({packet_size})",
            file=sys.stderr,
        )
        return 1

    # Create output filename
    output_path = f"{input_path}.pkt"

    # Compress
    packet_length_bits = packet_size * 8
    try:
        output_data = compress(
            input_data,
            packet_size=packet_length_bits,
            robustness=robustness,
            pt_limit=pt_period,
            ft_limit=ft_period,
            rt_limit=rt_period,
        )
    except Exception as e:
        print(f"Error: Compression failed: {e}", file=sys.stderr)
        return 1

    # Write output
    try:
        with open(output_path, "wb") as f:
            f.write(output_data)
    except OSError as e:
        print(f"Error: Cannot write output file: {output_path} ({e})", file=sys.stderr)
        return 1

    # Print summary
    num_packets = len(input_data) // packet_size
    ratio = len(input_data) / len(output_data) if len(output_data) > 0 else 0
    print(f"Input:       {input_path} ({len(input_data)} bytes, {num_packets} packets)")
    print(f"Output:      {output_path} ({len(output_data)} bytes)")
    print(f"Ratio:       {ratio:.2f}x")
    print(
        f"Parameters:  R={robustness}, pt={pt_period}, ft={ft_period}, rt={rt_period}"
    )

    return 0


def do_decompress(input_path: str, packet_size: int, robustness: int) -> int:
    """Decompress a file.

    Args:
        input_path: Compressed input file path.
        packet_size: Original packet size in bytes.
        robustness: Robustness level (0-7).

    Returns:
        0 on success, 1 on error.
    """
    # Read input file
    try:
        with open(input_path, "rb") as f:
            input_data = f.read()
    except OSError as e:
        print(f"Error: Cannot open input file: {input_path} ({e})", file=sys.stderr)
        return 1

    if len(input_data) == 0:
        print("Error: Input file is empty", file=sys.stderr)
        return 1

    # Create output filename
    output_path = make_decompress_filename(input_path)

    # Decompress
    packet_length_bits = packet_size * 8
    try:
        output_data = decompress(
            input_data,
            packet_size=packet_length_bits,
            robustness=robustness,
        )
    except Exception as e:
        print(f"Error: Decompression failed: {e}", file=sys.stderr)
        return 1

    # Write output
    try:
        with open(output_path, "wb") as f:
            f.write(output_data)
    except OSError as e:
        print(f"Error: Cannot write output file: {output_path} ({e})", file=sys.stderr)
        return 1

    # Print summary
    num_packets = len(output_data) // packet_size
    expansion = len(output_data) / len(input_data) if len(input_data) > 0 else 0
    print(f"Input:       {input_path} ({len(input_data)} bytes)")
    print(
        f"Output:      {output_path} ({len(output_data)} bytes, {num_packets} packets)"
    )
    print(f"Expansion:   {expansion:.2f}x")
    print(f"Parameters:  packet_size={packet_size}, R={robustness}")

    return 0


def main() -> int:
    """CLI entry point."""
    args = sys.argv
    prog_name = args[0] if args else "cli.py"

    # Check for help flag or no arguments
    if len(args) < 2:
        print_help(prog_name)
        return 1

    if args[1] in ("-h", "--help"):
        print_help(prog_name)
        return 0

    if args[1] in ("-v", "--version"):
        print_version()
        return 0

    # Check for decompress flag
    decompress_mode = args[1] == "-d"
    arg_offset = 2 if decompress_mode else 1

    if decompress_mode:
        # Decompress mode: -d <input.pkt> <packet_size> <robustness>
        if len(args) != 5:
            print("Error: Decompress requires 3 arguments after -d", file=sys.stderr)
            print(
                f"Usage: {prog_name} -d <input.pkt> <packet_size> <robustness>",
                file=sys.stderr,
            )
            return 1

        input_path = args[arg_offset]

        try:
            packet_size = int(args[arg_offset + 1])
        except ValueError:
            print("Error: packet_size must be an integer", file=sys.stderr)
            return 1

        try:
            robustness = int(args[arg_offset + 2])
        except ValueError:
            print("Error: robustness must be an integer", file=sys.stderr)
            return 1

        # Validate parameters
        if packet_size <= 0 or packet_size > 8192:
            print("Error: packet_size must be 1-8192 bytes", file=sys.stderr)
            return 1

        if robustness < 0 or robustness > 7:
            print("Error: robustness must be 0-7", file=sys.stderr)
            return 1

        return do_decompress(input_path, packet_size, robustness)

    else:
        # Compress mode: <input> <packet_size> <pt> <ft> <rt> <robustness>
        if len(args) != 7:
            print("Error: Compress requires 6 arguments", file=sys.stderr)
            print(
                f"Usage: {prog_name} <input> <packet_size> <pt> <ft> <rt> <robustness>",
                file=sys.stderr,
            )
            return 1

        input_path = args[1]

        try:
            packet_size = int(args[2])
            pt_period = int(args[3])
            ft_period = int(args[4])
            rt_period = int(args[5])
            robustness = int(args[6])
        except ValueError:
            print("Error: All numeric arguments must be integers", file=sys.stderr)
            return 1

        # Validate parameters
        if packet_size <= 0 or packet_size > 8192:
            print("Error: packet_size must be 1-8192 bytes", file=sys.stderr)
            return 1

        if robustness < 0 or robustness > 7:
            print("Error: robustness must be 0-7", file=sys.stderr)
            return 1

        if pt_period <= 0 or ft_period <= 0 or rt_period <= 0:
            print("Error: periods must be positive", file=sys.stderr)
            return 1

        return do_compress(
            input_path, packet_size, pt_period, ft_period, rt_period, robustness
        )


if __name__ == "__main__":
    sys.exit(main())
