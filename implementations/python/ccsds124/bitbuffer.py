"""
Variable-length bit buffer for building compressed output.

This module provides a dynamically-growing bit buffer for constructing
compressed output streams. Bits are appended sequentially using MSB-first
ordering as required by CCSDS 124.0-B-1.

Bit Ordering:
Bits are appended MSB-first within each byte:
- First bit appended goes to bit position 7
- Second bit goes to position 6, etc.

MicroPython compatible - no typing module imports.
"""

# Import for type hints only - works at runtime
if False:  # noqa: SIM108
    from ccsds124.bitvector import BitVector


class BitBuffer:
    """Variable-length bit buffer for building compressed output."""

    def __init__(self) -> None:
        """Initialize an empty bit buffer."""
        self._data = bytearray()
        self.num_bits = 0

    def clear(self) -> None:
        """Clear the buffer."""
        self._data = bytearray()
        self.num_bits = 0

    def append_bit(self, bit: int) -> None:
        """
        Append a single bit to the buffer.

        Args:
            bit: Bit value (0 or non-zero for 1)
        """
        byte_index = self.num_bits // 8
        bit_index = self.num_bits % 8

        # Extend buffer if needed
        while byte_index >= len(self._data):
            self._data.append(0)

        # CCSDS uses MSB-first bit ordering: first bit goes to position 7
        if bit:
            self._data[byte_index] |= 1 << (7 - bit_index)

        self.num_bits += 1

    def append_bits(self, data: bytes, num_bits: int) -> None:
        """
        Append multiple bits from bytes.

        Args:
            data: Source bytes
            num_bits: Number of bits to append (MSB-first)
        """
        for i in range(num_bits):
            byte_index = i // 8
            bit_index = i % 8

            # Extract bits MSB-first (bit 7, 6, 5, ..., 0)
            bit = (data[byte_index] >> (7 - bit_index)) & 1
            self.append_bit(bit)

    def append_bitvector(self, bv: "BitVector") -> None:
        """
        Append all bits from a BitVector.

        Args:
            bv: BitVector to append
        """
        # Calculate number of bytes from bit length
        num_bytes = (bv.length + 7) // 8

        # CCSDS MSB-first: bytes in order, bits within each byte from MSB to LSB
        for byte_idx in range(num_bytes):
            bits_in_this_byte = 8

            # Last byte may have fewer than 8 bits
            if byte_idx == num_bytes - 1:
                remainder = bv.length % 8
                if remainder != 0:
                    bits_in_this_byte = remainder

            # With MSB-first bitvector indexing: bit 0 is MSB, bit 7 is LSB
            # We want to append bits in order: MSB first, LSB last
            start_bit = byte_idx * 8
            for bit_offset in range(bits_in_this_byte):
                pos = start_bit + bit_offset
                bit = bv.get_bit(pos)
                self.append_bit(bit)

    def to_bytes(self) -> bytes:
        """
        Convert buffer contents to bytes.

        Returns:
            Bytes representation of the buffer
        """
        if self.num_bits == 0:
            return b""

        # Calculate number of bytes needed
        num_bytes = (self.num_bits + 7) // 8
        return bytes(self._data[:num_bytes])
