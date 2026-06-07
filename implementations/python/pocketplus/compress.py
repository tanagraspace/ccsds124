"""
POCKET+ compression algorithm implementation.

Implements CCSDS 124.0-B-1 Section 5.3 (Encoding Step):
- Compressor initialization and state management
- Main compression algorithm
- Output packet encoding: ot = ht || qt || ut

MicroPython compatible - no typing module imports.
"""

# Import for type hints only
if False:  # noqa: SIM108
    from pocketplus.bitvector import BitVector

# Constants
MAX_HISTORY = 16
MAX_VT_HISTORY = 16
MAX_ROBUSTNESS = 7
MAX_PACKET_LENGTH = 65535  # CCSDS 124.0-B-1 section 3.2: 1 <= F <= 2^16 - 1


class CompressParams:
    """Compression parameters for a single packet."""

    def __init__(
        self,
        min_robustness: int = 1,
        new_mask_flag: bool = False,
        send_mask_flag: bool = False,
        uncompressed_flag: bool = False,
    ) -> None:
        """
        Initialize compression parameters.

        Args:
            min_robustness: Rt - Minimum robustness level (0-7)
            new_mask_flag: pt - Update mask from build vector
            send_mask_flag: ft - Include mask in output
            uncompressed_flag: rt - Send uncompressed
        """
        self.min_robustness = min_robustness
        self.new_mask_flag = new_mask_flag
        self.send_mask_flag = send_mask_flag
        self.uncompressed_flag = uncompressed_flag


