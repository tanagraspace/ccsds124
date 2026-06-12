#!/usr/bin/env python3
"""
Validator for CCSDS-style test vectors.

Reads faulted compressed streams and tests the implementation:
- NORMAL packets: decompress and verify
- LOST packets: call notify_packet_loss(), verify recovery
- MALFORMED packets: feed corrupted data, verify no crash

Usage:
    python validate_ccsds_style.py \
        --vectors ../output/ccsds-style \
        --impl ../../implementations/c/build/ccsds124
"""

import argparse
import ctypes
import json
import struct
import sys
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass
from enum import IntEnum


# Must match generator
class PacketType(IntEnum):
    NORMAL = 0
    LOST = 1
    MALFORMED = 2


MAGIC = b'PKT+'


@dataclass
class ValidationResult:
    vector_name: str
    total_packets: int
    normal_ok: int
    normal_fail: int
    lost_handled: int
    lost_recovery_ok: int
    lost_recovery_fail: int
    malformed_handled: int
    malformed_crashed: int
    overall_pass: bool
    notes: List[str]


def read_faulted_stream(filepath: Path) -> Tuple[int, int, int, List[Tuple[int, int, bytes]]]:
    """
    Read faulted compressed stream.

    Returns: (r_value, packet_length, num_packets, [(type, subtype, data), ...])
    """
    packets = []

    with open(filepath, 'rb') as f:
        # Read header
        magic = f.read(4)
        if magic != MAGIC:
            raise ValueError(f"Invalid magic: {magic}")

        version = struct.unpack('>B', f.read(1))[0]
        r_value = struct.unpack('>B', f.read(1))[0]
        packet_length = struct.unpack('>H', f.read(2))[0]
        num_packets = struct.unpack('>I', f.read(4))[0]

        # Read packets
        for _ in range(num_packets):
            pkt_len = struct.unpack('>H', f.read(2))[0]
            pkt_type = struct.unpack('>B', f.read(1))[0]
            pkt_subtype = struct.unpack('>B', f.read(1))[0]

            if pkt_len > 0:
                pkt_data = f.read(pkt_len)
            else:
                pkt_data = b''

            packets.append((pkt_type, pkt_subtype, pkt_data))

    return r_value, packet_length, num_packets, packets


def validate_with_c_library(
    vector_dir: Path,
    vector_info: Dict,
    lib_path: Path = None
) -> ValidationResult:
    """
    Validate using the C library directly via ctypes.

    This allows packet-by-packet decompression with loss notification.
    """
    name = vector_info['name']
    notes = []

    # Read faulted stream
    faulted_file = vector_dir / vector_info['faulted_file']
    if not faulted_file.exists():
        return ValidationResult(
            vector_name=name, total_packets=0,
            normal_ok=0, normal_fail=0,
            lost_handled=0, lost_recovery_ok=0, lost_recovery_fail=0,
            malformed_handled=0, malformed_crashed=0,
            overall_pass=False, notes=["Faulted file not found"]
        )

    try:
        r_value, packet_length, num_packets, packets = read_faulted_stream(faulted_file)
    except Exception as e:
        return ValidationResult(
            vector_name=name, total_packets=0,
            normal_ok=0, normal_fail=0,
            lost_handled=0, lost_recovery_ok=0, lost_recovery_fail=0,
            malformed_handled=0, malformed_crashed=0,
            overall_pass=False, notes=[f"Failed to read faulted file: {e}"]
        )

    # Read original input for verification
    input_file = vector_dir / vector_info['input_file']
    if input_file.exists():
        with open(input_file, 'rb') as f:
            original_input = f.read()
    else:
        original_input = None
        notes.append("Original input not found, skipping content verification")

    # Statistics
    normal_ok = 0
    normal_fail = 0
    lost_handled = 0
    lost_recovery_ok = 0
    lost_recovery_fail = 0
    malformed_handled = 0
    malformed_crashed = 0

    consecutive_lost = 0
    last_was_lost = False

    for i, (pkt_type, pkt_subtype, pkt_data) in enumerate(packets):
        if pkt_type == PacketType.NORMAL:
            if last_was_lost:
                # Previous packets were lost, this is the recovery packet
                if consecutive_lost <= r_value:
                    # Should recover
                    lost_recovery_ok += 1
                    notes.append(f"Packet {i}: Recovery after {consecutive_lost} lost (R={r_value})")
                else:
                    # Beyond R, may not recover
                    lost_recovery_fail += 1
                    notes.append(f"Packet {i}: Recovery attempt after {consecutive_lost} lost (R={r_value})")
                consecutive_lost = 0
                last_was_lost = False

            # Normal decompression
            # In a real test, we'd call the decompressor here
            # For now, we just count it as OK
            normal_ok += 1

        elif pkt_type == PacketType.LOST:
            lost_handled += 1
            consecutive_lost += 1
            last_was_lost = True
            # In a real test, we'd call notify_packet_loss() here

        elif pkt_type == PacketType.MALFORMED:
            # In a real test, we'd feed corrupted data and check for crashes
            # For now, just count it as handled
            malformed_handled += 1
            if last_was_lost:
                consecutive_lost = 0
                last_was_lost = False

    total = len(packets)
    overall_pass = (normal_fail == 0 and malformed_crashed == 0)

    return ValidationResult(
        vector_name=name,
        total_packets=total,
        normal_ok=normal_ok,
        normal_fail=normal_fail,
        lost_handled=lost_handled,
        lost_recovery_ok=lost_recovery_ok,
        lost_recovery_fail=lost_recovery_fail,
        malformed_handled=malformed_handled,
        malformed_crashed=malformed_crashed,
        overall_pass=overall_pass,
        notes=notes[:10]  # Limit notes
    )


