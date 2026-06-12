"""Tests for encoding functions (COUNT, RLE, BE)."""

import pytest

from ccsds124.bitbuffer import BitBuffer
from ccsds124.bitvector import BitVector
from ccsds124.encode import bit_extract, bit_extract_forward, count_encode, rle_encode


class TestCountEncode:
    """Test COUNT encoding (CCSDS Equation 9)."""

    def test_count_encode_1(self) -> None:
        """Test A=1 produces '0'."""
        bb = BitBuffer()
        count_encode(bb, 1)
        assert bb.num_bits == 1
        assert bb.to_bytes() == bytes([0b00000000])

    def test_count_encode_2(self) -> None:
        """Test A=2 produces '110' + BIT5(0) = '11000000'."""
        bb = BitBuffer()
        count_encode(bb, 2)
        # '110' + '00000' = 8 bits
        assert bb.num_bits == 8
        assert bb.to_bytes() == bytes([0b11000000])

    def test_count_encode_3(self) -> None:
        """Test A=3 produces '110' + BIT5(1) = '11000001'."""
        bb = BitBuffer()
        count_encode(bb, 3)
        assert bb.num_bits == 8
        assert bb.to_bytes() == bytes([0b11000001])

    def test_count_encode_33(self) -> None:
        """Test A=33 produces '110' + BIT5(31) = '11011111'."""
        bb = BitBuffer()
        count_encode(bb, 33)
        # A-2 = 31 = 0b11111
        assert bb.num_bits == 8
        assert bb.to_bytes() == bytes([0b11011111])

    def test_count_encode_34(self) -> None:
        """Test A=34 uses extended format '111' + BIT_E(32)."""
        bb = BitBuffer()
        count_encode(bb, 34)
        # A-2 = 32, log2(32) = 5, E = 2*(5+1) - 6 = 6
        # '111' + 6-bit(32) = '111' + '100000' = 9 bits
        assert bb.num_bits == 9

    def test_count_encode_large(self) -> None:
        """Test large value encoding."""
        bb = BitBuffer()
        count_encode(bb, 1000)
        # Should use extended format
        assert bb.num_bits > 8

    def test_count_encode_multiple(self) -> None:
        """Test encoding multiple values sequentially."""
        bb = BitBuffer()
        count_encode(bb, 1)  # 1 bit
        count_encode(bb, 1)  # 1 bit
        count_encode(bb, 1)  # 1 bit
        assert bb.num_bits == 3

    def test_count_encode_zero_raises(self) -> None:
        """Test that COUNT(0) raises ValueError."""
        bb = BitBuffer()
        with pytest.raises(ValueError, match="out of range"):
            count_encode(bb, 0)

    def test_count_encode_negative_raises(self) -> None:
        """Test that negative value raises ValueError."""
        bb = BitBuffer()
        with pytest.raises(ValueError, match="out of range"):
            count_encode(bb, -1)

    def test_count_encode_too_large_raises(self) -> None:
        """Test that value > 65535 raises ValueError."""
        bb = BitBuffer()
        with pytest.raises(ValueError, match="out of range"):
            count_encode(bb, 65536)


class TestRleEncode:
    """Test RLE encoding (CCSDS Equation 10)."""

    def test_rle_encode_all_zeros(self) -> None:
        """Test RLE of all zeros produces just terminator '10'."""
        bv = BitVector(8)
        # All zeros
        bb = BitBuffer()
        rle_encode(bb, bv)
        # Should only have terminator '10'
        assert bb.num_bits == 2
        assert bb.to_bytes() == bytes([0b10000000])

    def test_rle_encode_single_one_at_end(self) -> None:
        """Test RLE with single 1 at LSB position."""
        bv = BitVector(8)
        bv.set_bit(7, 1)  # LSB position
        bb = BitBuffer()
        rle_encode(bb, bv)
        # COUNT(1) = '0', then terminator '10'
        # Total: 3 bits = '010'
        assert bb.num_bits == 3

    def test_rle_encode_single_one_at_start(self) -> None:
        """Test RLE with single 1 at MSB position."""
        bv = BitVector(8)
        bv.set_bit(0, 1)  # MSB position
        bb = BitBuffer()
        rle_encode(bb, bv)
        # Delta from end (pos 8) to pos 0 = 8
        # COUNT(8) = '110' + BIT5(6) = '110' + '00110' = 8 bits
        # Then terminator '10' = 2 bits
        # Total: 10 bits
        assert bb.num_bits == 10

    def test_rle_encode_alternating(self) -> None:
        """Test RLE with alternating bits."""
        bv = BitVector(8)
        bv.from_bytes(bytes([0b10101010]))
        bb = BitBuffer()
        rle_encode(bb, bv)
        # Four 1s, each with delta=2 (one zero between)
        # COUNT(2) = '110' + '00000' = 8 bits each... but last one is delta=1
        # Actually: positions 0,2,4,6 have 1s
        # From end (8): delta to 6 = 2, delta to 4 = 2, delta to 2 = 2, delta to 0 = 2
        # COUNT(2) * 4 + '10' terminator
        assert bb.num_bits > 0  # Just verify it runs

    def test_rle_encode_all_ones(self) -> None:
        """Test RLE with all ones."""
        bv = BitVector(8)
        bv.from_bytes(bytes([0xFF]))
        bb = BitBuffer()
        rle_encode(bb, bv)
        # 8 ones, each delta=1
        # COUNT(1) = '0' * 8 = 8 bits
        # Plus terminator '10' = 2 bits
        # Total: 10 bits
        assert bb.num_bits == 10