class Compressor:
    """POCKET+ compressor state and operations."""

    def __init__(
        self,
        packet_length: int,
        robustness: int = 1,
        initial_mask: "BitVector | None" = None,
        pt_limit: int = 0,
        ft_limit: int = 0,
        rt_limit: int = 0,
    ) -> None:
        """
        Initialize compressor.

        Args:
            packet_length: F - Input vector length in bits
            robustness: Rt - Base robustness level (0-7)
            initial_mask: M0 - Initial mask vector (None = all zeros)
            pt_limit: New mask period (0 = manual control)
            ft_limit: Send mask period (0 = manual control)
            rt_limit: Uncompressed period (0 = manual control)
        """
        from pocketplus.bitvector import BitVector

        # CCSDS 124.0-B-1 section 3.2: 1 <= F <= 2^16 - 1
        if packet_length < 1 or packet_length > MAX_PACKET_LENGTH:
            raise ValueError(
                "packet_length must be in 1.." + str(MAX_PACKET_LENGTH) + " bits"
            )

        self.F = packet_length
        self.robustness = min(robustness, MAX_ROBUSTNESS)

        # Period limits for automatic parameter management
        self.pt_limit = pt_limit
        self.ft_limit = ft_limit
        self.rt_limit = rt_limit

        # Initialize bit vectors
        self.mask = BitVector(packet_length)
        self.prev_mask = BitVector(packet_length)
        self.build = BitVector(packet_length)
        self.prev_input = BitVector(packet_length)
        self.initial_mask = BitVector(packet_length)

        # Set initial mask if provided
        if initial_mask is not None:
            self.initial_mask.copy_from(initial_mask)
            self.mask.copy_from(initial_mask)

        # Change history (circular buffer)
        self.change_history = [BitVector(packet_length) for _ in range(MAX_HISTORY)]
        self.history_index = 0

        # Flag history for ct calculation
        self.new_mask_flag_history = [0] * MAX_VT_HISTORY
        self.flag_history_index = 0

        # Time index
        self.t = 0

        # Countdown counters
        self.pt_counter = pt_limit
        self.ft_counter = ft_limit
        self.rt_counter = rt_limit

    def reset(self) -> None:
        """Reset compressor to initial state."""
        self.t = 0
        self.history_index = 0

        # Reset mask to initial
        self.mask.copy_from(self.initial_mask)
        self.prev_mask.zero()
        self.build.zero()
        self.prev_input.zero()

        # Clear change history
        for ch in self.change_history:
            ch.zero()

        # Reset countdown counters
        self.pt_counter = self.pt_limit
        self.ft_counter = self.ft_limit
        self.rt_counter = self.rt_limit

    def compress_packet(
        self, input_vec: "BitVector", params: "CompressParams | None" = None
    ) -> bytes:
        """
        Compress a single input packet.

        Args:
            input_vec: Input packet It (must be length F)
            params: Compression parameters (None = use defaults)

        Returns:
            Compressed output bytes
        """
        from pocketplus.bitbuffer import BitBuffer
        from pocketplus.bitvector import BitVector
        from pocketplus.encode import (
            bit_extract,
            bit_extract_forward,
            count_encode,
            rle_encode,
        )
        from pocketplus.mask import compute_change, update_build, update_mask

        # Use default params if none provided
        if params is None:
            params = CompressParams(min_robustness=self.robustness)

        output = BitBuffer()

        # ================================================================
        # STEP 1: Update Mask and Build Vectors (CCSDS Section 4)
        # ================================================================

        # Save previous mask
        prev_mask = self.mask.copy()

        # Save previous build
        prev_build = self.build.copy()

        # Update build vector (Equation 6)
        if self.t > 0:
            update_build(
                self.build,
                input_vec,
                self.prev_input,
                params.new_mask_flag,
                self.t,
            )

        # Update mask vector (Equation 7)
        if self.t > 0:
            update_mask(
                self.mask,
                input_vec,
                self.prev_input,
                prev_build,
                params.new_mask_flag,
            )

        # Compute change vector (Equation 8)
        change = BitVector(self.F)
        compute_change(change, self.mask, prev_mask, self.t)

        # Store change in history (circular buffer)
        self.change_history[self.history_index].copy_from(change)

        # ================================================================
        # STEP 2: Encode Output Packet (CCSDS Section 5.3)
        # ot = ht || qt || ut
        # ================================================================

        # Calculate Xt (robustness window)
        Xt = compute_robustness_window(self, change)

        # Calculate Vt (effective robustness)
        Vt = compute_effective_robustness(self, change)

        # Calculate dt flag
        dt = 1 if (not params.send_mask_flag and not params.uncompressed_flag) else 0

        # ================================================================
        # Component ht: Mask change information
        # ht = RLE(Xt) || BIT4(Vt) || et || kt || ct || dt
        # ================================================================

        # 1. RLE(Xt) - Run-length encode the robustness window
        rle_encode(output, Xt)

        # 2. BIT4(Vt) - 4-bit effective robustness level
        for i in range(3, -1, -1):
            output.append_bit((Vt >> i) & 1)

        # 3. et, kt, ct - Only if Vt > 0 and there are mask changes
        if Vt > 0 and Xt.hamming_weight() > 0:
            # Calculate et
            et = has_positive_updates(Xt, self.mask)
            output.append_bit(et)

            if et:
                # kt - Output '1' for positive updates (mask=0), '0' for negative
                # Extract INVERTED mask values in forward order
                inverted_mask = BitVector(self.F)
                for j in range(self.mask.length):
                    mask_bit = self.mask.get_bit(j)
                    inverted_mask.set_bit(j, 1 if mask_bit == 0 else 0)

                bit_extract_forward(output, inverted_mask, Xt)

                # Calculate and encode ct
                ct = compute_ct_flag(self, Vt, params.new_mask_flag)
                output.append_bit(ct)

        # 4. dt - Flag indicating if both ft and rt are zero
        output.append_bit(dt)

        # ================================================================
        # Component qt: Optional full mask
        # qt = empty if dt=1, '1' || RLE(<(Mt XOR (Mt<<))>) if ft=1, '0' otherwise
        # ================================================================

        if dt == 0:  # Only if dt = 0
            if params.send_mask_flag:
                output.append_bit(1)  # Flag: mask follows

                # Encode mask as RLE(M XOR (M<<))
                mask_shifted = self.mask.left_shift()
                mask_diff = self.mask.xor(mask_shifted)
                rle_encode(output, mask_diff)
            else:
                output.append_bit(0)  # Flag: no mask

        # ================================================================
        # Component ut: Unpredictable bits or full input
        # ================================================================

        if params.uncompressed_flag:
            # '1' || COUNT(F) || It
            output.append_bit(1)  # Flag: full input follows
            count_encode(output, self.F)
            output.append_bitvector(input_vec)
        else:
            if dt == 0:
                # '0' || BE(...)
                output.append_bit(0)  # Flag: compressed

            # Determine extraction mask based on ct
            ct = compute_ct_flag(self, Vt, params.new_mask_flag)

            if ct and Vt > 0:
                # BE(It, (Xt OR Mt)) - extract bits where mask OR changes are set
                extraction_mask = self.mask.or_(Xt)
                bit_extract(output, input_vec, extraction_mask)
            else:
                # BE(It, Mt) - extract only unpredictable bits
                bit_extract(output, input_vec, self.mask)

        # ================================================================
        # STEP 3: Update State for Next Cycle
        # ================================================================

        # Save current input and mask as previous for next iteration
        self.prev_input.copy_from(input_vec)
        self.prev_mask.copy_from(self.mask)

        # Track new_mask_flag for ct calculation
        self.new_mask_flag_history[self.flag_history_index] = (
            1 if params.new_mask_flag else 0
        )
        self.flag_history_index = (self.flag_history_index + 1) % MAX_VT_HISTORY

        # Advance time
        self.t += 1

        # Advance history index (circular buffer)
        self.history_index = (self.history_index + 1) % MAX_HISTORY

        return output.to_bytes()


