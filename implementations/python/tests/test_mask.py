"""Tests for mask operations (build, mask, change vectors)."""

from ccsds124.bitvector import BitVector
from ccsds124.mask import compute_change, update_build, update_mask


class TestUpdateBuild:
    """Test build vector update (CCSDS Equation 6)."""

    def test_update_build_t0_resets(self) -> None:
        """Test build is reset to zero at t=0."""
        build = BitVector(8)
        build.from_bytes(bytes([0xFF]))  # Pre-fill with ones

        input_vec = BitVector(8)
        input_vec.from_bytes(bytes([0xAA]))
        prev_input = BitVector(8)

        update_build(build, input_vec, prev_input, new_mask_flag=False, t=0)

        # Should be all zeros
        assert build.to_bytes() == bytes([0x00])

    def test_update_build_new_mask_resets(self) -> None:
        """Test build is reset when new_mask_flag is set."""
        build = BitVector(8)
        build.from_bytes(bytes([0xFF]))

        input_vec = BitVector(8)
        input_vec.from_bytes(bytes([0xAA]))
        prev_input = BitVector(8)
        prev_input.from_bytes(bytes([0x55]))

        update_build(build, input_vec, prev_input, new_mask_flag=True, t=5)

        # Should be all zeros despite t > 0
        assert build.to_bytes() == bytes([0x00])

    def test_update_build_accumulates_changes(self) -> None:
        """Test build accumulates XOR of inputs."""
        build = BitVector(8)  # Starts at zero

        input_vec = BitVector(8)
        input_vec.from_bytes(bytes([0b11110000]))
        prev_input = BitVector(8)
        prev_input.from_bytes(bytes([0b11000000]))

        # At t=1, build = (input XOR prev) OR build_prev
        # = (0b11110000 XOR 0b11000000) OR 0b00000000
        # = 0b00110000 OR 0b00000000
        # = 0b00110000
        update_build(build, input_vec, prev_input, new_mask_flag=False, t=1)

        assert build.to_bytes() == bytes([0b00110000])

    def test_update_build_or_with_previous(self) -> None:
        """Test build ORs with previous build value."""
        build = BitVector(8)
        build.from_bytes(bytes([0b00001111]))  # Previous build

        input_vec = BitVector(8)
        input_vec.from_bytes(bytes([0b11110000]))
        prev_input = BitVector(8)
        prev_input.from_bytes(bytes([0b11000000]))

        # build = (0b11110000 XOR 0b11000000) OR 0b00001111
        # = 0b00110000 OR 0b00001111
        # = 0b00111111
        update_build(build, input_vec, prev_input, new_mask_flag=False, t=2)

        assert build.to_bytes() == bytes([0b00111111])


class TestUpdateMask:
    """Test mask vector update (CCSDS Equation 7)."""

    def test_update_mask_normal_operation(self) -> None:
        """Test mask update without new_mask_flag."""
        mask = BitVector(8)
        mask.from_bytes(bytes([0b00001111]))  # Previous mask

        input_vec = BitVector(8)
        input_vec.from_bytes(bytes([0b11110000]))
        prev_input = BitVector(8)
        prev_input.from_bytes(bytes([0b11000000]))
        build_prev = BitVector(8)  # Not used when new_mask_flag=False

        # mask = (input XOR prev) OR mask_prev
        # = (0b11110000 XOR 0b11000000) OR 0b00001111
        # = 0b00110000 OR 0b00001111
        # = 0b00111111
        update_mask(mask, input_vec, prev_input, build_prev, new_mask_flag=False)

        assert mask.to_bytes() == bytes([0b00111111])

    def test_update_mask_with_new_mask_flag(self) -> None:
        """Test mask update with new_mask_flag set."""
        mask = BitVector(8)
        mask.from_bytes(bytes([0xFF]))  # Previous mask (should be replaced)

        input_vec = BitVector(8)
        input_vec.from_bytes(bytes([0b11110000]))
        prev_input = BitVector(8)
        prev_input.from_bytes(bytes([0b11000000]))
        build_prev = BitVector(8)
        build_prev.from_bytes(bytes([0b00001111]))

        # mask = (input XOR prev) OR build_prev
        # = (0b11110000 XOR 0b11000000) OR 0b00001111
        # = 0b00110000 OR 0b00001111
        # = 0b00111111
        update_mask(mask, input_vec, prev_input, build_prev, new_mask_flag=True)

        assert mask.to_bytes() == bytes([0b00111111])

    def test_update_mask_no_changes(self) -> None:
        """Test mask when inputs are identical."""
        mask = BitVector(8)
        mask.from_bytes(bytes([0b10101010]))

        input_vec = BitVector(8)
        input_vec.from_bytes(bytes([0xAA]))
        prev_input = BitVector(8)
        prev_input.from_bytes(bytes([0xAA]))  # Same as input
        build_prev = BitVector(8)

        # mask = (input XOR prev) OR mask_prev
        # = 0x00 OR 0b10101010
        # = 0b10101010 (unchanged)
        update_mask(mask, input_vec, prev_input, build_prev, new_mask_flag=False)

        assert mask.to_bytes() == bytes([0b10101010])


