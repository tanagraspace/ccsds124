"""
POCKET+ decoding functions (COUNT, RLE, bit insert).

Implements CCSDS 124.0-B-1 decoding schemes (inverse of encoding):
- Counter Decoding (COUNT) - inverse of Section 5.2.2
- Run-Length Decoding (RLE) - inverse of Section 5.2.3
- Bit Insertion - inverse of Section 5.2.4 BE

MicroPython compatible - no typing module imports.
"""

# Import for type hints only
if False:  # noqa: SIM108
    from pocketplus.bitreader import BitReader
    from pocketplus.bitvector import BitVector


def count_decode(reader: "BitReader") -> int:
    """
    Decode a COUNT-encoded value.

    Decoding rules (inverse of CCSDS Equation 9):
    - '0' → 1
    - '10' → 0 (terminator)
    - '110' + BIT5 → value + 2 (range 2-33)
    - '111' + BIT_E → value + 2 (range 34+)

    Args:
        reader: BitReader to read encoded bits from

    Returns:
        Decoded positive integer (0 for terminator, 1+ for values)
    """
    # Read first bit
    first_bit = reader.read_bit()

    if first_bit == 0:
        # Case 1: '0' → 1
        return 1

    # First bit was 1, read second bit
    second_bit = reader.read_bit()

    if second_bit == 0:
        # Case 2: '10' → terminator (0)
        return 0

    # First two bits were '11', read third bit
    third_bit = reader.read_bit()

    if third_bit == 0:
        # Case 3: '110' + BIT5 → value + 2 (range 2-33)
        value = reader.read_bits(5)
        return value + 2

    else:
        # Case 4: '111' + BIT_E → value + 2 (range 34+)
        # Need to determine E by reading bits until we can decode
        # E = 2*floor(log2(value)+1) - 6
        # We read bits incrementally to find the value

        # Start with 6 bits (minimum for value >= 32)
        e = 6
        value = reader.read_bits(e)

        # Check if we have the right number of bits
        # For value v, E should be 2*floor(log2(v)+1) - 6
        while True:
            # Calculate expected E for this value
            if value == 0:
                expected_e = 0
            else:
                # Find floor(log2(value))
                log_val = 0
                temp = value
                while temp > 1:
                    temp >>= 1
                    log_val += 1
                expected_e = 2 * (log_val + 1) - 6

            if expected_e == e:
                # We have the right number of bits
                break

            # Need more bits
            e += 2
            # Read 2 more bits and shift in
            extra = reader.read_bits(2)
            value = (value << 2) | extra

        return value + 2


def rle_decode(reader: "BitReader", length: int) -> "BitVector":
    """
    Decode an RLE-encoded bit vector.

    Decoding rules (inverse of CCSDS Equation 10):
    - Read COUNT values until terminator (0)
    - Each COUNT value represents position delta to next '1' bit

    Args:
        reader: BitReader to read encoded bits from
        length: Length of the output bit vector

    Returns:
        Decoded BitVector
    """
    from pocketplus.bitvector import BitVector

    result = BitVector(length)

    # Start from end of vector
    position = length

    while True:
        # Decode next count
        count = count_decode(reader)

        if count == 0:
            # Terminator reached
            break

        # Move position back by count. A delta beyond the remaining bit
        # position means the encoding is invalid for this vector length
        # (GOTCHAS #21): reject it instead of silently skipping.
        if count > position:
            raise ValueError("invalid RLE delta: exceeds remaining bit position")
        position -= count

        # Set the bit at this position
        result.set_bit(position, 1)

    return result


def bit_insert(reader: "BitReader", data: "BitVector", mask: "BitVector") -> None:
    """
    Insert bits into data at positions specified by mask (inverse of BE).

    Reads bits from reader and inserts them into data at positions
    where mask has '1' bits. Bits are read in reverse order (highest
    position to lowest) to match BE extraction order.

    Args:
        reader: BitReader to read bits from
        data: BitVector to insert bits into
        mask: BitVector indicating which positions to fill
    """
    # Collect positions of '1' bits in mask
    positions = []
    for i in range(mask.length):
        if mask.get_bit(i):
            positions.append(i)

    # Insert bits in reverse order (highest position to lowest)
    # This matches the extraction order in bit_extract
    for i in range(len(positions) - 1, -1, -1):
        pos = positions[i]
        bit = reader.read_bit()
        data.set_bit(pos, bit)


def bit_insert_forward(
    reader: "BitReader", data: "BitVector", mask: "BitVector"
) -> None:
    """
    Insert bits in forward order (for kt component).

    Same as bit_insert but reads bits from lowest position to highest.

    Args:
        reader: BitReader to read bits from
        data: BitVector to insert bits into
        mask: BitVector indicating which positions to fill
    """
    # Insert bits in forward order
    for i in range(mask.length):
        if mask.get_bit(i):
            bit = reader.read_bit()
            data.set_bit(i, bit)