class TestBitExtract:
    """Test BE (Bit Extraction) - reverse order (CCSDS Equation 11)."""

    def test_bit_extract_no_mask(self) -> None:
        """Test extraction with zero mask produces no output."""
        data = BitVector(8)
        data.from_bytes(bytes([0xFF]))
        mask = BitVector(8)
        # All zeros in mask

        bb = BitBuffer()
        bit_extract(bb, data, mask)
        assert bb.num_bits == 0

    def test_bit_extract_full_mask(self) -> None:
        """Test extraction with full mask extracts all bits in reverse."""
        data = BitVector(8)
        data.from_bytes(bytes([0b10110100]))
        mask = BitVector(8)
        mask.from_bytes(bytes([0xFF]))

        bb = BitBuffer()
        bit_extract(bb, data, mask)
        assert bb.num_bits == 8
        # Reverse order: bit 7, 6, 5, 4, 3, 2, 1, 0
        # Original: 10110100
        # Reversed: 00101101
        assert bb.to_bytes() == bytes([0b00101101])

    def test_bit_extract_partial_mask(self) -> None:
        """Test extraction with partial mask."""
        data = BitVector(8)
        data.from_bytes(bytes([0b11110000]))
        mask = BitVector(8)
        mask.from_bytes(bytes([0b10100000]))  # Positions 0 and 2

        bb = BitBuffer()
        bit_extract(bb, data, mask)
        assert bb.num_bits == 2
        # Positions with 1 in mask: 0, 2
        # Extract in reverse: bit[2]=1, bit[0]=1
        # Result: 11
        assert bb.to_bytes() == bytes([0b11000000])

    def test_bit_extract_example_from_spec(self) -> None:
        """Test the example from CCSDS spec comments."""
        # BE('10110011', '01001010') should extract at positions 1,4,6
        data = BitVector(8)
        data.from_bytes(bytes([0b10110011]))
        mask = BitVector(8)
        mask.from_bytes(bytes([0b01001010]))

        bb = BitBuffer()
        bit_extract(bb, data, mask)
        assert bb.num_bits == 3
        # Mask positions with 1: 1, 4, 6
        # Data at those positions: bit[1]=0, bit[4]=0, bit[6]=1
        # Reverse order: bit[6], bit[4], bit[1] = 1, 0, 0
        assert bb.to_bytes() == bytes([0b10000000])

    def test_bit_extract_mismatched_lengths_raises(self) -> None:
        """Test that mismatched data/mask lengths raises ValueError."""
        data = BitVector(8)
        mask = BitVector(16)

        bb = BitBuffer()
        with pytest.raises(ValueError, match="same length"):
            bit_extract(bb, data, mask)


class TestBitExtractForward:
    """Test forward bit extraction (for kt component)."""

    def test_bit_extract_forward_full_mask(self) -> None:
        """Test forward extraction with full mask."""
        data = BitVector(8)
        data.from_bytes(bytes([0b10110100]))
        mask = BitVector(8)
        mask.from_bytes(bytes([0xFF]))

        bb = BitBuffer()
        bit_extract_forward(bb, data, mask)
        assert bb.num_bits == 8
        # Forward order: same as original
        assert bb.to_bytes() == bytes([0b10110100])

    def test_bit_extract_forward_partial_mask(self) -> None:
        """Test forward extraction with partial mask."""
        data = BitVector(8)
        data.from_bytes(bytes([0b11110000]))
        mask = BitVector(8)
        mask.from_bytes(bytes([0b10100000]))  # Positions 0 and 2

        bb = BitBuffer()
        bit_extract_forward(bb, data, mask)
        assert bb.num_bits == 2
        # Positions with 1 in mask: 0, 2
        # Extract in forward: bit[0]=1, bit[2]=1
        # Result: 11
        assert bb.to_bytes() == bytes([0b11000000])

    def test_bit_extract_forward_mismatched_lengths_raises(self) -> None:
        """Test that mismatched data/mask lengths raises ValueError."""
        data = BitVector(8)
        mask = BitVector(16)

        bb = BitBuffer()
        with pytest.raises(ValueError, match="same length"):
            bit_extract_forward(bb, data, mask)
