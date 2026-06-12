"""Tests for decoding functions (COUNT, RLE, bit insert)."""

import pytest

from ccsds124.bitbuffer import BitBuffer
from ccsds124.bitreader import BitReader
from ccsds124.bitvector import BitVector
from ccsds124.decode import bit_insert, bit_insert_forward, count_decode, rle_decode
from ccsds124.encode import bit_extract, bit_extract_forward, count_encode, rle_encode


class TestCountDecode:
    """Test COUNT decoding."""

    def test_count_decode_1(self) -> None:
        """Test decoding '0' produces 1."""
        reader = BitReader(bytes([0b00000000]))
        value = count_decode(reader)
        assert value == 1
        assert reader.position == 1

    def test_count_decode_terminator(self) -> None:
        """Test decoding '10' produces 0 (terminator)."""
        reader = BitReader(bytes([0b10000000]))
        value = count_decode(reader)
        assert value == 0
        assert reader.position == 2

    def test_count_decode_2(self) -> None:
        """Test decoding '110' + BIT5(0) produces 2."""
        reader = BitReader(bytes([0b11000000]))
        value = count_decode(reader)
        assert value == 2
        assert reader.position == 8

    def test_count_decode_33(self) -> None:
        """Test decoding '110' + BIT5(31) produces 33."""
        reader = BitReader(bytes([0b11011111]))
        value = count_decode(reader)
        assert value == 33
        assert reader.position == 8

    def test_count_decode_34(self) -> None:
        """Test decoding extended format for 34."""
        # First encode it
        bb = BitBuffer()
        count_encode(bb, 34)
        encoded = bb.to_bytes()

        # Then decode it
        reader = BitReader(encoded)
        value = count_decode(reader)
        assert value == 34

    def test_count_round_trip(self) -> None:
        """Test encode/decode round trip for various values."""
        test_values = [1, 2, 10, 33, 34, 100, 500, 1000, 10000]
        for original in test_values:
            bb = BitBuffer()
            count_encode(bb, original)
            encoded = bb.to_bytes()

            reader = BitReader(encoded)
            decoded = count_decode(reader)
            assert decoded == original, f"Failed for {original}"


class TestRleDecode:
    """Test RLE decoding."""

    def test_rle_decode_all_zeros(self) -> None:
        """Test RLE decode of all zeros (just terminator)."""
        # Encode all zeros
        bv = BitVector(8)
        bb = BitBuffer()
        rle_encode(bb, bv)

        # Decode
        reader = BitReader(bb.to_bytes())
        result = rle_decode(reader, 8)

        # Should be all zeros
        for i in range(8):
            assert result.get_bit(i) == 0

    def test_rle_decode_single_one(self) -> None:
        """Test RLE decode with single 1 at end."""
        bv = BitVector(8)
        bv.set_bit(7, 1)
        bb = BitBuffer()
        rle_encode(bb, bv)

        reader = BitReader(bb.to_bytes())
        result = rle_decode(reader, 8)

        assert result.get_bit(7) == 1
        for i in range(7):
            assert result.get_bit(i) == 0

    def test_rle_round_trip(self) -> None:
        """Test RLE encode/decode round trip."""
        original = BitVector(16)
        original.from_bytes(bytes([0b10110100, 0b01010101]))

        bb = BitBuffer()
        rle_encode(bb, original)

        reader = BitReader(bb.to_bytes())
        result = rle_decode(reader, 16)

        assert result.equals(original)

    def test_rle_round_trip_all_ones(self) -> None:
        """Test RLE with all ones."""
        original = BitVector(8)
        original.from_bytes(bytes([0xFF]))

        bb = BitBuffer()
        rle_encode(bb, original)

        reader = BitReader(bb.to_bytes())
        result = rle_decode(reader, 8)

        assert result.equals(original)


class TestBitInsert:
    """Test bit insertion (inverse of BE)."""

    def test_bit_insert_no_mask(self) -> None:
        """Test insertion with zero mask does nothing."""
        data = BitVector(8)
        mask = BitVector(8)

        reader = BitReader(b"")
        bit_insert(reader, data, mask)

        for i in range(8):
            assert data.get_bit(i) == 0

    def test_bit_insert_full_mask(self) -> None:
        """Test insertion with full mask."""
        data = BitVector(8)
        mask = BitVector(8)
        mask.from_bytes(bytes([0xFF]))

        # Create bits to insert (reversed for BE compatibility)
        original = bytes([0b10110100])
        bv_orig = BitVector(8)
        bv_orig.from_bytes(original)

        # Extract bits
        bb = BitBuffer()
        bit_extract(bb, bv_orig, mask)

        # Insert back
        reader = BitReader(bb.to_bytes())
        bit_insert(reader, data, mask)

        # Should match original
        assert data.to_bytes() == original

    def test_bit_insert_partial_mask(self) -> None:
        """Test insertion with partial mask."""
        data = BitVector(8)
        mask = BitVector(8)
        mask.from_bytes(bytes([0b10100000]))

        original = BitVector(8)
        original.from_bytes(bytes([0b11110000]))

        # Extract bits at mask positions
        bb = BitBuffer()
        bit_extract(bb, original, mask)

        # Insert back
        reader = BitReader(bb.to_bytes())
        bit_insert(reader, data, mask)

        # Check only masked positions
        assert data.get_bit(0) == original.get_bit(0)
        assert data.get_bit(2) == original.get_bit(2)


