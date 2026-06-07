"""
POCKET+ decompression algorithm implementation.

Implements CCSDS 124.0-B-1 decompression (inverse of Section 5.3):
- Bit reader for parsing compressed packets
- COUNT and RLE decoding
- Packet decompression and mask reconstruction

MicroPython compatible - no typing module imports.
"""

# Import for type hints only
if False:  # noqa: SIM108
    from pocketplus.bitvector import BitVector

# Constants
MAX_ROBUSTNESS = 7
MAX_PACKET_LENGTH = 65535  # CCSDS 124.0-B-1 section 3.2: 1 <= F <= 2^16 - 1


class Decompressor:
    """POCKET+ decompressor state and operations."""

    def __init__(
        self,
        packet_length: int,
        robustness: int = 1,
        initial_mask: "BitVector | None" = None,
    ) -> None:
        """
        Initialize decompressor.

        Args:
            packet_length: F - Output vector length in bits
            robustness: Rt - Base robustness level (0-7)
            initial_mask: M0 - Initial mask vector (None = all zeros)
        """
        from pocketplus.bitvector import BitVector

        # CCSDS 124.0-B-1 section 3.2: 1 <= F <= 2^16 - 1
        if packet_length < 1 or packet_length > MAX_PACKET_LENGTH:
            raise ValueError(
                "packet_length must be in 1.." + str(MAX_PACKET_LENGTH) + " bits"
            )

        self.F = packet_length
        self.robustness = min(robustness, MAX_ROBUSTNESS)

        # Initialize bit vectors
        self.mask = BitVector(packet_length)
        self.initial_mask = BitVector(packet_length)
        self.prev_output = BitVector(packet_length)
        self.Xt = BitVector(packet_length)  # Positive changes tracker

        # Set initial mask if provided
        if initial_mask is not None:
            self.initial_mask.copy_from(initial_mask)
            self.mask.copy_from(initial_mask)

        # Time index
        self.t = 0

    def reset(self) -> None:
        """Reset decompressor to initial state."""
        self.t = 0
        self.mask.copy_from(self.initial_mask)
        self.prev_output.zero()
        self.Xt.zero()

    def decompress_packet(self, reader: "BitReader") -> "BitVector":
        """
        Decompress a single compressed packet.

        Args:
            reader: BitReader positioned at packet start

        Returns:
            Decompressed output packet (length F)
        """
        from pocketplus.bitvector import BitVector
        from pocketplus.decode import bit_insert, count_decode, rle_decode

        output = BitVector(self.F)

        # Copy previous output as prediction base
        output.copy_from(self.prev_output)

        # Clear positive changes tracker
        self.Xt.zero()

        # ================================================================
        # Parse ht: Mask change information
        # ht = RLE(Xt) || BIT4(Vt) || et || kt || ct || dt
        # ================================================================

        # Decode RLE(Xt) - mask changes
        Xt = rle_decode(reader, self.F)

        # Read BIT4(Vt) - effective robustness
        Vt = reader.read_bits(4)

        # Process et, kt, ct if Vt > 0 and there are changes
        ct = 0
        change_count = Xt.hamming_weight()

        if Vt > 0 and change_count > 0:
            # Read et
            et = reader.read_bit()

            if et == 1:
                # Read kt - determines positive/negative updates
                # kt has one bit per change in Xt
                kt_bits = []

                # Read kt bits (forward order)
                for i in range(self.F):
                    if Xt.get_bit(i):
                        bit_val = reader.read_bit()
                        kt_bits.append(bit_val)

                # Apply mask updates based on kt
                kt_idx = 0
                for i in range(self.F):
                    if Xt.get_bit(i):
                        # kt=1 means positive update (mask becomes 0)
                        # kt=0 means negative update (mask becomes 1)
                        if kt_bits[kt_idx]:
                            self.mask.set_bit(i, 0)
                            self.Xt.set_bit(i, 1)  # Track positive change
                        else:
                            self.mask.set_bit(i, 1)
                        kt_idx += 1

                # Read ct
                ct = reader.read_bit()
            else:
                # et = 0: all updates are negative (mask bits become 1)
                for i in range(self.F):
                    if Xt.get_bit(i):
                        self.mask.set_bit(i, 1)

        elif Vt == 0 and change_count > 0:
            # Vt = 0: toggle mask bits at change positions
            for i in range(self.F):
                if Xt.get_bit(i):
                    current_val = self.mask.get_bit(i)
                    self.mask.set_bit(i, 1 if current_val == 0 else 0)

        # Read dt
        dt = reader.read_bit()

        # ================================================================
        # Parse qt: Optional full mask
        # ================================================================

        rt = 0

        # dt=1 means both ft=0 and rt=0 (optimization per CCSDS Eq. 13)
        # dt=0 means we need to read ft and rt from the stream
        if dt == 0:
            # Read ft flag
            ft = reader.read_bit()

            if ft == 1:
                # Full mask follows: decode RLE(M XOR (M<<))
                mask_diff = rle_decode(reader, self.F)

                # Reverse the horizontal XOR to get the actual mask.
                # HXOR encoding: HXOR[i] = M[i] XOR M[i+1], with HXOR[F-1] = M[F-1]
                # Reversal: start from LSB (position F-1) and work towards MSB
                # M[F-1] = HXOR[F-1] (just copy)
                # M[i] = HXOR[i] XOR M[i+1] for i < F-1

                # Copy LSB bit directly (position F-1)
                current = mask_diff.get_bit(self.F - 1)
                self.mask.set_bit(self.F - 1, current)

                # Process remaining bits from F-2 down to 0
                for i in range(self.F - 2, -1, -1):
                    hxor_bit = mask_diff.get_bit(i)
                    # M[i] = HXOR[i] XOR M[i+1] = HXOR[i] XOR current
                    current = hxor_bit ^ current
                    self.mask.set_bit(i, current)

            # Read rt flag
            rt = reader.read_bit()

        if rt == 1:
            # Full packet follows: COUNT(F) || It
            _ = count_decode(reader)  # Read and discard packet length

            # Read full packet
            for i in range(self.F):
                bit = reader.read_bit()
                output.set_bit(i, bit)
        else:
            # Compressed: extract unpredictable bits
            if ct == 1 and Vt > 0:
                # BE(It, (Xt OR Mt))
                extraction_mask = self.mask.or_(self.Xt)
            else:
                # BE(It, Mt)
                extraction_mask = self.mask.copy()

            # Insert unpredictable bits
            bit_insert(reader, output, extraction_mask)

        # ================================================================
        # Update state for next cycle
        # ================================================================

        self.prev_output.copy_from(output)
        self.t += 1

        return output


# Import BitReader at module level for type hints
if False:  # noqa: SIM108
    from pocketplus.bitreader import BitReader


def decompress(
    data: bytes,
    packet_size: int,
    robustness: int = 1,
    initial_mask: "BitVector | None" = None,
) -> bytes:
    """
    Decompress data using POCKET+ algorithm.

    High-level API that handles:
    - Sequential packet decompression
    - Byte boundary alignment between packets
    - Output accumulation

    Args:
        data: Compressed input bytes
        packet_size: Packet length in bits
        robustness: Rt - Base robustness level (0-7)
        initial_mask: M0 initial mask (None = all zeros)

    Returns:
        Decompressed data bytes
    """
    from pocketplus.bitreader import BitReader

    # Initialize decompressor
    decomp = Decompressor(
        packet_length=packet_size,
        robustness=robustness,
        initial_mask=initial_mask,
    )

    # Initialize bit reader
    reader = BitReader(data)

    # Output accumulation
    output_bytes = bytearray()

    # Decompress packets until input exhausted
    while reader.remaining > 0:
        output = decomp.decompress_packet(reader)

        # Append to output
        output_bytes.extend(output.to_bytes())

        # Align to byte boundary for next packet
        reader.align_byte()

    return bytes(output_bytes)
