"""
Fixed-length bit vector implementation using 32-bit words.

This module provides fixed-length bit vector operations optimized for
POCKET+ compression. Uses 32-bit words with big-endian byte packing to
match ESA/ESOC reference implementation.

Bit Numbering Convention (CCSDS 124.0-B-1 Section 1.6.1):
- Bit 0 = MSB (Most Significant Bit, transmitted first)
- Bit N-1 = LSB (Least Significant Bit)

MicroPython compatible - no typing module imports.
"""


class BitVector:
    """Fixed-length bit vector using 32-bit word storage."""

    def __init__(self, num_bits: int) -> None:
        """
        Initialize a bit vector with specified length.

        Args:
            num_bits: Number of bits (must be > 0)

        Raises:
            ValueError: If num_bits <= 0
        """
        if num_bits <= 0:
            raise ValueError("num_bits must be positive")

        self.length = num_bits
        # Calculate number of 32-bit words needed
        num_bytes = (num_bits + 7) // 8
        self.num_words = (num_bytes + 3) // 4

        # Initialize all words to zero
        self._data = [0] * self.num_words

    def zero(self) -> None:
        """Set all bits to zero."""
        for i in range(self.num_words):
            self._data[i] = 0

    def copy(self) -> "BitVector":
        """
        Create a copy of this bit vector.

        Returns:
            New BitVector with same contents
        """
        result = BitVector(self.length)
        for i in range(self.num_words):
            result._data[i] = self._data[i]
        return result

    def copy_from(self, other: "BitVector") -> None:
        """
        Copy contents from another bit vector into this one.

        Args:
            other: Source bit vector to copy from
        """
        num_words = min(self.num_words, other.num_words)
        for i in range(num_words):
            self._data[i] = other._data[i]

    def get_bit(self, pos: int) -> int:
        """
        Get bit value at position.

        Args:
            pos: Bit position (0 = MSB, length-1 = LSB)

        Returns:
            Bit value (0 or 1)

        Raises:
            IndexError: If pos is out of range
        """
        if pos < 0 or pos >= self.length:
            raise IndexError(f"Bit position {pos} out of range [0, {self.length})")

        # Calculate byte and bit within byte
        byte_index = pos // 8
        bit_in_byte = pos % 8

        # Calculate word index and byte position within word
        word_index = byte_index // 4
        byte_in_word = byte_index % 4

        # Big-endian: byte 0 is at bits 24-31, byte 1 at 16-23, etc.
        # MSB-first indexing: bit 0 is the MSB (leftmost) within each byte
        bit_in_word = ((3 - byte_in_word) * 8) + (7 - bit_in_byte)

        return (self._data[word_index] >> bit_in_word) & 1

    def set_bit(self, pos: int, value: int) -> None:
        """
        Set bit value at position.

        Args:
            pos: Bit position (0 = MSB, length-1 = LSB)
            value: Bit value (0 or non-zero for 1)

        Raises:
            IndexError: If pos is out of range
        """
        if pos < 0 or pos >= self.length:
            raise IndexError(f"Bit position {pos} out of range [0, {self.length})")

        # Calculate byte and bit within byte
        byte_index = pos // 8
        bit_in_byte = pos % 8

        # Calculate word index and byte position within word
        word_index = byte_index // 4
        byte_in_word = byte_index % 4

        # Big-endian: byte 0 is at bits 24-31, byte 1 at 16-23, etc.
        # MSB-first indexing: bit 0 is the MSB (leftmost) within each byte
        bit_in_word = ((3 - byte_in_word) * 8) + (7 - bit_in_byte)

        if value:
            # Set bit to 1
            self._data[word_index] |= 1 << bit_in_word
        else:
            # Clear bit to 0
            self._data[word_index] &= ~(1 << bit_in_word)

    def from_bytes(self, data: bytes) -> None:
        """
        Load bit vector from bytes.

        Args:
            data: Bytes to load (big-endian)
        """
        # Zero the array first
        self.zero()

        # Pack bytes into 32-bit words (big-endian)
        j = 4  # Counter for bytes within word (4, 3, 2, 1)
        bytes_to_int = 0
        current_word = 0

        for byte in data:
            j -= 1
            bytes_to_int |= byte << (j * 8)

            if j == 0:
                # Word complete - store it
                self._data[current_word] = bytes_to_int
                current_word += 1
                bytes_to_int = 0
                j = 4

        # Handle incomplete final word
        if j < 4:
            self._data[current_word] = bytes_to_int

    def to_bytes(self) -> bytes:
        """
        Convert bit vector to bytes.

        Returns:
            Bytes representation (big-endian)
        """
        expected_bytes = (self.length + 7) // 8
        result = bytearray(expected_bytes)

        byte_index = 0
        for word_index in range(self.num_words):
            word = self._data[word_index]

            # Extract up to 4 bytes from this word
            for j in range(3, -1, -1):
                if byte_index >= expected_bytes:
                    break
                result[byte_index] = (word >> (j * 8)) & 0xFF
                byte_index += 1

        return bytes(result)

    def xor(self, a: "BitVector", b: "BitVector | None" = None) -> "BitVector":
        """
        Compute XOR of bit vectors.

        Two calling conventions:
        - result = v.xor(other) - returns new vector with v XOR other
        - v.xor(a, b) - stores a XOR b into v (in-place)

        Args:
            a: First operand (or only operand if b is None)
            b: Second operand (optional)

        Returns:
            New BitVector if b is None, else self
        """
        if b is None:
            # Old API: return self XOR a
            result = BitVector(self.length)
            num_words = min(self.num_words, a.num_words)
            for i in range(num_words):
                result._data[i] = self._data[i] ^ a._data[i]
            return result
        else:
            # New API: self = a XOR b
            num_words = min(self.num_words, a.num_words, b.num_words)
            for i in range(num_words):
                self._data[i] = a._data[i] ^ b._data[i]
            return self

    def or_(self, a: "BitVector", b: "BitVector | None" = None) -> "BitVector":
        """
        Compute OR of bit vectors.

        Two calling conventions:
        - result = v.or_(other) - returns new vector with v OR other
        - v.or_(a, b) - stores a OR b into v (in-place)

        Args:
            a: First operand (or only operand if b is None)
            b: Second operand (optional)

        Returns:
            New BitVector if b is None, else self
        """
        if b is None:
            # Old API: return self OR a
            result = BitVector(self.length)
            num_words = min(self.num_words, a.num_words)
            for i in range(num_words):
                result._data[i] = self._data[i] | a._data[i]
            return result
        else:
            # New API: self = a OR b
            num_words = min(self.num_words, a.num_words, b.num_words)
            for i in range(num_words):
                self._data[i] = a._data[i] | b._data[i]
            return self

    def and_(self, other: "BitVector") -> "BitVector":
        """
        Compute AND with another bit vector.

        Args:
            other: Other bit vector

        Returns:
            New BitVector with result
        """
        result = BitVector(self.length)
        num_words = min(self.num_words, other.num_words)

        for i in range(num_words):
            result._data[i] = self._data[i] & other._data[i]

        return result

    def not_(self) -> "BitVector":
        """
        Compute NOT (bitwise inversion).

        Returns:
            New BitVector with inverted bits
        """
        result = BitVector(self.length)

        for i in range(self.num_words):
            result._data[i] = ~self._data[i] & 0xFFFFFFFF

        # Mask off unused bits in last word
        if self.num_words > 0:
            num_bytes = (self.length + 7) // 8
            bytes_in_last_word = ((num_bytes - 1) % 4) + 1
            bits_in_last_byte = self.length - ((num_bytes - 1) * 8)

            # Create mask for valid bits in big-endian word.
            # MSB-first: valid bits in a partial last byte are at the HIGH
            # end of the byte, so mask 0xFF << (8 - bits) (GOTCHAS #20).
            mask = 0
            for byte in range(bytes_in_last_word):
                if byte == bytes_in_last_word - 1:
                    byte_mask = (0xFF << (8 - bits_in_last_byte)) & 0xFF
                else:
                    byte_mask = 0xFF
                shift_amt = (3 - byte) * 8
                mask |= byte_mask << shift_amt

            result._data[self.num_words - 1] &= mask

        return result

    def left_shift(self) -> "BitVector":
        """
        Compute left shift by 1 position.

        Left shift moves bits toward MSB (lower indices).
        MSB is lost, LSB becomes 0.

        Returns:
            New BitVector with shifted bits
        """
        result = BitVector(self.length)

        # Word-level left shift (big-endian: MSB in high bits of first word)
        # Shift each word left by 1, carry MSB from next word
        carry = 0
        for i in range(self.num_words - 1, -1, -1):
            word = self._data[i]
            result._data[i] = ((word << 1) | carry) & 0xFFFFFFFF
            carry = (word >> 31) & 1

        return result

    def hamming_weight(self) -> int:
        """
        Count number of 1 bits.

        Returns:
            Number of bits set to 1
        """
        count = 0
        for word in self._data:
            # Fast popcount using bit manipulation
            n = word
            while n:
                count += n & 1
                n >>= 1
        return count

    def equals(self, other: "BitVector") -> bool:
        """
        Check equality with another bit vector.

        Args:
            other: Other bit vector

        Returns:
            True if equal, False otherwise
        """
        if self.length != other.length:
            return False

        for i in range(self.num_words):
            if self._data[i] != other._data[i]:
                return False

        return True
