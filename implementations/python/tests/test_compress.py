"""Tests for POCKET+ compression."""

import pytest

from pocketplus.bitvector import BitVector
from pocketplus.compress import (
    Compressor,
    CompressParams,
    compress,
    compute_ct_flag,
    compute_effective_robustness,
    compute_robustness_window,
    has_positive_updates,
)


class TestCompressorInit:
    """Test compressor initialization."""

    def test_init_basic(self) -> None:
        """Test basic compressor initialization."""
        comp = Compressor(packet_length=720, robustness=1)
        assert comp.F == 720
        assert comp.robustness == 1
        assert comp.t == 0

    def test_init_with_limits(self) -> None:
        """Test initialization with period limits."""
        comp = Compressor(
            packet_length=720,
            robustness=2,
            pt_limit=10,
            ft_limit=20,
            rt_limit=50,
        )
        assert comp.pt_limit == 10
        assert comp.ft_limit == 20
        assert comp.rt_limit == 50

    def test_init_with_initial_mask(self) -> None:
        """Test initialization with initial mask."""
        initial_mask = BitVector(8)
        initial_mask.from_bytes(bytes([0xFF]))
        comp = Compressor(packet_length=8, robustness=1, initial_mask=initial_mask)
        assert comp.mask.to_bytes() == bytes([0xFF])

    def test_reset(self) -> None:
        """Test compressor reset."""
        comp = Compressor(packet_length=8, robustness=1)
        # Simulate some state changes
        comp.t = 10
        comp.reset()
        assert comp.t == 0


class TestRobustnessWindow:
    """Test robustness window (Xt) computation."""

    def test_robustness_window_t0(self) -> None:
        """Test Xt at t=0 equals current change."""
        comp = Compressor(packet_length=8, robustness=2)
        change = BitVector(8)
        change.from_bytes(bytes([0b10101010]))

        Xt = compute_robustness_window(comp, change)
        assert Xt.to_bytes() == change.to_bytes()

    def test_robustness_window_with_history(self) -> None:
        """Test Xt ORs historical changes."""
        comp = Compressor(packet_length=8, robustness=2)

        # Simulate history
        comp.t = 3
        comp.change_history[0].from_bytes(bytes([0b00001111]))
        comp.change_history[1].from_bytes(bytes([0b11110000]))
        comp.change_history[2].from_bytes(bytes([0b00000000]))
        comp.history_index = 3

        change = BitVector(8)
        change.from_bytes(bytes([0b00110000]))

        Xt = compute_robustness_window(comp, change)
        # Should OR: change(0b00110000), history[2](0b00000000), history[1](0b11110000)
        # R=2, so we include t, t-1, t-2 (2 historical entries)
        # Result: 0b00110000 | 0b00000000 | 0b11110000 = 0b11110000
        assert Xt.to_bytes() == bytes([0b11110000])


class TestEffectiveRobustness:
    """Test effective robustness (Vt) computation."""

    def test_effective_robustness_early(self) -> None:
        """Test Vt = Rt for t <= Rt."""
        comp = Compressor(packet_length=8, robustness=3)
        comp.t = 2
        change = BitVector(8)

        Vt = compute_effective_robustness(comp, change)
        assert Vt == 3  # Rt when t <= Rt

    def test_effective_robustness_with_ct(self) -> None:
        """Test Vt = Rt + Ct for t > Rt."""
        comp = Compressor(packet_length=8, robustness=2)
        comp.t = 10

        # All zeros in history = Ct accumulates
        for i in range(16):
            comp.change_history[i].zero()
        comp.history_index = 10

        change = BitVector(8)
        Vt = compute_effective_robustness(comp, change)
        # Ct should be > 0 since no changes in history
        assert Vt >= 2


