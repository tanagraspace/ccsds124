#!/usr/bin/env python3
"""
Unified CCSDS 124.0-B-1 test vector generator.

Generates test vectors with optional fault injection into the compressed stream:
- --inject-lost: Remove packets to simulate transmission loss
- --inject-malformed: Corrupt packets to test error handling

Examples:
    # Clean test vectors (no faults)
    python generate.py --output-dir ./vectors --size 100MB

    # With lost packet markers (test R parameter recovery)
    python generate.py --output-dir ./vectors --size 100MB \
        --inject-lost --lost-frequency 50

    # With malformed packets (test error handling)
    python generate.py --output-dir ./vectors --size 100MB \
        --inject-malformed --malformed-frequency 100

    # Full CCSDS-style validation (both fault types)
    python generate.py --output-dir ./vectors --size 1GB \
        --inject-lost --lost-frequency 50 \
        --inject-malformed --malformed-frequency 100 \
        --seed 42

    # Specific R value and packet size
    python generate.py --output-dir ./vectors --size 100MB \
        --r-values 0,1,2,3,4,5,6,7 \
        --packet-sizes 90,180 \
        --patterns housekeeping,predictable
"""

import argparse
import json
import struct
import subprocess
import sys
import hashlib
import numpy as np
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass, asdict, field
from enum import IntEnum
from common import set_deterministic_seed, calculate_md5


# Packet type markers
class PacketType(IntEnum):
    NORMAL = 0
    LOST = 1
    MALFORMED = 2


# Malformed subtypes
class MalformedType(IntEnum):
    NONE = 0
    TRUNCATED = 1
    BIT_FLIPS = 2
    ZEROS = 3
    ONES = 4
    RANDOM = 5


# File format
MAGIC = b'PKT+'
VERSION = 1


@dataclass
class PacketInfo:
    index: int
    type: int
    subtype: int
    offset: int
    original_length: int
    stored_length: int


@dataclass
class TestVectorInfo:
    name: str
    r_value: int
    packet_length_bytes: int
    num_packets: int
    pattern: str
    seed: int

    input_file: str
    input_md5: str
    input_size: int

    compressed_file: str
    compressed_md5: str
    compressed_size: int

    # Only present if faults injected
    faulted_file: str = ""
    faulted_size: int = 0

    # Injection settings
    inject_lost: bool = False
    lost_frequency: int = 0
    inject_malformed: bool = False
    malformed_frequency: int = 0

    # Statistics
    num_normal: int = 0
    num_lost: int = 0
    num_malformed: int = 0

    packets: List[Dict] = field(default_factory=list)


def generate_input_data(pattern: str, num_packets: int, packet_length: int, seed: int) -> bytes:
    """Generate input data based on pattern type."""
    np.random.seed(seed)
    data = bytearray()

    if pattern == "housekeeping":
        temps = np.random.uniform(20.0, 30.0, 4)
        volts = np.random.uniform(3.2, 3.4, 4)

        for pkt_num in range(num_packets):
            packet = bytearray(packet_length)
            pos = 0

            # Packet counter
            if pos + 2 <= packet_length:
                struct.pack_into('>H', packet, pos, pkt_num & 0xFFFF)
                pos += 2

            # Timestamp
            if pos + 4 <= packet_length:
                struct.pack_into('>I', packet, pos, (pkt_num * 1000) & 0xFFFFFFFF)
                pos += 4

            # Temperatures with drift
            for i in range(4):
                if pos + 2 > packet_length:
                    break
                temps[i] += np.random.uniform(-0.1, 0.1)
                temps[i] = np.clip(temps[i], 15.0, 40.0)
                struct.pack_into('>H', packet, pos, int(temps[i] * 100) & 0xFFFF)
                pos += 2

            # Voltages with drift
            for i in range(4):
                if pos + 2 > packet_length:
                    break
                volts[i] += np.random.uniform(-0.01, 0.01)
                volts[i] = np.clip(volts[i], 3.0, 3.6)
                struct.pack_into('>H', packet, pos, int(volts[i] * 1000) & 0xFFFF)
                pos += 2

            # Fill rest
            while pos < packet_length:
                packet[pos] = 0x00 if np.random.random() > 0.01 else np.random.randint(0, 256)
                pos += 1

            data.extend(packet)

    elif pattern == "predictable":
        for pkt_num in range(num_packets):
            packet = bytearray(packet_length)
            packet[0] = pkt_num & 0xFF
            packet[1] = (pkt_num >> 8) & 0xFF
            for i in range(2, packet_length):
                packet[i] = 0x55
            data.extend(packet)

    elif pattern == "entropy":
        for pkt_num in range(num_packets):
            np.random.seed(seed + pkt_num)
            packet = bytes(np.random.randint(0, 256, packet_length, dtype=np.uint8))
            data.extend(packet)

    elif pattern == "edge":
        patterns = [0x00, 0xFF, 0xAA, 0x55, 0x0F, 0xF0]
        for pkt_num in range(num_packets):
            val = patterns[pkt_num % len(patterns)]
            packet = bytes([val] * packet_length)
            data.extend(packet)

    else:
        # Default: zeros
        data = bytearray(num_packets * packet_length)

    return bytes(data)