class TestRoundTrip:
    """Test full encode/decode round trips."""

    def test_count_rle_round_trip(self) -> None:
        """Test full RLE round trip with various patterns."""
        patterns = [
            bytes([0x00]),
            bytes([0xFF]),
            bytes([0xAA]),
            bytes([0x55]),
            bytes([0xDE, 0xAD]),
            bytes([0x12, 0x34, 0x56, 0x78]),
        ]

        for pattern in patterns:
            original = BitVector(len(pattern) * 8)
            original.from_bytes(pattern)

            # Encode
            bb = BitBuffer()
            rle_encode(bb, original)

            # Decode
            reader = BitReader(bb.to_bytes())
            result = rle_decode(reader, len(pattern) * 8)

            assert result.equals(original), f"Failed for pattern {pattern.hex()}"

    def test_bit_extract_insert_round_trip(self) -> None:
        """Test BE extract/insert round trip."""
        data = BitVector(16)
        data.from_bytes(bytes([0xCA, 0xFE]))

        mask = BitVector(16)
        mask.from_bytes(bytes([0xF0, 0x0F]))

        # Extract
        bb = BitBuffer()
        bit_extract(bb, data, mask)

        # Create new vector and insert
        result = BitVector(16)
        reader = BitReader(bb.to_bytes())
        bit_insert(reader, result, mask)

        # Check only masked positions match
        for i in range(16):
            if mask.get_bit(i):
                assert result.get_bit(i) == data.get_bit(i)


class TestBitInsertForward:
    """Test forward bit insertion (for kt component)."""

    def test_bit_insert_forward_no_mask(self) -> None:
        """Test forward insertion with zero mask does nothing."""
        data = BitVector(8)
        mask = BitVector(8)

        reader = BitReader(b"")
        bit_insert_forward(reader, data, mask)

        for i in range(8):
            assert data.get_bit(i) == 0

    def test_bit_insert_forward_full_mask(self) -> None:
        """Test forward insertion with full mask."""
        data = BitVector(8)
        mask = BitVector(8)
        mask.from_bytes(bytes([0xFF]))

        original = bytes([0b10110100])
        bv_orig = BitVector(8)
        bv_orig.from_bytes(original)

        # Extract bits in forward order
        bb = BitBuffer()
        bit_extract_forward(bb, bv_orig, mask)

        # Insert back in forward order
        reader = BitReader(bb.to_bytes())
        bit_insert_forward(reader, data, mask)

        # Should match original
        assert data.to_bytes() == original

    def test_bit_insert_forward_partial_mask(self) -> None:
        """Test forward insertion with partial mask."""
        data = BitVector(8)
        mask = BitVector(8)
        mask.from_bytes(bytes([0b10100000]))

        original = BitVector(8)
        original.from_bytes(bytes([0b11110000]))

        # Extract bits at mask positions in forward order
        bb = BitBuffer()
        bit_extract_forward(bb, original, mask)

        # Insert back in forward order
        reader = BitReader(bb.to_bytes())
        bit_insert_forward(reader, data, mask)

        # Check only masked positions
        assert data.get_bit(0) == original.get_bit(0)
        assert data.get_bit(2) == original.get_bit(2)

    def test_bit_extract_insert_forward_round_trip(self) -> None:
        """Test forward extract/insert round trip."""
        data = BitVector(16)
        data.from_bytes(bytes([0xDE, 0xAD]))

        mask = BitVector(16)
        mask.from_bytes(bytes([0xAA, 0x55]))

        # Extract in forward order
        bb = BitBuffer()
        bit_extract_forward(bb, data, mask)

        # Create new vector and insert in forward order
        result = BitVector(16)
        reader = BitReader(bb.to_bytes())
        bit_insert_forward(reader, result, mask)

        # Check only masked positions match
        for i in range(16):
            if mask.get_bit(i):
                assert result.get_bit(i) == data.get_bit(i)


class TestRLEDeltaValidation:
    """GOTCHAS #21 / issue #92: invalid RLE deltas must be rejected."""

    def test_rle_decode_rejects_delta_beyond_position(self) -> None:
        # COUNT(9) = '110'+BIT5(7), then terminator '10' -> 0xC7 0x80.
        # Delta 9 exceeds an 8-bit vector: must raise, not silently skip.
        reader = BitReader(bytes([0xC7, 0x80]))
        with pytest.raises(ValueError):
            rle_decode(reader, 8)