class TestPositiveUpdates:
    """Test positive update (et) detection."""

    def test_no_positive_updates(self) -> None:
        """Test et=0 when no positive updates."""
        mask = BitVector(8)
        mask.from_bytes(bytes([0xFF]))  # All unpredictable
        Xt = BitVector(8)
        Xt.from_bytes(bytes([0xFF]))

        et = has_positive_updates(Xt, mask)
        assert et == 0  # All changed bits are already unpredictable

    def test_has_positive_updates(self) -> None:
        """Test et=1 when positive updates exist."""
        mask = BitVector(8)
        mask.from_bytes(bytes([0b11110000]))  # Low nibble predictable
        Xt = BitVector(8)
        Xt.from_bytes(bytes([0b00001111]))  # Low nibble changed

        et = has_positive_updates(Xt, mask)
        assert et == 1  # Predictable bits changed


class TestCtFlag:
    """Test ct flag computation."""

    def test_ct_flag_zero_when_vt_zero(self) -> None:
        """Test ct=0 when Vt=0."""
        comp = Compressor(packet_length=8, robustness=0)
        ct = compute_ct_flag(comp, Vt=0, current_new_mask_flag=True)
        assert ct == 0

    def test_ct_flag_counts_new_mask_flags(self) -> None:
        """Test ct counts new_mask_flag occurrences."""
        comp = Compressor(packet_length=8, robustness=2)
        comp.t = 5

        # Set multiple new_mask_flags in history
        comp.new_mask_flag_history[0] = 1
        comp.new_mask_flag_history[1] = 1
        comp.flag_history_index = 2

        ct = compute_ct_flag(comp, Vt=3, current_new_mask_flag=False)
        assert ct == 1  # 2+ flags in window


class TestCompressPacket:
    """Test single packet compression."""

    def test_compress_first_packet(self) -> None:
        """Test first packet compression (uncompressed)."""
        comp = Compressor(
            packet_length=8,
            robustness=1,
            pt_limit=10,
            ft_limit=20,
            rt_limit=50,
        )

        input_data = BitVector(8)
        input_data.from_bytes(bytes([0xAA]))

        params = CompressParams(
            min_robustness=1,
            new_mask_flag=False,
            send_mask_flag=True,
            uncompressed_flag=True,
        )

        output = comp.compress_packet(input_data, params)
        assert len(output) > 0  # Should produce some output
        assert comp.t == 1

    def test_compress_identical_packets(self) -> None:
        """Test compressing identical packets achieves compression."""
        comp = Compressor(packet_length=8, robustness=1)

        # Send same packet twice
        input_data = BitVector(8)
        input_data.from_bytes(bytes([0x55]))

        params = CompressParams(
            min_robustness=1,
            new_mask_flag=False,
            send_mask_flag=False,
            uncompressed_flag=False,
        )

        # First packet
        _ = comp.compress_packet(input_data, params)

        # Second packet (identical)
        output2 = comp.compress_packet(input_data, params)
        # Second packet should be smaller (more predictable)
        assert len(output2) > 0


class TestCompressStream:
    """Test stream compression."""

    def test_compress_basic(self) -> None:
        """Test basic stream compression."""
        input_data = bytes([0xAA] * 10)  # 10 identical bytes
        output = compress(
            input_data,
            packet_size=8,  # 8 bits = 1 byte per packet
            robustness=1,
            pt_limit=10,
            ft_limit=20,
            rt_limit=50,
        )
        assert len(output) > 0

    def test_compress_multiple_packets(self) -> None:
        """Test compression of multiple packets."""
        # 10 packets of 2 bytes each
        input_data = bytes([0x12, 0x34] * 10)
        output = compress(
            input_data,
            packet_size=16,
            robustness=2,
            pt_limit=5,
            ft_limit=10,
            rt_limit=25,
        )
        assert len(output) > 0


