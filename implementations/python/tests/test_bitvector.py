"""Tests for BitVector class."""

import pytest

from pocketplus.bitvector import BitVector


class TestBitVectorInit:
    """Test BitVector initialization."""

    def test_init_with_length(self) -> None:
        """Test creating a BitVector with specified length."""
        bv = BitVector(720)
        assert bv.length == 720
        assert bv.num_words == 23  # ceil(720/8/4) = ceil(90/4) = 23

    def test_init_zero_length_raises(self) -> None:
        """Test that zero length raises ValueError."""
        with pytest.raises(ValueError):
            BitVector(0)

    def test_init_all_zeros(self) -> None:
        """Test that new BitVector is all zeros."""
        bv = BitVector(32)
        for i in range(32):
            assert bv.get_bit(i) == 0


class TestBitVectorGetSetBit:
    """Test get_bit and set_bit methods."""

    def test_set_and_get_bit(self) -> None:
        """Test setting and getting individual bits."""
        bv = BitVector(16)
        bv.set_bit(0, 1)
        bv.set_bit(7, 1)
        bv.set_bit(15, 1)

        assert bv.get_bit(0) == 1
        assert bv.get_bit(7) == 1
        assert bv.get_bit(15) == 1
        assert bv.get_bit(1) == 0
        assert bv.get_bit(8) == 0

    def test_set_bit_to_zero(self) -> None:
        """Test clearing a bit."""
        bv = BitVector(8)
        bv.set_bit(3, 1)
        assert bv.get_bit(3) == 1
        bv.set_bit(3, 0)
        assert bv.get_bit(3) == 0

    def test_get_bit_out_of_range(self) -> None:
        """Test that out of range access raises error."""
        bv = BitVector(8)
        with pytest.raises(IndexError):
            bv.get_bit(8)
        with pytest.raises(IndexError):
            bv.get_bit(-1)

    def test_set_bit_out_of_range(self) -> None:
        """Test that out of range set raises error."""
        bv = BitVector(8)
        with pytest.raises(IndexError):
            bv.set_bit(8, 1)


class TestBitVectorFromBytes:
    """Test from_bytes and to_bytes methods."""

    def test_from_bytes_single_byte(self) -> None:
        """Test loading from a single byte."""
        bv = BitVector(8)
        bv.from_bytes(bytes([0b10110100]))

        # MSB-first: bit 0 is the MSB
        assert bv.get_bit(0) == 1  # MSB
        assert bv.get_bit(1) == 0
        assert bv.get_bit(2) == 1
        assert bv.get_bit(3) == 1
        assert bv.get_bit(4) == 0
        assert bv.get_bit(5) == 1
        assert bv.get_bit(6) == 0
        assert bv.get_bit(7) == 0  # LSB

    def test_from_bytes_multiple_bytes(self) -> None:
        """Test loading from multiple bytes."""
        bv = BitVector(16)
        bv.from_bytes(bytes([0xFF, 0x00]))

        # First byte (0xFF) -> bits 0-7 all 1
        for i in range(8):
            assert bv.get_bit(i) == 1

        # Second byte (0x00) -> bits 8-15 all 0
        for i in range(8, 16):
            assert bv.get_bit(i) == 0

    def test_to_bytes_single_byte(self) -> None:
        """Test converting to bytes."""
        bv = BitVector(8)
        bv.set_bit(0, 1)  # MSB
        bv.set_bit(2, 1)
        bv.set_bit(3, 1)
        bv.set_bit(5, 1)

        result = bv.to_bytes()
        assert result == bytes([0b10110100])

    def test_to_bytes_multiple_bytes(self) -> None:
        """Test converting multiple bytes."""
        bv = BitVector(16)
        for i in range(8):
            bv.set_bit(i, 1)

        result = bv.to_bytes()
        assert result == bytes([0xFF, 0x00])

    def test_round_trip(self) -> None:
        """Test from_bytes -> to_bytes round trip."""
        original = bytes([0xDE, 0xAD, 0xBE, 0xEF])
        bv = BitVector(32)
        bv.from_bytes(original)
        result = bv.to_bytes()
        assert result == original