def compute_robustness_window(
    comp: Compressor, current_change: "BitVector"
) -> "BitVector":
    """
    Compute robustness window Xt.

    Xt = <(Dt-Rt OR Dt-Rt+1 OR ... OR Dt)>
    where <a> means reverse the bit order.

    Args:
        comp: Compressor with change history
        current_change: Current change vector Dt

    Returns:
        Robustness window Xt
    """
    from pocketplus.bitvector import BitVector

    Xt = BitVector(comp.F)

    if comp.robustness == 0 or comp.t == 0:
        # Xt = Dt (no reversal - RLE processes LSB to MSB directly)
        Xt.copy_from(current_change)
    else:
        # OR together changes from t-Rt to t
        combined = current_change.copy()

        # Determine how many historical changes to include
        num_changes = min(comp.t, comp.robustness)

        # OR with historical changes (going backwards from current)
        for i in range(1, num_changes + 1):
            # Calculate index of change from i iterations ago
            hist_idx = (comp.history_index + MAX_HISTORY - i) % MAX_HISTORY
            combined = combined.or_(comp.change_history[hist_idx])

        # Don't reverse - RLE will process from LSB to MSB directly
        Xt.copy_from(combined)

    return Xt


def compute_effective_robustness(comp: Compressor, current_change: "BitVector") -> int:
    """
    Compute effective robustness Vt.

    Vt = Rt + Ct (CCSDS Section 5.3.2.2)
    where Ct = number of consecutive iterations with no mask changes

    Args:
        comp: Compressor with change history
        current_change: Current change vector Dt

    Returns:
        Effective robustness Vt (0-15)
    """
    _ = current_change  # Unused - Ct is computed from history

    Rt = comp.robustness
    Vt = Rt

    # For t > Rt, compute Ct
    if comp.t > Rt:
        # Count backwards through history starting from Rt+1 positions back
        Ct = 0

        for i in range(Rt + 1, min(16, comp.t + 1)):
            hist_idx = (comp.history_index + MAX_HISTORY - i) % MAX_HISTORY
            if comp.change_history[hist_idx].hamming_weight() > 0:
                break  # Found a change, stop counting
            Ct += 1
            if Ct >= 15 - Rt:
                break  # Cap at maximum Ct value

        Vt = Rt + Ct
        # Note: Vt cannot exceed 15 since Ct is capped at (15 - Rt) above

    return Vt


def has_positive_updates(Xt: "BitVector", mask: "BitVector") -> int:
    """
    Check for positive mask updates (et flag).

    et = 1 if any changed bits (in Xt) are predictable (mask bit = 0)

    Args:
        Xt: Robustness window
        mask: Current mask Mt

    Returns:
        1 if positive updates exist, 0 otherwise
    """
    # Word-level check: Xt AND (NOT mask) - any bit set means positive update
    for i in range(min(Xt.num_words, mask.num_words)):
        if Xt._data[i] & ~mask._data[i] & 0xFFFFFFFF:
            return 1
    return 0


