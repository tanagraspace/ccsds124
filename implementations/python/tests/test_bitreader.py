"""Tests for BitReader class."""

import pytest

from ccsds124.bitreader import BitReader


class TestBitReaderInit:
    """Test BitReader initialization."""

    def test_init(self) -> None:
        """Test creating a BitReader."""
        br = BitReader(bytes([0xFF, 0x00]))
        assert br.position == 0
        assert br.remaining == 16

    def test_init_empty(self) -> None:
        """Test creating a BitReader with empty data."""
        br = BitReader(b"")
        assert br.position == 0
        assert br.remaining == 0


class TestBitReaderReadBit:
    """Test read_bit method."""

    def test_read_single_bit(self) -> None:
        """Test reading a single bit."""
        br = BitReader(bytes([0b10000000]))
        bit = br.read_bit()
        assert bit == 1
        assert br.position == 1

    def test_read_bits_msb_first(self) -> None:
        """Test that bits are read MSB-first."""
        br = BitReader(bytes([0b10110100]))

        assert br.read_bit() == 1  # bit 7 (MSB)
        assert br.read_bit() == 0  # bit 6
        assert br.read_bit() == 1  # bit 5
        assert br.read_bit() == 1  # bit 4
        assert br.read_bit() == 0  # bit 3
        assert br.read_bit() == 1  # bit 2
        assert br.read_bit() == 0  # bit 1
        assert br.read_bit() == 0  # bit 0 (LSB)

    def test_read_across_byte_boundary(self) -> None:
        """Test reading across byte boundary."""
        br = BitReader(bytes([0xFF, 0x00]))

        # Read all 1s from first byte
        for _ in range(8):
            assert br.read_bit() == 1

        # Read all 0s from second byte
        for _ in range(8):
            assert br.read_bit() == 0

    def test_read_bit_underflow(self) -> None:
        """Test reading past end of data raises error."""
        br = BitReader(bytes([0xFF]))

        # Read all 8 bits
        for _ in range(8):
            br.read_bit()

        # Next read should raise
        with pytest.raises(EOFError):
            br.read_bit()


class TestBitReaderReadBits:
    """Test read_bits method."""

    def test_read_bits_single_byte(self) -> None:
        """Test reading 8 bits as integer."""
        br = BitReader(bytes([0xAB]))
        value = br.read_bits(8)
        assert value == 0xAB

    def test_read_bits_partial(self) -> None:
        """Test reading fewer than 8 bits."""
        br = BitReader(bytes([0b11110000]))
        value = br.read_bits(4)
        assert value == 0b1111  # First 4 bits

    def test_read_bits_multiple_bytes(self) -> None:
        """Test reading more than 8 bits."""
        br = BitReader(bytes([0xFF, 0x00]))
        value = br.read_bits(16)
        assert value == 0xFF00

    def test_read_bits_cross_boundary(self) -> None:
        """Test reading bits that cross byte boundary."""
        br = BitReader(bytes([0b11110000, 0b00001111]))
        _ = br.read_bits(4)  # Skip first 4 bits (1111)
        value = br.read_bits(8)  # Read next 8 bits (0000 0000)
        assert value == 0b00000000

    def test_read_bits_zero(self) -> None:
        """Test reading zero bits."""
        br = BitReader(bytes([0xFF]))
        value = br.read_bits(0)
        assert value == 0
        assert br.position == 0

    def test_read_bits_underflow(self) -> None:
        """Test reading more bits than available."""
        br = BitReader(bytes([0xFF]))
        with pytest.raises(EOFError):
            br.read_bits(16)


class TestBitReaderPosition:
    """Test position and remaining properties."""

    def test_position_initial(self) -> None:
        """Test initial position is 0."""
        br = BitReader(bytes([0xFF]))
        assert br.position == 0

    def test_position_after_read(self) -> None:
        """Test position updates after reading."""
        br = BitReader(bytes([0xFF, 0xFF]))
        br.read_bits(5)
        assert br.position == 5

    def test_remaining_initial(self) -> None:
        """Test initial remaining count."""
        br = BitReader(bytes([0xAB, 0xCD, 0xEF]))
        assert br.remaining == 24

    def test_remaining_after_read(self) -> None:
        """Test remaining updates after reading."""
        br = BitReader(bytes([0xFF, 0xFF]))
        br.read_bits(5)
        assert br.remaining == 11


class TestBitReaderPeek:
    """Test peek_bit method."""

    def test_peek_bit(self) -> None:
        """Test peeking at next bit without consuming."""
        br = BitReader(bytes([0b10000000]))
        bit = br.peek_bit()
        assert bit == 1
        assert br.position == 0  # Position unchanged

    def test_peek_then_read(self) -> None:
        """Test peek followed by read."""
        br = BitReader(bytes([0b11000000]))
        assert br.peek_bit() == 1
        assert br.read_bit() == 1
        assert br.peek_bit() == 1
        assert br.read_bit() == 1
        assert br.peek_bit() == 0

    def test_peek_empty_raises(self) -> None:
        """Test peek on empty reader raises error."""
        br = BitReader(b"")
        with pytest.raises(EOFError):
            br.peek_bit()