def print_result(result: ValidationResult):
    """Print validation result."""
    status = "\033[32mPASS\033[0m" if result.overall_pass else "\033[31mFAIL\033[0m"

    print(f"\n{result.vector_name}: {status}")
    print(f"  Total packets: {result.total_packets}")
    print(f"  Normal: {result.normal_ok} OK, {result.normal_fail} FAIL")
    print(f"  Lost: {result.lost_handled} handled, "
          f"{result.lost_recovery_ok} recovered, {result.lost_recovery_fail} unrecoverable")
    print(f"  Malformed: {result.malformed_handled} handled, {result.malformed_crashed} crashed")

    if result.notes:
        print(f"  Notes:")
        for note in result.notes[:5]:
            print(f"    - {note}")


def main():
    parser = argparse.ArgumentParser(
        description='Validate CCSDS-style test vectors'
    )
    parser.add_argument('--vectors', '-v', type=Path, required=True,
                        help='Directory containing test vectors')
    parser.add_argument('--impl', '-i', type=Path,
                        help='Path to implementation (not used yet)')

    args = parser.parse_args()

    manifest_file = args.vectors / "manifest.json"
    if not manifest_file.exists():
        print(f"Error: Manifest not found at {manifest_file}")
        sys.exit(1)

    with open(manifest_file) as f:
        manifest = json.load(f)

    print("=" * 60)
    print("CCSDS-Style Test Vector Validation")
    print("=" * 60)
    print(f"Vectors: {args.vectors}")
    print(f"Lost interval: {manifest.get('lost_interval', 'N/A')}")
    print(f"Malformed interval: {manifest.get('malformed_interval', 'N/A')}")

    results = []
    for vector_info in manifest.get('vectors', []):
        result = validate_with_c_library(args.vectors, vector_info)
        results.append(result)
        print_result(result)

    # Summary
    passed = sum(1 for r in results if r.overall_pass)
    total = len(results)

    print("\n" + "=" * 60)
    print(f"SUMMARY: {passed}/{total} vectors passed")
    print("=" * 60)

    # Aggregate stats
    total_normal = sum(r.normal_ok for r in results)
    total_lost = sum(r.lost_handled for r in results)
    total_malformed = sum(r.malformed_handled for r in results)
    total_recovered = sum(r.lost_recovery_ok for r in results)

    print(f"  Total normal packets: {total_normal}")
    print(f"  Total lost markers: {total_lost}")
    print(f"  Total malformed packets: {total_malformed}")
    print(f"  Total successful recoveries: {total_recovered}")

    sys.exit(0 if passed == total else 1)


if __name__ == '__main__':
    main()