def compute_ct_flag(comp: Compressor, Vt: int, current_new_mask_flag: bool) -> int:
    """
    Compute ct flag for multiple mask updates.

    ct = 1 if new_mask_flag was set 2+ times in last Vt+1 iterations
    (including current packet)

    Args:
        comp: Compressor with flag history
        Vt: Effective robustness level
        current_new_mask_flag: Current packet's pt value

    Returns:
        1 if multiple updates, 0 otherwise
    """
    if Vt == 0:
        return 0

    # Count how many times new_mask_flag was set
    count = 0

    # Include current packet's flag
    if current_new_mask_flag:
        count += 1

    # Check history for Vt previous entries
    iterations_to_check = min(Vt, comp.t)

    for i in range(iterations_to_check):
        # Calculate history index going backwards from previous
        hist_idx = (comp.flag_history_index + MAX_VT_HISTORY - 1 - i) % MAX_VT_HISTORY
        if comp.new_mask_flag_history[hist_idx]:
            count += 1

    return 1 if count >= 2 else 0


def compress(
    data: bytes,
    packet_size: int,
    robustness: int = 1,
    pt_limit: int = 10,
    ft_limit: int = 20,
    rt_limit: int = 50,
    initial_mask: "BitVector | None" = None,
) -> bytes:
    """
    Compress data using POCKET+ algorithm.

    High-level API that handles:
    - Splitting input into F-bit packets
    - Automatic pt/ft/rt parameter management
    - CCSDS init phase (first Rt+1 packets)
    - Output accumulation with byte padding

    Args:
        data: Input data bytes (must be multiple of packet_size/8)
        packet_size: Packet length in bits
        robustness: Rt - Base robustness level (0-7)
        pt_limit: New mask period (default 10)
        ft_limit: Send mask period (default 20)
        rt_limit: Uncompressed period (default 50)
        initial_mask: M0 initial mask (None = all zeros)

    Returns:
        Compressed data bytes
    """
    from pocketplus.bitvector import BitVector

    # Calculate packet size in bytes
    packet_bytes = (packet_size + 7) // 8

    # Verify input size is a multiple of packet size
    if len(data) % packet_bytes != 0:
        raise ValueError(
            f"Input size {len(data)} is not a multiple of packet size {packet_bytes}"
        )

    # Calculate number of packets
    num_packets = len(data) // packet_bytes

    # Initialize compressor
    comp = Compressor(
        packet_length=packet_size,
        robustness=robustness,
        initial_mask=initial_mask,
        pt_limit=pt_limit,
        ft_limit=ft_limit,
        rt_limit=rt_limit,
    )

    # Output accumulation
    output_bytes = bytearray()

    # Process each packet
    for i in range(num_packets):
        # Load packet data
        input_vec = BitVector(packet_size)
        packet_data = data[i * packet_bytes : (i + 1) * packet_bytes]
        input_vec.from_bytes(packet_data)

        # Compute parameters
        params = CompressParams(min_robustness=robustness)

        # Automatic parameter management
        if pt_limit > 0 and ft_limit > 0 and rt_limit > 0:
            if i == 0:
                # First packet: fixed init values
                params.send_mask_flag = True
                params.uncompressed_flag = True
                params.new_mask_flag = False
            else:
                # Packets 1+: check and update countdown counters
                # ft counter
                if comp.ft_counter == 1:
                    params.send_mask_flag = True
                    comp.ft_counter = ft_limit
                else:
                    comp.ft_counter -= 1
                    params.send_mask_flag = False

                # pt counter
                if comp.pt_counter == 1:
                    params.new_mask_flag = True
                    comp.pt_counter = pt_limit
                else:
                    comp.pt_counter -= 1
                    params.new_mask_flag = False

                # rt counter
                if comp.rt_counter == 1:
                    params.uncompressed_flag = True
                    comp.rt_counter = rt_limit
                else:
                    comp.rt_counter -= 1
                    params.uncompressed_flag = False

                # Override for remaining init packets
                # CCSDS requires first Rt+1 packets to have ft=1, rt=1, pt=0
                if i <= robustness:
                    params.send_mask_flag = True
                    params.uncompressed_flag = True
                    params.new_mask_flag = False

        # Compress packet
        packet_output = comp.compress_packet(input_vec, params)
        output_bytes.extend(packet_output)

    return bytes(output_bytes)
