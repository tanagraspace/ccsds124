"""Stub test to verify CI pipeline works."""

import ccsds124


def test_version() -> None:
    """Test that version is defined."""
    assert ccsds124.__version__ == "1.0.0"


def test_compress_works() -> None:
    """Test that compress produces output."""
    # 8 bytes of data, packet size = 16 bits (2 bytes)
    result = ccsds124.compress(b"\x00\x00\x00\x00\x00\x00\x00\x00", packet_size=16)
    assert len(result) > 0


def test_decompress_works() -> None:
    """Test that decompress round-trips with compress."""
    original = b"\x00\x00\x00\x00\x00\x00\x00\x00"
    compressed = ccsds124.compress(original, packet_size=16)
    decompressed = ccsds124.decompress(compressed, packet_size=16)
    assert decompressed == original
