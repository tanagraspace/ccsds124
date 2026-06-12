"""Tests for BitBuffer class."""

from ccsds124.bitbuffer import BitBuffer
from ccsds124.bitvector import BitVector


class TestBitBufferInit:
    """Test BitBuffer initialization."""

    def test_init(self) -> None:
        """Test creating an empty BitBuffer."""
        bb = BitBuffer()
        assert bb.num_bits == 0

    def test_init_empty_to_bytes(self) -> None:
        """Test that empty buffer produces empty bytes."""
        bb = BitBuffer()
        assert bb.to_bytes() == b""


class TestBitBufferAppendBit:
    """Test append_bit method."""

    def test_append_single_bit(self) -> None:
        """Test appending a single bit."""
        bb = BitBuffer()
        bb.append_bit(1)
        assert bb.num_bits == 1

    def test_append_byte_worth_of_bits(self) -> None:
        """Test appending 8 bits."""
        bb = BitBuffer()
        # Append 10110100 (MSB first)
        bits = [1, 0, 1, 1, 0, 1, 0, 0]
        for bit in bits:
            bb.append_bit(bit)

        assert bb.num_bits == 8
        assert bb.to_bytes() == bytes([0b10110100])

    def test_append_bits_msb_first(self) -> None:
        """Test that bits are appended MSB-first."""
        bb = BitBuffer()
        bb.append_bit(1)  # Goes to bit 7 (MSB)
        bb.append_bit(0)  # Goes to bit 6
        bb.append_bit(0)  # Goes to bit 5
        bb.append_bit(0)  # Goes to bit 4
        bb.append_bit(0)  # Goes to bit 3
        bb.append_bit(0)  # Goes to bit 2
        bb.append_bit(0)  # Goes to bit 1
        bb.append_bit(1)  # Goes to bit 0 (LSB)

        assert bb.to_bytes() == bytes([0b10000001])


class TestBitBufferAppendBits:
    """Test append_bits method."""

    def test_append_bits_single_byte(self) -> None:
        """Test appending bits from a single byte."""
        bb = BitBuffer()
        bb.append_bits(bytes([0xAB]), 8)

        assert bb.num_bits == 8
        assert bb.to_bytes() == bytes([0xAB])

    def test_append_bits_partial_byte(self) -> None:
        """Test appending fewer than 8 bits."""
        bb = BitBuffer()
        # Append only first 4 bits of 0b10110000
        bb.append_bits(bytes([0b10110000]), 4)

        assert bb.num_bits == 4
        # First 4 bits: 1011 -> should produce 0b10110000 when read as MSB-first
        result = bb.to_bytes()
        assert result == bytes([0b10110000])

    def test_append_bits_multiple_bytes(self) -> None:
        """Test appending bits from multiple bytes."""
        bb = BitBuffer()
        bb.append_bits(bytes([0xDE, 0xAD]), 16)

        assert bb.num_bits == 16
        assert bb.to_bytes() == bytes([0xDE, 0xAD])

    def test_append_bits_cross_byte_boundary(self) -> None:
        """Test appending bits that cross byte boundary."""
        bb = BitBuffer()
        bb.append_bits(bytes([0xFF]), 4)  # First 4 bits: 1111
        bb.append_bits(bytes([0x00]), 4)  # First 4 bits: 0000

        assert bb.num_bits == 8
        assert bb.to_bytes() == bytes([0b11110000])


class TestBitBufferAppendBitVector:
    """Test append_bitvector method."""

    def test_append_bitvector_full_bytes(self) -> None:
        """Test appending a BitVector with full bytes."""
        bb = BitBuffer()
        bv = BitVector(16)
        bv.from_bytes(bytes([0xCA, 0xFE]))

        bb.append_bitvector(bv)

        assert bb.num_bits == 16
        assert bb.to_bytes() == bytes([0xCA, 0xFE])

    def test_append_bitvector_partial_byte(self) -> None:
        """Test appending a BitVector with partial byte."""
        bb = BitBuffer()
        bv = BitVector(4)
        bv.set_bit(0, 1)
        bv.set_bit(1, 0)
        bv.set_bit(2, 1)
        bv.set_bit(3, 1)

        bb.append_bitvector(bv)

        assert bb.num_bits == 4
        # Bits 1011 at MSB positions
        assert bb.to_bytes() == bytes([0b10110000])

    def test_append_multiple_bitvectors(self) -> None:
        """Test appending multiple BitVectors."""
        bb = BitBuffer()
        bv1 = BitVector(8)
        bv1.from_bytes(bytes([0xFF]))
        bv2 = BitVector(8)
        bv2.from_bytes(bytes([0x00]))

        bb.append_bitvector(bv1)
        bb.append_bitvector(bv2)

        assert bb.num_bits == 16
        assert bb.to_bytes() == bytes([0xFF, 0x00])


class TestBitBufferToBytes:
    """Test to_bytes method."""

    def test_to_bytes_full_bytes(self) -> None:
        """Test converting full bytes."""
        bb = BitBuffer()
        bb.append_bits(bytes([0xDE, 0xAD, 0xBE, 0xEF]), 32)

        assert bb.to_bytes() == bytes([0xDE, 0xAD, 0xBE, 0xEF])

    def test_to_bytes_partial_byte(self) -> None:
        """Test converting with trailing partial byte."""
        bb = BitBuffer()
        bb.append_bits(bytes([0xFF]), 4)

        result = bb.to_bytes()
        assert len(result) == 1
        # First 4 bits are 1111, remaining should be 0
        assert result == bytes([0b11110000])

    def test_to_bytes_empty(self) -> None:
        """Test converting empty buffer."""
        bb = BitBuffer()
        assert bb.to_bytes() == b""


class TestBitBufferClear:
    """Test clear method."""

    def test_clear(self) -> None:
        """Test clearing a buffer."""
        bb = BitBuffer()
        bb.append_bits(bytes([0xFF, 0xFF]), 16)
        assert bb.num_bits == 16

        bb.clear()
        assert bb.num_bits == 0
        assert bb.to_bytes() == b""


class TestBitBufferSize:
    """Test size property."""

    def test_size_empty(self) -> None:
        """Test size of empty buffer."""
        bb = BitBuffer()
        assert bb.num_bits == 0

    def test_size_after_append(self) -> None:
        """Test size after appending bits."""
        bb = BitBuffer()
        bb.append_bit(1)
        bb.append_bit(0)
        bb.append_bit(1)

        assert bb.num_bits == 3