def compress_with_impl(impl_path: Path, input_file: Path, packet_length: int,
                       r_value: int, pt: int = 10, ft: int = 20, rt: int = 50) -> Optional[Path]:
    """Compress using implementation CLI."""
    try:
        subprocess.run(
            [str(impl_path), str(input_file), str(packet_length),
             str(pt), str(ft), str(rt), str(r_value)],
            cwd=input_file.parent,
            capture_output=True,
            timeout=600
        )
        compressed = Path(str(input_file) + ".pkt")
        return compressed if compressed.exists() else None
    except Exception as e:
        print(f"    Compression error: {e}")
        return None


def parse_compressed_packets(data: bytes, num_packets: int) -> List[bytes]:
    """Parse compressed data into approximate packets."""
    if num_packets <= 0:
        return []

    # Distribute bytes evenly, with remainder spread across first packets
    base_size = len(data) // num_packets
    remainder = len(data) % num_packets

    packets = []
    offset = 0

    for i in range(num_packets):
        # Add 1 extra byte to first 'remainder' packets
        pkt_size = base_size + (1 if i < remainder else 0)
        packets.append(data[offset:offset + pkt_size])
        offset += pkt_size

    return packets


def corrupt_packet(data: bytes, mtype: MalformedType, seed: int) -> bytes:
    """Corrupt a packet according to malformed type."""
    np.random.seed(seed)
    arr = bytearray(data)

    if mtype == MalformedType.TRUNCATED:
        return bytes(arr[:len(arr)//2])
    elif mtype == MalformedType.BIT_FLIPS:
        for _ in range(max(1, len(arr) // 8)):
            if arr:
                pos = np.random.randint(0, len(arr))
                arr[pos] ^= (1 << np.random.randint(0, 8))
        return bytes(arr)
    elif mtype == MalformedType.ZEROS:
        return bytes(len(arr))
    elif mtype == MalformedType.ONES:
        return bytes([0xFF] * len(arr))
    elif mtype == MalformedType.RANDOM:
        return bytes(np.random.randint(0, 256, len(arr), dtype=np.uint8))

    return bytes(arr)


def write_faulted_stream(
    output_file: Path,
    packets: List[bytes],
    r_value: int,
    packet_length: int,
    inject_lost: bool,
    lost_frequency: int,
    inject_malformed: bool,
    malformed_frequency: int,
    seed: int
) -> Tuple[List[PacketInfo], int, int, int]:
    """Write compressed stream with optional fault injection."""
    infos = []
    num_normal = 0
    num_lost = 0
    num_malformed = 0
    malform_idx = 0
    malform_types = [MalformedType.TRUNCATED, MalformedType.BIT_FLIPS,
                     MalformedType.ZEROS, MalformedType.RANDOM]

    with open(output_file, 'wb') as f:
        # Header
        f.write(MAGIC)
        f.write(struct.pack('>B', VERSION))
        f.write(struct.pack('>B', r_value))
        f.write(struct.pack('>H', packet_length))
        f.write(struct.pack('>I', len(packets)))

        for i, pkt_data in enumerate(packets):
            offset = f.tell()
            orig_len = len(pkt_data)

            # Check malformed first (priority)
            if inject_malformed and malformed_frequency > 0 and i > 0 and i % malformed_frequency == 0:
                mtype = malform_types[malform_idx % len(malform_types)]
                malform_idx += 1
                corrupted = corrupt_packet(pkt_data, mtype, seed + i)

                f.write(struct.pack('>H', len(corrupted)))
                f.write(struct.pack('>B', PacketType.MALFORMED))
                f.write(struct.pack('>B', mtype))
                f.write(corrupted)

                infos.append(PacketInfo(i, PacketType.MALFORMED, mtype, offset, orig_len, len(corrupted)))
                num_malformed += 1

            # Check lost
            elif inject_lost and lost_frequency > 0 and i > 0 and i % lost_frequency == 0:
                f.write(struct.pack('>H', 0))
                f.write(struct.pack('>B', PacketType.LOST))
                f.write(struct.pack('>B', 0))

                infos.append(PacketInfo(i, PacketType.LOST, 0, offset, orig_len, 0))
                num_lost += 1

            # Normal
            else:
                f.write(struct.pack('>H', len(pkt_data)))
                f.write(struct.pack('>B', PacketType.NORMAL))
                f.write(struct.pack('>B', 0))
                f.write(pkt_data)

                infos.append(PacketInfo(i, PacketType.NORMAL, 0, offset, orig_len, len(pkt_data)))
                num_normal += 1

    return infos, num_normal, num_lost, num_malformed


def generate_vector(
    output_dir: Path,
    name: str,
    r_value: int,
    packet_length: int,
    num_packets: int,
    pattern: str,
    impl_path: Path,
    seed: int,
    inject_lost: bool = False,
    lost_frequency: int = 50,
    inject_malformed: bool = False,
    malformed_frequency: int = 100
) -> Optional[TestVectorInfo]:
    """Generate a single test vector."""
    print(f"\n  {name}: R={r_value}, {num_packets} packets...")

    # Generate input
    input_file = output_dir / f"{name}_input.bin"
    input_data = generate_input_data(pattern, num_packets, packet_length, seed)
    with open(input_file, 'wb') as f:
        f.write(input_data)
    input_md5 = calculate_md5(input_data)

    # Compress
    compressed_file = compress_with_impl(impl_path, input_file, packet_length, r_value)
    if not compressed_file:
        print(f"    ERROR: Compression failed")
        return None

    with open(compressed_file, 'rb') as f:
        compressed_data = f.read()
    compressed_md5 = calculate_md5(compressed_data)

    # Rename compressed file
    clean_file = output_dir / f"{name}_compressed.pkt"
    compressed_file.rename(clean_file)

    ratio = len(input_data) / len(compressed_data) if compressed_data else 0
    print(f"    Compressed: {len(compressed_data):,} bytes (ratio: {ratio:.2f}x)")

    info = TestVectorInfo(
        name=name,
        r_value=r_value,
        packet_length_bytes=packet_length,
        num_packets=num_packets,
        pattern=pattern,
        seed=seed,
        input_file=input_file.name,
        input_md5=input_md5,
        input_size=len(input_data),
        compressed_file=clean_file.name,
        compressed_md5=compressed_md5,
        compressed_size=len(compressed_data),
        inject_lost=inject_lost,
        lost_frequency=lost_frequency if inject_lost else 0,
        inject_malformed=inject_malformed,
        malformed_frequency=malformed_frequency if inject_malformed else 0,
    )

    # Inject faults if requested
    if inject_lost or inject_malformed:
        packets = parse_compressed_packets(compressed_data, num_packets)
        faulted_file = output_dir / f"{name}_faulted.pkt"

        infos, num_normal, num_lost, num_malformed = write_faulted_stream(
            faulted_file, packets, r_value, packet_length,
            inject_lost, lost_frequency,
            inject_malformed, malformed_frequency,
            seed
        )

        info.faulted_file = faulted_file.name
        info.faulted_size = faulted_file.stat().st_size
        info.num_normal = num_normal
        info.num_lost = num_lost
        info.num_malformed = num_malformed
        info.packets = [asdict(p) for p in infos]

        print(f"    Faulted: {num_normal} normal, {num_lost} lost, {num_malformed} malformed")
    else:
        info.num_normal = num_packets

    # Save metadata
    meta_file = output_dir / f"{name}_metadata.json"
    with open(meta_file, 'w') as f:
        # Don't include full packet list in summary (too large)
        meta_dict = asdict(info)
        if len(meta_dict.get('packets', [])) > 100:
            meta_dict['packets'] = f"[{len(info.packets)} packets - see full file]"
        json.dump(meta_dict, f, indent=2)

    return info


def parse_size(s: str) -> int:
    """Parse size string like '1GB' to bytes."""
    s = s.upper().strip()
    for suffix, mult in [('GB', 1024**3), ('MB', 1024**2), ('KB', 1024), ('B', 1)]:
        if s.endswith(suffix):
            return int(float(s[:-len(suffix)]) * mult)
    return int(s)


def parse_list(s: str, type_fn=str) -> List:
    """Parse comma-separated list."""
    if not s:
        return []
    return [type_fn(x.strip()) for x in s.split(',')]


def main():
    parser = argparse.ArgumentParser(
        description='Generate CCSDS 124.0-B-1 test vectors with optional fault injection',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Clean vectors (no faults)
  python generate.py -o ./vectors -s 100MB

  # With lost packet injection
  python generate.py -o ./vectors -s 100MB --inject-lost --lost-frequency 50

  # With malformed packet injection
  python generate.py -o ./vectors -s 100MB --inject-malformed --malformed-frequency 100

  # Full CCSDS-style (both)
  python generate.py -o ./vectors -s 1GB --inject-lost --inject-malformed --seed 42
"""
    )

    # Output
    parser.add_argument('--output-dir', '-o', type=Path, default=Path('./output'),
                        help='Output directory')
    parser.add_argument('--size', '-s', type=str, default='10MB',
                        help='Target total size (e.g., 10MB, 1GB)')

    # Test parameters
    parser.add_argument('--r-values', '-r', type=str, default='0,1,2,3,4,5,6,7',
                        help='Comma-separated R values (default: 0-7)')
    parser.add_argument('--packet-sizes', '-p', type=str, default='90',
                        help='Comma-separated packet sizes in bytes (default: 90)')
    parser.add_argument('--patterns', type=str, default='housekeeping',
                        help='Comma-separated patterns: housekeeping,predictable,entropy,edge')

    # Implementation
    parser.add_argument('--impl', '-i', type=Path,
                        help='Path to ccsds124 CLI (auto-detected if not specified)')

    # Fault injection
    parser.add_argument('--inject-lost', action='store_true',
                        help='Inject LOST packet markers (simulates transmission loss)')
    parser.add_argument('--lost-frequency', type=int, default=50,
                        help='Inject LOST marker every N packets (default: 50)')

    parser.add_argument('--inject-malformed', action='store_true',
                        help='Inject MALFORMED packets (simulates corruption)')
    parser.add_argument('--malformed-frequency', type=int, default=100,
                        help='Inject MALFORMED packet every N packets (default: 100)')

    # Reproducibility
    parser.add_argument('--seed', type=int, default=42,
                        help='Random seed for reproducibility (default: 42)')

    args = parser.parse_args()

    # Find implementation
    if args.impl:
        impl_path = args.impl
    else:
        # Try to find it
        candidates = [
            Path(__file__).parent.parent.parent / 'implementations/c/build/ccsds124',
            Path.cwd() / 'implementations/c/build/ccsds124',
            Path.home() / 'ccsds124/implementations/c/build/ccsds124',
        ]
        impl_path = None
        for c in candidates:
            if c.exists():
                impl_path = c
                break

    if not impl_path or not impl_path.exists():
        print("Error: ccsds124 CLI not found")
        print("Build with: cd implementations/c && make")
        print("Or specify with: --impl /path/to/ccsds124")
        sys.exit(1)

    # Parse parameters
    r_values = parse_list(args.r_values, int)
    packet_sizes = parse_list(args.packet_sizes, int)
    patterns = parse_list(args.patterns)
    target_size = parse_size(args.size)

    args.output_dir.mkdir(parents=True, exist_ok=True)

    # Print config
    print("=" * 60)
    print("CCSDS 124.0-B-1 Test Vector Generator")
    print("=" * 60)
    print(f"Output:        {args.output_dir}")
    print(f"Target size:   {target_size:,} bytes ({target_size/1024/1024:.1f} MB)")
    print(f"R values:      {r_values}")
    print(f"Packet sizes:  {packet_sizes}")
    print(f"Patterns:      {patterns}")
    print(f"Seed:          {args.seed}")
    print(f"Implementation: {impl_path}")

    if args.inject_lost or args.inject_malformed:
        print(f"\nFault Injection:")
        if args.inject_lost:
            print(f"  LOST:      every {args.lost_frequency} packets")
        if args.inject_malformed:
            print(f"  MALFORMED: every {args.malformed_frequency} packets")

    # Calculate packets per vector
    num_combos = len(r_values) * len(packet_sizes) * len(patterns)
    size_per_vector = target_size // max(1, num_combos)

    all_infos = []

    print(f"\nGenerating {num_combos} test vectors...")

    for r in r_values:
        for pkt_size in packet_sizes:
            for pattern in patterns:
                num_packets = max(200, size_per_vector // pkt_size)
                name = f"R{r}_F{pkt_size*8}_{pattern}"

                info = generate_vector(
                    output_dir=args.output_dir,
                    name=name,
                    r_value=r,
                    packet_length=pkt_size,
                    num_packets=num_packets,
                    pattern=pattern,
                    impl_path=impl_path,
                    seed=args.seed + r + pkt_size,
                    inject_lost=args.inject_lost,
                    lost_frequency=args.lost_frequency,
                    inject_malformed=args.inject_malformed,
                    malformed_frequency=args.malformed_frequency
                )

                if info:
                    all_infos.append(info)

    # Write manifest
    manifest = {
        "generator": "generate.py",
        "version": VERSION,
        "seed": args.seed,
        "inject_lost": args.inject_lost,
        "lost_frequency": args.lost_frequency if args.inject_lost else None,
        "inject_malformed": args.inject_malformed,
        "malformed_frequency": args.malformed_frequency if args.inject_malformed else None,
        "vectors": [
            {
                "name": i.name,
                "r_value": i.r_value,
                "packet_length": i.packet_length_bytes,
                "num_packets": i.num_packets,
                "pattern": i.pattern,
                "input_file": i.input_file,
                "compressed_file": i.compressed_file,
                "faulted_file": i.faulted_file if i.faulted_file else None,
                "num_normal": i.num_normal,
                "num_lost": i.num_lost,
                "num_malformed": i.num_malformed,
            }
            for i in all_infos
        ]
    }

    manifest_file = args.output_dir / "manifest.json"
    with open(manifest_file, 'w') as f:
        json.dump(manifest, f, indent=2)

    # Summary
    total_input = sum(i.input_size for i in all_infos)
    total_compressed = sum(i.compressed_size for i in all_infos)
    total_lost = sum(i.num_lost for i in all_infos)
    total_malformed = sum(i.num_malformed for i in all_infos)

    print("\n" + "=" * 60)
    print(f"Generated {len(all_infos)} test vectors")
    print(f"Total input:      {total_input:,} bytes ({total_input/1024/1024:.1f} MB)")
    print(f"Total compressed: {total_compressed:,} bytes ({total_compressed/1024/1024:.1f} MB)")
    if args.inject_lost:
        print(f"Total LOST:       {total_lost} packets")
    if args.inject_malformed:
        print(f"Total MALFORMED:  {total_malformed} packets")
    print(f"Manifest: {manifest_file}")
    print("=" * 60)


if __name__ == '__main__':
    main()