class TestBitVectorBitwiseOps:
    """Test bitwise operations (XOR, OR, AND, NOT)."""

    def test_xor(self) -> None:
        """Test XOR operation."""
        a = BitVector(8)
        b = BitVector(8)
        a.from_bytes(bytes([0b11110000]))
        b.from_bytes(bytes([0b10101010]))

        result = a.xor(b)
        assert result.to_bytes() == bytes([0b01011010])

    def test_or(self) -> None:
        """Test OR operation."""
        a = BitVector(8)
        b = BitVector(8)
        a.from_bytes(bytes([0b11110000]))
        b.from_bytes(bytes([0b10101010]))

        result = a.or_(b)
        assert result.to_bytes() == bytes([0b11111010])

    def test_and(self) -> None:
        """Test AND operation."""
        a = BitVector(8)
        b = BitVector(8)
        a.from_bytes(bytes([0b11110000]))
        b.from_bytes(bytes([0b10101010]))

        result = a.and_(b)
        assert result.to_bytes() == bytes([0b10100000])

    def test_not(self) -> None:
        """Test NOT operation."""
        a = BitVector(8)
        a.from_bytes(bytes([0b11110000]))

        result = a.not_()
        assert result.to_bytes() == bytes([0b00001111])

    def test_not_with_partial_byte(self) -> None:
        """Test NOT with non-byte-aligned length."""
        a = BitVector(4)
        a.from_bytes(bytes([0b11110000]))  # Only first 4 bits matter

        result = a.not_()
        # First 4 bits inverted: 1111 -> 0000
        # But we only care about 4 bits
        for i in range(4):
            assert result.get_bit(i) == 0

    def test_not_msb_aligned_non_byte_aligned(self) -> None:
        """GOTCHAS #20 (issue #103): NOT of a non-byte-aligned vector must set
        the valid (high) bits and leave padding zero.

        NOT of an all-zero length-12 vector: positions 0..11 must all be 1.
        The pre-fix bug used a low-bits mask, leaving positions 8..11 at 0 and
        wrongly setting padding bits 12..15.
        """
        a = BitVector(12)  # all zeros
        result = a.not_()
        assert [result.get_bit(i) for i in range(12)] == [1] * 12
        assert result.hamming_weight() == 12

    def test_not_multi_byte_partial(self) -> None:
        """Test NOT with multiple bytes where last byte is partial.

        This tests the edge case in line 284 where we have multiple
        bytes in the last word and need to mask properly.
        """
        # 12 bits = 1.5 bytes, spans into 2nd byte of a 32-bit word
        a = BitVector(12)
        a.from_bytes(bytes([0xFF, 0xF0]))  # All 1s in first 12 bits

        result = a.not_()
        # All 12 bits should be inverted to 0
        for i in range(12):
            assert result.get_bit(i) == 0

    def test_not_exactly_two_bytes(self) -> None:
        """Test NOT with exactly 16 bits (2 bytes)."""
        a = BitVector(16)
        a.from_bytes(bytes([0xFF, 0xFF]))

        result = a.not_()
        assert result.to_bytes() == bytes([0x00, 0x00])

    def test_not_three_bytes_partial(self) -> None:
        """Test NOT with 20 bits (2.5 bytes) to exercise middle byte path."""
        a = BitVector(20)
        a.from_bytes(bytes([0xFF, 0xFF, 0xF0]))  # All 1s in first 20 bits

        result = a.not_()
        for i in range(20):
            assert result.get_bit(i) == 0


class TestBitVectorLeftShift:
    """Test left shift operation."""

    def test_left_shift(self) -> None:
        """Test left shift moves bits toward MSB."""
        a = BitVector(8)
        a.from_bytes(bytes([0b00001111]))

        result = a.left_shift()
        # Left shift: each bit moves to lower index, LSB gets 0
        # 00001111 -> 00011110
        assert result.to_bytes() == bytes([0b00011110])

    def test_left_shift_msb_lost(self) -> None:
        """Test that MSB is lost on left shift."""
        a = BitVector(8)
        a.from_bytes(bytes([0b10000001]))

        result = a.left_shift()
        # MSB (1) is lost, LSB becomes 0
        # 10000001 -> 00000010
        assert result.to_bytes() == bytes([0b00000010])


class TestBitVectorHammingWeight:
    """Test hamming weight (popcount)."""

    def test_hamming_weight_all_zeros(self) -> None:
        """Test hamming weight of all zeros."""
        bv = BitVector(32)
        assert bv.hamming_weight() == 0

    def test_hamming_weight_all_ones(self) -> None:
        """Test hamming weight of all ones."""
        bv = BitVector(8)
        bv.from_bytes(bytes([0xFF]))
        assert bv.hamming_weight() == 8

    def test_hamming_weight_mixed(self) -> None:
        """Test hamming weight of mixed bits."""
        bv = BitVector(8)
        bv.from_bytes(bytes([0b10110100]))
        assert bv.hamming_weight() == 4

    def test_hamming_weight_partial_byte(self) -> None:
        """Test hamming weight with non-byte-aligned length."""
        bv = BitVector(4)
        bv.set_bit(0, 1)
        bv.set_bit(1, 1)
        bv.set_bit(2, 1)
        bv.set_bit(3, 1)
        assert bv.hamming_weight() == 4


class TestBitVectorCopy:
    """Test copy operation."""

    def test_copy(self) -> None:
        """Test copying a BitVector."""
        a = BitVector(16)
        a.from_bytes(bytes([0xDE, 0xAD]))

        b = a.copy()
        assert b.to_bytes() == a.to_bytes()
        assert b.length == a.length

    def test_copy_independence(self) -> None:
        """Test that copy is independent of original."""
        a = BitVector(8)
        a.from_bytes(bytes([0xFF]))

        b = a.copy()
        b.set_bit(0, 0)

        assert a.get_bit(0) == 1
        assert b.get_bit(0) == 0


class TestBitVectorEquals:
    """Test equality comparison."""

    def test_equals_same(self) -> None:
        """Test that identical vectors are equal."""
        a = BitVector(8)
        b = BitVector(8)
        a.from_bytes(bytes([0xAB]))
        b.from_bytes(bytes([0xAB]))

        assert a.equals(b)

    def test_equals_different(self) -> None:
        """Test that different vectors are not equal."""
        a = BitVector(8)
        b = BitVector(8)
        a.from_bytes(bytes([0xAB]))
        b.from_bytes(bytes([0xCD]))

        assert not a.equals(b)

    def test_equals_different_length(self) -> None:
        """Test that different length vectors are not equal."""
        a = BitVector(8)
        b = BitVector(16)

        assert not a.equals(b)


class TestBitVectorZero:
    """Test zero operation."""

    def test_zero(self) -> None:
        """Test zeroing a BitVector."""
        bv = BitVector(16)
        bv.from_bytes(bytes([0xFF, 0xFF]))
        bv.zero()

        assert bv.to_bytes() == bytes([0x00, 0x00])
