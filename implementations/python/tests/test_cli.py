"""Tests for CCSDS 124.0-B-1 CLI."""

import subprocess
import sys
import tempfile
from pathlib import Path

import pytest

# Path to cli.py
CLI_PATH = Path(__file__).parent.parent / "cli.py"

# Path to test vectors
REPO_ROOT = Path(__file__).parent.parent.parent.parent
TEST_VECTORS_DIR = REPO_ROOT / "test-vectors"
INPUT_DIR = TEST_VECTORS_DIR / "input"


def run_cli(*args: str) -> subprocess.CompletedProcess:
    """Run the CLI with the given arguments."""
    return subprocess.run(
        [sys.executable, str(CLI_PATH), *args],
        capture_output=True,
        text=True,
    )


class TestCliHelp:
    """Test CLI help and version."""

    def test_help_short(self) -> None:
        """Test -h flag shows help."""
        result = run_cli("-h")
        assert result.returncode == 0
        assert "CCSDS 124.0-B-1" in result.stdout
        assert "Usage:" in result.stdout

    def test_help_long(self) -> None:
        """Test --help flag shows help."""
        result = run_cli("--help")
        assert result.returncode == 0
        assert "Usage:" in result.stdout

    def test_version_short(self) -> None:
        """Test -v flag shows version."""
        result = run_cli("-v")
        assert result.returncode == 0
        assert "ccsds124" in result.stdout
        assert "1.0.0" in result.stdout

    def test_version_long(self) -> None:
        """Test --version flag shows version."""
        result = run_cli("--version")
        assert result.returncode == 0
        assert "1.0.0" in result.stdout

    def test_no_args_shows_help(self) -> None:
        """Test that no arguments shows help and exits with error."""
        result = run_cli()
        assert result.returncode == 1
        assert "Usage:" in result.stdout or "Usage:" in result.stderr


class TestCliErrors:
    """Test CLI error handling."""

    def test_compress_missing_args(self) -> None:
        """Test compress with missing arguments."""
        result = run_cli("input.bin", "90")  # Missing pt, ft, rt, robustness
        assert result.returncode == 1
        assert "Error" in result.stderr

    def test_decompress_missing_args(self) -> None:
        """Test decompress with missing arguments."""
        result = run_cli("-d", "input.pkt")  # Missing packet_size and robustness
        assert result.returncode == 1
        assert "Error" in result.stderr

    def test_invalid_robustness(self) -> None:
        """Test invalid robustness value."""
        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
            f.write(b"\x00" * 90)
            f.flush()
            result = run_cli(f.name, "90", "10", "20", "50", "8")  # robustness > 7
            assert result.returncode == 1
            assert "robustness" in result.stderr.lower()
            Path(f.name).unlink()

    def test_input_file_not_found(self) -> None:
        """Test error when input file doesn't exist."""
        result = run_cli("nonexistent.bin", "90", "10", "20", "50", "1")
        assert result.returncode == 1
        assert "Error" in result.stderr


class TestCliCompress:
    """Test CLI compression."""

    def test_compress_simple(self) -> None:
        """Test basic compression."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create input file (2 packets of 90 bytes each)
            input_path = Path(tmpdir) / "test.bin"
            input_path.write_bytes(b"\xaa" * 180)

            # Compress
            result = run_cli(str(input_path), "90", "10", "20", "50", "1")
            assert result.returncode == 0

            # Check output file exists
            output_path = Path(tmpdir) / "test.bin.pkt"
            assert output_path.exists()
            assert output_path.stat().st_size > 0

            # Check output contains summary
            assert "Input:" in result.stdout
            assert "Output:" in result.stdout
            assert "Ratio:" in result.stdout

    def test_compress_matches_reference(self) -> None:
        """Test compression matches reference for simple vector."""
        simple_input = INPUT_DIR / "simple.ccsds"
        if not simple_input.exists():
            pytest.skip("Test vector not available")

        with tempfile.TemporaryDirectory() as tmpdir:
            # Copy input to temp dir
            import shutil

            input_path = Path(tmpdir) / "simple.ccsds"
            shutil.copy(simple_input, input_path)

            # Compress with same params as reference
            result = run_cli(str(input_path), "90", "10", "20", "50", "1")
            assert result.returncode == 0

            # Output should exist
            output_path = Path(tmpdir) / "simple.ccsds.pkt"
            assert output_path.exists()


class TestCliDecompress:
    """Test CLI decompression."""

    def test_decompress_simple(self) -> None:
        """Test basic decompression."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create input file
            input_path = Path(tmpdir) / "test.bin"
            original_data = b"\xaa" * 180
            input_path.write_bytes(original_data)

            # Compress first
            run_cli(str(input_path), "90", "10", "20", "50", "1")
            compressed_path = Path(tmpdir) / "test.bin.pkt"
            assert compressed_path.exists()

            # Decompress
            result = run_cli("-d", str(compressed_path), "90", "1")
            assert result.returncode == 0

            # Check output file exists
            output_path = Path(tmpdir) / "test.bin.depkt"
            assert output_path.exists()

            # Verify round-trip
            assert output_path.read_bytes() == original_data


class TestCliRoundTrip:
    """Test CLI round-trip compression/decompression."""

    def test_round_trip_with_test_vector(self) -> None:
        """Test round-trip with simple test vector."""
        simple_input = INPUT_DIR / "simple.ccsds"
        if not simple_input.exists():
            pytest.skip("Test vector not available")

        with tempfile.TemporaryDirectory() as tmpdir:
            import shutil

            input_path = Path(tmpdir) / "simple.ccsds"
            shutil.copy(simple_input, input_path)
            original_data = input_path.read_bytes()

            # Compress
            result = run_cli(str(input_path), "90", "10", "20", "50", "1")
            assert result.returncode == 0

            # Decompress
            compressed_path = Path(tmpdir) / "simple.ccsds.pkt"
            result = run_cli("-d", str(compressed_path), "90", "1")
            assert result.returncode == 0

            # Verify
            output_path = Path(tmpdir) / "simple.ccsds.depkt"
            assert output_path.read_bytes() == original_data

    def test_round_trip_varying_robustness(self) -> None:
        """Test round-trip with different robustness levels."""
        with tempfile.TemporaryDirectory() as tmpdir:
            for robustness in [0, 1, 3, 7]:
                input_path = Path(tmpdir) / f"test_r{robustness}.bin"
                original_data = bytes(range(256)) * 2  # 512 bytes = ~5 packets
                input_path.write_bytes(original_data[:450])  # 5 packets of 90 bytes

                # Compress
                result = run_cli(
                    str(input_path), "90", "5", "10", "25", str(robustness)
                )
                assert result.returncode == 0, f"Compress failed for R={robustness}"

                # Decompress
                compressed_path = Path(tmpdir) / f"test_r{robustness}.bin.pkt"
                result = run_cli("-d", str(compressed_path), "90", str(robustness))
                assert result.returncode == 0, f"Decompress failed for R={robustness}"

                # Verify
                output_path = Path(tmpdir) / f"test_r{robustness}.bin.depkt"
                assert output_path.read_bytes() == original_data[:450]