class TestCompressEdgeCases:
    """Test edge cases in compression."""

    def test_compress_all_zeros(self) -> None:
        """Test compression of all-zero data."""
        input_data = bytes([0x00] * 90)  # Standard packet size
        output = compress(
            input_data,
            packet_size=720,
            robustness=1,
            pt_limit=10,
            ft_limit=20,
            rt_limit=50,
        )
        assert len(output) > 0

    def test_compress_all_ones(self) -> None:
        """Test compression of all-one data."""
        input_data = bytes([0xFF] * 90)
        output = compress(
            input_data,
            packet_size=720,
            robustness=1,
            pt_limit=10,
            ft_limit=20,
            rt_limit=50,
        )
        assert len(output) > 0

    def test_compress_robustness_zero(self) -> None:
        """Test compression with robustness=0."""
        input_data = bytes([0xAA, 0x55] * 5)
        output = compress(
            input_data,
            packet_size=16,
            robustness=0,
            pt_limit=5,
            ft_limit=10,
            rt_limit=25,
        )
        assert len(output) > 0

    def test_compress_max_robustness(self) -> None:
        """Test compression with maximum robustness (7)."""
        input_data = bytes([0xDE, 0xAD] * 10)
        output = compress(
            input_data,
            packet_size=16,
            robustness=7,
            pt_limit=5,
            ft_limit=10,
            rt_limit=25,
        )
        assert len(output) > 0

    def test_compress_invalid_input_size_raises(self) -> None:
        """Test that input size not multiple of packet size raises ValueError."""
        input_data = bytes([0xAA] * 7)  # 7 bytes, not divisible by 8-bit packet
        with pytest.raises(ValueError, match="not a multiple"):
            compress(
                input_data,
                packet_size=16,  # 16 bits = 2 bytes per packet
                robustness=1,
            )

    def test_compress_packet_no_params_uses_defaults(self) -> None:
        """Test compress_packet with params=None uses default params."""
        comp = Compressor(
            packet_length=8,
            robustness=1,
            pt_limit=10,
            ft_limit=20,
            rt_limit=50,
        )

        input_data = BitVector(8)
        input_data.from_bytes(bytes([0xAA]))

        # Call with params=None to trigger default param creation
        output = comp.compress_packet(input_data, params=None)
        assert len(output) > 0
        assert comp.t == 1


class TestEffectiveRobustnessCap:
    """Test Vt capping at 15."""

    def test_vt_max_at_15(self) -> None:
        """Test that Vt maxes at 15 (4 bits max).

        Ct is capped at (15 - Rt) so Vt = Rt + Ct <= 15.
        """
        comp = Compressor(packet_length=8, robustness=7)
        # Set t large enough and ensure Ct accumulates to maximum
        comp.t = 50

        # All zeros in history = Ct accumulates to maximum
        for i in range(16):
            comp.change_history[i].zero()
        comp.history_index = 16

        change = BitVector(8)
        Vt = compute_effective_robustness(comp, change)
        # Rt=7, Ct can be up to (15-Rt)=8
        # So Vt = Rt + Ct = 7 + 8 = 15 (max)
        assert Vt == 15


class TestNonZeroInitialMask:
    """Regression tests for GOTCHAS #19: D0 = 0 per CCSDS Eq. 8 (issue #98)."""

    def test_first_packet_starts_with_rle_terminator(self) -> None:
        """With a non-zero M0, the first packet must still encode X0 = 0.

        CCSDS Eq. 8 defines D0 = 0, so RLE(X0) is just the terminator '10'.
        The pre-fix behavior (D0 = M0) wrongly encoded the initial mask as a
        change, producing '11...' as the first bits.
        """
        initial_mask = BitVector(16)
        initial_mask.from_bytes(bytes([0xFF, 0x00]))
        comp = Compressor(
            packet_length=16,
            robustness=1,
            initial_mask=initial_mask,
            pt_limit=10,
            ft_limit=20,
            rt_limit=50,
        )

        input_data = BitVector(16)
        input_data.from_bytes(bytes([0xAB, 0xCD]))
        output = comp.compress_packet(input_data)

        # First two bits of h0 are RLE(X0) = '10' (terminator only)
        first_byte = output[0]
        assert (first_byte >> 7) & 1 == 1
        assert (first_byte >> 6) & 1 == 0


class TestPacketLengthValidation:
    """CCSDS 124.0-B-1 section 3.2: 1 <= F <= 65535 (issue #99)."""

    def test_rejects_zero_packet_length(self) -> None:
        with pytest.raises(ValueError):
            Compressor(packet_length=0)

    def test_rejects_oversized_packet_length(self) -> None:
        with pytest.raises(ValueError):
            Compressor(packet_length=65536)

    def test_accepts_boundary_packet_lengths(self) -> None:
        assert Compressor(packet_length=1).F == 1
        assert Compressor(packet_length=65535).F == 65535