class TestComputeChange:
    """Test change vector computation (CCSDS Equation 8)."""

    def test_compute_change_t0(self) -> None:
        """Test change at t=0 is zero (CCSDS Eq. 8: D0 = 0)."""
        change = BitVector(8)
        change.from_bytes(bytes([0xFF]))  # Should be overwritten
        mask = BitVector(8)
        mask.from_bytes(bytes([0b11110000]))
        prev_mask = BitVector(8)
        prev_mask.from_bytes(bytes([0xFF]))  # Should be ignored at t=0

        compute_change(change, mask, prev_mask, t=0)

        # CCSDS Eq. 8: D0 = 0 regardless of M0 (no change at initialization)
        assert change.to_bytes() == bytes([0x00])

    def test_compute_change_t_positive(self) -> None:
        """Test change at t>0 is XOR of masks."""
        change = BitVector(8)
        mask = BitVector(8)
        mask.from_bytes(bytes([0b11110000]))
        prev_mask = BitVector(8)
        prev_mask.from_bytes(bytes([0b11001100]))

        compute_change(change, mask, prev_mask, t=5)

        # change = mask XOR prev_mask
        # = 0b11110000 XOR 0b11001100
        # = 0b00111100
        assert change.to_bytes() == bytes([0b00111100])

    def test_compute_change_no_change(self) -> None:
        """Test change is zero when masks are identical."""
        change = BitVector(8)
        mask = BitVector(8)
        mask.from_bytes(bytes([0xAA]))
        prev_mask = BitVector(8)
        prev_mask.from_bytes(bytes([0xAA]))

        compute_change(change, mask, prev_mask, t=3)

        assert change.to_bytes() == bytes([0x00])


class TestMaskIntegration:
    """Test mask operations working together."""

    def test_build_mask_change_sequence(self) -> None:
        """Test a sequence of build/mask/change operations."""
        length = 16

        # Initialize vectors
        build = BitVector(length)
        mask = BitVector(length)

        # Input sequence
        inputs = [
            bytes([0x00, 0x00]),  # t=0
            bytes([0xFF, 0x00]),  # t=1: changed high byte
            bytes([0xFF, 0xFF]),  # t=2: changed low byte
            bytes([0xFF, 0xFF]),  # t=3: no change
        ]

        for t, input_bytes in enumerate(inputs):
            input_vec = BitVector(length)
            input_vec.from_bytes(input_bytes)

            if t == 0:
                prev_input = BitVector(length)  # All zeros
            else:
                prev_input = BitVector(length)
                prev_input.from_bytes(inputs[t - 1])

            # Update build
            update_build(build, input_vec, prev_input, new_mask_flag=False, t=t)

            # Save previous mask
            prev_mask_copy = mask.copy()

            # Update mask
            update_mask(mask, input_vec, prev_input, build, new_mask_flag=False)

            # Compute change
            change = BitVector(length)
            compute_change(change, mask, prev_mask_copy, t=t)

        # After sequence, mask should have accumulated all changes
        # High byte changed at t=1, low byte at t=2
        assert mask.to_bytes() == bytes([0xFF, 0xFF])

    def test_new_mask_flag_resets_build(self) -> None:
        """Test that new_mask_flag properly resets the build vector."""
        length = 8
        build = BitVector(length)
        build.from_bytes(bytes([0xFF]))  # Accumulated build

        mask = BitVector(length)
        mask.from_bytes(bytes([0xAA]))

        input_vec = BitVector(length)
        input_vec.from_bytes(bytes([0x55]))
        prev_input = BitVector(length)
        prev_input.from_bytes(bytes([0x00]))

        # With new_mask_flag, build should reset
        update_build(build, input_vec, prev_input, new_mask_flag=True, t=10)
        assert build.to_bytes() == bytes([0x00])

        # Mask should use build_prev (the old build value before reset)
        # But since we pass the reset build, it uses zeros
        build_prev = BitVector(length)  # Simulating saved build before reset
        build_prev.from_bytes(bytes([0xFF]))
        update_mask(mask, input_vec, prev_input, build_prev, new_mask_flag=True)

        # mask = (0x55 XOR 0x00) OR 0xFF = 0x55 OR 0xFF = 0xFF
        assert mask.to_bytes() == bytes([0xFF])
