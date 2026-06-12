"""
CCSDS 124.0-B-1 encoding functions (COUNT, RLE, BE).

Implements CCSDS 124.0-B-1 Section 5.2 encoding schemes:
- Counter Encoding (COUNT) - Section 5.2.2, Table 5-1, Equation 9
- Run-Length Encoding (RLE) - Section 5.2.3, Equation 10
- Bit Extraction (BE) - Section 5.2.4, Equation 11

MicroPython compatible - no typing module imports.
"""

import math

# Import for type hints only
if False:  # noqa: SIM108
    from ccsds124.bitbuffer import BitBuffer
    from ccsds124.bitvector import BitVector


def count_encode(output: "BitBuffer", a: int) -> None:
    """
    Encode a positive integer using COUNT encoding (CCSDS Equation 9).

    Encoding rules:
    - A = 1 → '0'
    - 2 ≤ A ≤ 33 → '110' + BIT5(A-2)
    - A ≥ 34 → '111' + BIT_E(A-2) where E = 2*floor(log2(A-2)+1) - 6

    Args:
        output: BitBuffer to append encoded bits to
        a: Positive integer to encode (1 to 65535)

    Raises:
        ValueError: If a is out of valid range
    """
    if a < 1 or a > 65535:
        raise ValueError(f"COUNT value {a} out of range [1, 65535]")

    if a == 1:
        # Case 1: A = 1 → '0'
        output.append_bit(0)

    elif a <= 33:
        # Case 2: 2 ≤ A ≤ 33 → '110' + BIT5(A-2)
        output.append_bit(1)
        output.append_bit(1)
        output.append_bit(0)

        # Append 5-bit value of (A-2), MSB first
        value = a - 2
        for i in range(4, -1, -1):
            output.append_bit((value >> i) & 1)

    else:
        # Case 3: A ≥ 34 → '111' + BIT_E(A-2)
        output.append_bit(1)
        output.append_bit(1)
        output.append_bit(1)

        # Calculate E = 2*floor(log2(A-2)+1) - 6
        value = a - 2
        log_val = math.log2(value)
        e = 2 * (int(math.floor(log_val)) + 1) - 6

        # Append E-bit value, MSB first
        for i in range(e - 1, -1, -1):
            output.append_bit((value >> i) & 1)


def rle_encode(output: "BitBuffer", bv: "BitVector") -> None:
    """
    Encode a bit vector using RLE encoding (CCSDS Equation 10).

    RLE(a) = COUNT(C0) || COUNT(C1) || ... || COUNT(C_{H(a)-1}) || '10'

    where Ci = 1 + (count of consecutive '0' bits before i-th '1' bit)
    and H(a) = Hamming weight (number of '1' bits)

    Args:
        output: BitBuffer to append encoded bits to
        bv: BitVector to encode
    """
    # DeBruijn lookup table for fast LSB finding
    debruijn_lookup = [
        1,
        2,
        29,
        3,
        30,
        15,
        25,
        4,
        31,
        23,
        21,
        16,
        26,
        18,
        5,
        9,
        32,
        28,
        14,
        24,
        22,
        20,
        17,
        8,
        27,
        13,
        19,
        7,
        12,
        6,
        11,
        10,
    ]

    # Start from the end of the vector
    old_bit_position = bv.length

    # Process words in reverse order (from high to low)
    for word_idx in range(bv.num_words - 1, -1, -1):
        word_data = bv._data[word_idx]

        # Process all set bits in this word
        while word_data != 0:
            # Isolate the LSB: x = word & -word
            lsb = word_data & (-word_data & 0xFFFFFFFF)

            # Find LSB position using DeBruijn sequence
            debruijn_index = ((lsb * 0x077CB531) & 0xFFFFFFFF) >> 27
            bit_position_in_word = debruijn_lookup[debruijn_index]

            # Count from the other side
            bit_position_in_word = 32 - bit_position_in_word

            # Calculate global bit position
            new_bit_position = (word_idx * 32) + bit_position_in_word

            # Calculate delta (number of zeros + 1)
            delta = old_bit_position - new_bit_position

            # Encode the count
            count_encode(output, delta)

            # Update old position for next iteration
            old_bit_position = new_bit_position

            # Clear the processed bit
            word_data ^= lsb

    # Append terminator '10'
    output.append_bit(1)
    output.append_bit(0)


def bit_extract(output: "BitBuffer", data: "BitVector", mask: "BitVector") -> None:
    """
    Extract bits using BE (Bit Extraction) in reverse order (CCSDS Equation 11).

    BE(a, b) = a_{g_{H(b)-1}} || ... || a_{g_1} || a_{g_0}

    where g_i is the position of the i-th '1' bit in b (MSB to LSB order)

    Extracts bits from 'data' at positions where 'mask' has '1' bits.
    Output order: highest position to lowest (reverse).

    Args:
        output: BitBuffer to append extracted bits to
        data: BitVector to extract bits from
        mask: BitVector indicating which bits to extract

    Raises:
        ValueError: If data and mask have different lengths
    """
    if data.length != mask.length:
        raise ValueError("Data and mask must have same length")

    # Collect positions of '1' bits in mask
    positions = []
    for i in range(mask.length):
        if mask.get_bit(i):
            positions.append(i)

    # Extract bits in reverse order (highest position to lowest)
    for i in range(len(positions) - 1, -1, -1):
        pos = positions[i]
        bit = data.get_bit(pos)
        output.append_bit(bit)


def bit_extract_forward(
    output: "BitBuffer", data: "BitVector", mask: "BitVector"
) -> None:
    """
    Extract bits in forward order (for kt component).

    Same as bit_extract but outputs bits from lowest position to highest.

    Args:
        output: BitBuffer to append extracted bits to
        data: BitVector to extract bits from
        mask: BitVector indicating which bits to extract

    Raises:
        ValueError: If data and mask have different lengths
    """
    if data.length != mask.length:
        raise ValueError("Data and mask must have same length")

    # Collect positions of '1' bits in mask and extract in forward order
    for i in range(mask.length):
        if mask.get_bit(i):
            bit = data.get_bit(i)
            output.append_bit(bit)
