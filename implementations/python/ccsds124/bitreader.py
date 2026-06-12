"""
Sequential bit reader for decompression.

This module provides sequential bit-level reading from bytes,
used for parsing compressed CCSDS 124.0-B-1 streams.

Bit Ordering:
Bits are read MSB-first within each byte (matching BitBuffer output):
- First bit read is bit position 7 (MSB)
- Last bit read is bit position 0 (LSB)

MicroPython compatible - no typing module imports.
"""


class BitReader:
    """Sequential bit reader from bytes."""

    def __init__(self, data: bytes) -> None:
        """
        Initialize a bit reader.

        Args:
            data: Bytes to read from
        """
        self._data = data
        self._total_bits = len(data) * 8
        self.position = 0

    @property
    def remaining(self) -> int:
        """Number of bits remaining to read."""
        return self._total_bits - self.position

    def peek_bit(self) -> int:
        """
        Peek at the next bit without consuming it.

        Returns:
            Next bit value (0 or 1)

        Raises:
            EOFError: If no more bits available
        """
        if self.position >= self._total_bits:
            raise EOFError("No more bits to read")

        byte_index = self.position // 8
        bit_index = self.position % 8

        # MSB-first: bit 0 in stream is bit 7 of first byte
        return (self._data[byte_index] >> (7 - bit_index)) & 1

    def read_bit(self) -> int:
        """
        Read and consume a single bit.

        Returns:
            Bit value (0 or 1)

        Raises:
            EOFError: If no more bits available
        """
        bit = self.peek_bit()
        self.position += 1
        return bit

    def read_bits(self, num_bits: int) -> int:
        """
        Read and consume multiple bits as an integer.

        Args:
            num_bits: Number of bits to read

        Returns:
            Integer value of bits (MSB-first)

        Raises:
            EOFError: If not enough bits available
        """
        if num_bits == 0:
            return 0

        if self.position + num_bits > self._total_bits:
            raise EOFError(f"Not enough bits: need {num_bits}, have {self.remaining}")

        result = 0
        for _ in range(num_bits):
            result = (result << 1) | self.read_bit()

        return result

    def align_byte(self) -> None:
        """
        Advance to the next byte boundary.

        If already at a byte boundary, does nothing.
        Used for packet separation in compressed streams.
        """
        bit_offset = self.position % 8
        if bit_offset != 0:
            self.position += 8 - bit_offset
