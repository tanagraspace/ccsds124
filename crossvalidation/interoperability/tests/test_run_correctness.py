#!/usr/bin/env python3
"""Tests for the pairwise interoperability runner.

Everything is driven by small deterministic fake implementations generated at
test time, so the suite needs no external binary, no network, and no CCSDS
implementation. Run with:

    python3 -m unittest discover -s crossvalidation/interoperability/tests -v
"""
import contextlib
import io
import json
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
RUNNER_DIR = os.path.dirname(TESTS_DIR)
sys.path.insert(0, RUNNER_DIR)

import run_correctness  # noqa: E402


FAKE_IMPLEMENTATION_SOURCE = r'''#!/usr/bin/env python3
"""Deterministic fake implementation used by the runner tests.

Behaviour is selected with the FAKE_MODE environment variable. "Compression" is
a reversible XOR with the key in FAKE_KEY, so two fakes sharing a key produce
byte-identical streams, and two fakes with different keys cannot read each
other's output.
"""
import json
import os
import sys


def write_result(path, payload):
    with open(path, "w") as handle:
        json.dump(payload, handle)


mode = sys.argv[1]
arguments = dict(zip(sys.argv[2::2], sys.argv[3::2]))
behaviour = os.environ.get("FAKE_MODE", "good")
key = int(os.environ.get("FAKE_KEY", "0"))

if behaviour == "hang":
    import time
    time.sleep(30)

with open(arguments["--in"], "rb") as handle:
    data = handle.read()

base = {"impl": "fake", "mode": mode, "input_bytes": len(data)}

if behaviour == "compress_error" and mode == "compress":
    write_result(arguments["--json"],
                 dict(base, rc=7, output_bytes=0, error="synthetic failure"))
    sys.exit(1)

if behaviour == "bad_json":
    with open(arguments["--json"], "w") as handle:
        handle.write("{not valid json")
    sys.exit(0)

if behaviour == "missing_keys":
    write_result(arguments["--json"], {"impl": "fake", "mode": mode})
    sys.exit(0)

if behaviour == "wrong_types":
    write_result(arguments["--json"], dict(base, rc="zero", output_bytes=-3))
    sys.exit(0)

if behaviour == "bad_mode":
    write_result(arguments["--json"],
                 dict(base, mode="transmogrify", rc=0, output_bytes=0))
    sys.exit(0)

if behaviour == "bad_error_type":
    write_result(arguments["--json"],
                 dict(base, rc=0, output_bytes=0, error={"message": "invalid"}))
    sys.exit(0)

if behaviour == "empty_impl":
    write_result(arguments["--json"], dict(base, impl="", rc=0, output_bytes=0))
    sys.exit(0)

if behaviour == "no_json":
    sys.exit(0)

if behaviour == "no_output":
    write_result(arguments["--json"], dict(base, rc=0, output_bytes=0))
    sys.exit(0)

output = bytes(byte ^ key for byte in data)

if behaviour == "bad_decompress" and mode == "decompress":
    output = bytes((byte + 1) % 256 for byte in output)

if behaviour == "extra_zero_byte" and mode == "decompress":
    output = output + b"\x00"

with open(arguments["--out"], "wb") as handle:
    handle.write(output)

if behaviour == "wrong_input_bytes":
    write_result(arguments["--json"],
                 dict(base, rc=0, input_bytes=len(data) + 1,
                      output_bytes=len(output)))
    sys.exit(0)

if behaviour == "wrong_output_bytes":
    write_result(arguments["--json"],
                 dict(base, rc=0, output_bytes=len(output) + 1))
    sys.exit(0)

if behaviour == "rc_nonzero_exit_zero":
    write_result(arguments["--json"],
                 dict(base, rc=5, output_bytes=len(output), error="declared rc 5"))
    sys.exit(0)

if behaviour == "exit_nonzero_rc_zero":
    write_result(arguments["--json"], dict(base, rc=0, output_bytes=len(output)))
    sys.exit(4)

if behaviour == "both_nonzero":
    write_result(arguments["--json"],
                 dict(base, rc=9, output_bytes=len(output), error="both non-zero"))
    sys.exit(4)

write_result(arguments["--json"], dict(base, rc=0, output_bytes=len(output)))
'''


def write_fake_implementation(path, extra_environment=None):
    """Write the fake implementation, optionally wrapped to force env vars."""
    with open(path, "w") as handle:
        handle.write(FAKE_IMPLEMENTATION_SOURCE)
    os.chmod(path, os.stat(path).st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)


def write_env_wrapper(path, target, environment):
    """Write an executable that runs `target` with extra environment variables."""
    assignments = "".join(
        f"os.environ[{key!r}] = {value!r}\n" for key, value in environment.items())
    with open(path, "w") as handle:
        handle.write(
            "#!/usr/bin/env python3\n"
            "import os, runpy, sys\n"
            f"{assignments}"
            f"sys.argv[0] = {target!r}\n"
            f"runpy.run_path({target!r}, run_name='__main__')\n")
    os.chmod(path, 0o755)


def write_json_file(path, payload):
    with open(path, "w") as handle:
        json.dump(payload, handle)


def read_json_file(path):
    with open(path) as handle:
        return json.load(handle)


def build_case(name, input_path, robustness=1, pt=10, ft=20, rt=50,
               packet_bytes=90):
    """One test case entry, with the packet geometry the fixtures use."""
    return {"name": name, "input": input_path, "packet_bytes": packet_bytes,
            "R": robustness, "pt": pt, "ft": ft, "rt": rt}


class TestHelpers(unittest.TestCase):
    """Direct tests of the comparison helpers, without any subprocess."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="ccsds124-helpers-")
        self.addCleanup(shutil.rmtree, self.temp_dir, ignore_errors=True)

    def write_pair(self, first_content, second_content):
        """Write two files and return their paths."""
        paths = []
        for name, content in (("first", first_content), ("second", second_content)):
            path = os.path.join(self.temp_dir, name)
            with open(path, "wb") as handle:
                handle.write(content)
            paths.append(path)
        return paths

    def test_reconstruction_exact(self):
        matches, _ = run_correctness.reconstruction_matches(
            *self.write_pair(b"hello", b"hello"))
        self.assertTrue(matches)

    def test_reconstruction_rejects_one_extra_zero_byte(self):
        matches, detail = run_correctness.reconstruction_matches(
            *self.write_pair(b"hello", b"hello\x00"))
        self.assertFalse(matches)
        self.assertIn("size", detail)

    def test_reconstruction_rejects_short_output(self):
        matches, detail = run_correctness.reconstruction_matches(
            *self.write_pair(b"hello", b"hel"))
        self.assertFalse(matches)
        self.assertIn("size", detail)

    def test_reconstruction_rejects_missing_output(self):
        original, _ = self.write_pair(b"hello", b"hello")
        matches, detail = run_correctness.reconstruction_matches(
            original, os.path.join(self.temp_dir, "absent"))
        self.assertFalse(matches)
        self.assertIn("no output produced", detail)

    def test_streams_identical(self):
        identical, difference, detail = run_correctness.compare_streams(
            *self.write_pair(b"abc", b"abc"))
        self.assertTrue(identical)
        self.assertIsNone(difference)
        self.assertEqual(detail, "")

    def test_empty_streams_are_identical(self):
        identical, difference, _ = run_correctness.compare_streams(
            *self.write_pair(b"", b""))
        self.assertTrue(identical)
        self.assertIsNone(difference)

    def test_difference_on_first_bit(self):
        _, difference, _ = run_correctness.compare_streams(
            *self.write_pair(bytes([0x00]), bytes([0x80])))
        self.assertEqual(difference["byte"], 0)
        self.assertEqual(difference["bit"], 0)
        self.assertEqual(difference["value_a"], 0x00)
        self.assertEqual(difference["value_b"], 0x80)

    def test_difference_in_the_middle(self):
        _, difference, _ = run_correctness.compare_streams(
            *self.write_pair(b"abcdef", b"abcXef"))
        self.assertEqual(difference["byte"], 3)
        self.assertEqual(difference["value_a"], ord("d"))
        self.assertEqual(difference["value_b"], ord("X"))

    def test_first_stream_shorter(self):
        identical, difference, _ = run_correctness.compare_streams(
            *self.write_pair(b"abc", b"abcd"))
        self.assertFalse(identical)
        self.assertEqual(difference["byte"], 3)
        self.assertIsNone(difference["value_a"])
        self.assertEqual(difference["value_b"], ord("d"))

    def test_second_stream_shorter(self):
        identical, difference, _ = run_correctness.compare_streams(
            *self.write_pair(b"abcd", b"abc"))
        self.assertFalse(identical)
        self.assertEqual(difference["value_a"], ord("d"))
        self.assertIsNone(difference["value_b"])

    def test_comparison_impossible_when_a_stream_is_missing(self):
        first, _ = self.write_pair(b"abc", b"abc")
        identical, difference, detail = run_correctness.compare_streams(
            first, os.path.join(self.temp_dir, "absent"))
        self.assertIsNone(identical)
        self.assertIsNone(difference)
        self.assertIn("not produced", detail)


class RunnerTestBase(unittest.TestCase):
    """Shared fixture: one fake executable, one input file, one case file."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="ccsds124-interop-")
        self.addCleanup(shutil.rmtree, self.temp_dir, ignore_errors=True)

        self.fake_executable = os.path.join(self.temp_dir, "fake_impl.py")
        write_fake_implementation(self.fake_executable)

        # 900 bytes: exactly ten packets of 90 bytes.
        self.input_path = os.path.join(self.temp_dir, "input.bin")
        with open(self.input_path, "wb") as handle:
            handle.write(bytes((index * 7 + 3) % 256 for index in range(900)))

        self.cases_path = os.path.join(self.temp_dir, "cases.json")
        write_json_file(self.cases_path, [build_case("fixture", self.input_path)])

        self.out_json_path = os.path.join(self.temp_dir, "out.json")
        self.workdir = os.path.join(self.temp_dir, "work")

    def write_config_file(self, entries=None, timeout=None, name="config.json"):
        """Write a configuration and return its path."""
        if entries is None:
            second = {"name": "impl-b", "executable": self.fake_executable}
            if timeout is not None:
                second["timeout"] = timeout
            entries = [{"name": "impl-a", "executable": self.fake_executable}, second]

        config_path = os.path.join(self.temp_dir, name)
        write_json_file(config_path, {"implementations": entries})
        return config_path

    def run_runner(self, config_path=None, cases_path=None, environment=None,
                   workdir=None, extra_args=()):
        """Run the runner in-process and return (exit_code, stderr, rows)."""
        argv = [
            "--config", config_path or self.write_config_file(),
            "--cases", cases_path or self.cases_path,
            "--workdir", workdir or self.workdir,
            "--out-json", self.out_json_path,
            "--out-csv", os.path.join(self.temp_dir, "out.csv"),
            *extra_args,
        ]

        child_environment = dict(os.environ, FAKE_MODE="good")
        if environment:
            child_environment.update(environment)

        stdout, stderr = io.StringIO(), io.StringIO()
        with mock.patch.dict(os.environ, child_environment, clear=True), \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            exit_code = run_correctness.main(argv)

        rows = read_json_file(self.out_json_path) if os.path.exists(
            self.out_json_path) else []
        return exit_code, stderr.getvalue(), rows


class TestNominal(RunnerTestBase):
    def test_bit_identical_streams(self):
        exit_code, stderr, rows = self.run_runner()
        self.assertEqual(exit_code, 0, stderr)
        self.assertEqual(len(rows), 1)

        row = rows[0]
        self.assertIs(row["E_bit_identical"], True)
        self.assertIsNone(row["E_first_difference"])
        for key in ("A_encoder_roundtrip", "B_decoder_roundtrip",
                    "C_encoder_to_decoder", "D_decoder_to_encoder"):
            self.assertTrue(row[key], f"{key} should pass")

    def test_reported_impl_is_recorded_not_checked(self):
        _, _, rows = self.run_runner()
        self.assertEqual(rows[0]["encoder_reported_impl"], "fake")
        self.assertNotEqual(rows[0]["encoder"], rows[0]["encoder_reported_impl"])

    def test_sha256_recorded_for_every_artefact(self):
        _, _, rows = self.run_runner()
        for key in ("input_sha256", "encoder_compressed_sha256",
                    "decoder_compressed_sha256", "A_output_sha256",
                    "B_output_sha256"):
            self.assertEqual(len(rows[0][key]), 64, key)

    def test_runner_processes_multiple_cases(self):
        second_input = os.path.join(self.temp_dir, "input2.bin")
        with open(second_input, "wb") as handle:
            handle.write(bytes((index * 11) % 256 for index in range(1800)))

        cases_path = os.path.join(self.temp_dir, "two_cases.json")
        write_json_file(cases_path, [
            build_case("one", self.input_path),
            build_case("two", second_input, robustness=2, pt=20, ft=50, rt=100),
        ])

        exit_code, stderr, rows = self.run_runner(cases_path=cases_path)
        self.assertEqual(exit_code, 0, stderr)
        self.assertEqual(len(rows), 2)
        self.assertEqual({row["case"] for row in rows}, {"one", "two"})

    def test_three_implementations_give_three_unique_pairs(self):
        config_path = self.write_config_file(entries=[
            {"name": "impl-a", "executable": self.fake_executable},
            {"name": "impl-b", "executable": self.fake_executable},
            {"name": "impl-c", "executable": self.fake_executable},
        ])
        exit_code, stderr, rows = self.run_runner(config_path=config_path)
        self.assertEqual(exit_code, 0, stderr)
        self.assertEqual(len(rows), 3)
        self.assertEqual(
            {(row["encoder"], row["decoder"]) for row in rows},
            {("impl-a", "impl-b"), ("impl-a", "impl-c"), ("impl-b", "impl-c")})
        for row in rows:
            for key in ("A_encoder_roundtrip", "B_decoder_roundtrip",
                        "C_encoder_to_decoder", "D_decoder_to_encoder"):
                self.assertIsNotNone(row[key])

    def test_results_are_natively_json_serialisable(self):
        self.run_runner()
        with open(self.out_json_path) as handle:
            self.assertIsInstance(json.load(handle), list)

    def test_runs_from_a_different_working_directory(self):
        """Relative paths resolve against the config and cases files, not the CWD."""
        config_path = self.write_config_file(entries=[
            {"name": "impl-a", "executable": "fake_impl.py"},
            {"name": "impl-b", "executable": "fake_impl.py"},
        ], name="relative_config.json")

        cases_path = os.path.join(self.temp_dir, "relative_cases.json")
        write_json_file(cases_path, [build_case("relative", "input.bin")])

        other_directory = tempfile.mkdtemp(prefix="ccsds124-cwd-")
        self.addCleanup(shutil.rmtree, other_directory, ignore_errors=True)

        process = subprocess.run([
            sys.executable, os.path.join(RUNNER_DIR, "run_correctness.py"),
            "--config", config_path, "--cases", cases_path,
            "--workdir", self.workdir, "--out-json", self.out_json_path,
        ], capture_output=True, text=True, cwd=other_directory, timeout=60)

        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertTrue(read_json_file(self.out_json_path)[0]["A_encoder_roundtrip"])

    def test_paths_containing_spaces(self):
        spaced_directory = os.path.join(self.temp_dir, "dir with spaces")
        os.makedirs(spaced_directory, exist_ok=True)
        spaced_input = os.path.join(spaced_directory, "in put.bin")
        shutil.copy(self.input_path, spaced_input)

        cases_path = os.path.join(spaced_directory, "ca ses.json")
        write_json_file(cases_path, [build_case("spaced", spaced_input)])

        exit_code, stderr, rows = self.run_runner(cases_path=cases_path)
        self.assertEqual(exit_code, 0, stderr)
        self.assertTrue(rows[0]["A_encoder_roundtrip"])


class TestStaleArtefacts(RunnerTestBase):
    """A run that produces nothing must never reuse a previous run's files."""

    def test_missing_json_is_not_satisfied_by_a_previous_run(self):
        first_code, _, _ = self.run_runner()
        self.assertEqual(first_code, 0)

        second_code, _, rows = self.run_runner(environment={"FAKE_MODE": "no_json"})
        self.assertEqual(second_code, 1)
        self.assertIn("produced no JSON", rows[0]["encoder_compress_error"])

    def test_missing_output_is_not_satisfied_by_a_previous_run(self):
        first_code, _, _ = self.run_runner()
        self.assertEqual(first_code, 0)

        second_code, _, rows = self.run_runner(environment={"FAKE_MODE": "no_output"})
        self.assertEqual(second_code, 1)
        self.assertIn("no output written", rows[0]["encoder_compress_error"])
        self.assertIsNone(rows[0]["encoder_compressed_bytes"])


class TestArtefactLayout(RunnerTestBase):
    def test_names_that_would_collide_when_concatenated_stay_separate(self):
        """("a-vs-b", "c") and ("a", "b-vs-c") must not share a directory.

        Both are valid names, and joining them into a single tag would produce
        "a-vs-b-vs-c" for either pair. Nesting keeps them apart.
        """
        config_path = self.write_config_file(entries=[
            {"name": "a-vs-b", "executable": self.fake_executable},
            {"name": "c", "executable": self.fake_executable},
            {"name": "a", "executable": self.fake_executable},
            {"name": "b-vs-c", "executable": self.fake_executable},
        ])

        exit_code, stderr, rows = self.run_runner(config_path=config_path)
        self.assertEqual(exit_code, 0, stderr)

        # Four implementations give six unique pairs.
        self.assertEqual(len(rows), 6)
        pairs = {(row["encoder"], row["decoder"]) for row in rows}
        self.assertEqual(len(pairs), 6)
        self.assertIn(("a-vs-b", "c"), pairs)
        self.assertIn(("a", "b-vs-c"), pairs)

        # No result was overwritten: every pair kept its own artefacts.
        for row in rows:
            self.assertTrue(row["A_encoder_roundtrip"])
            self.assertIsNotNone(row["encoder_compressed_sha256"])

        first = os.path.join(self.workdir, "fixture", "a-vs-b", "c")
        second = os.path.join(self.workdir, "fixture", "a", "b-vs-c")
        self.assertTrue(os.path.isdir(first))
        self.assertTrue(os.path.isdir(second))
        self.assertNotEqual(os.path.realpath(first), os.path.realpath(second))

        # Every artefact stays under the work directory.
        root = os.path.realpath(self.workdir)
        for directory, _, filenames in os.walk(root):
            for filename in filenames:
                path = os.path.realpath(os.path.join(directory, filename))
                self.assertTrue(path.startswith(root + os.sep), path)


class TestOutputPathErrors(RunnerTestBase):
    def test_workdir_is_an_existing_file(self):
        blocking_file = os.path.join(self.temp_dir, "not-a-directory")
        with open(blocking_file, "w") as handle:
            handle.write("")

        exit_code, stderr, _ = self.run_runner(workdir=blocking_file)
        self.assertEqual(exit_code, 2)
        self.assertNotIn("Traceback", stderr)
        self.assertIn("workdir", stderr)

    def test_out_json_parent_is_a_file(self):
        blocking_file = os.path.join(self.temp_dir, "blocking")
        with open(blocking_file, "w") as handle:
            handle.write("")
        self.out_json_path = os.path.join(blocking_file, "out.json")

        exit_code, stderr, _ = self.run_runner()
        self.assertEqual(exit_code, 2)
        self.assertNotIn("Traceback", stderr)
        self.assertIn("cannot write", stderr)


class TestExecutionContract(RunnerTestBase):
    def test_exit_zero_and_rc_zero_succeeds(self):
        exit_code, _, rows = self.run_runner()
        self.assertEqual(exit_code, 0)
        self.assertEqual(rows[0]["encoder_process_exit_code"], 0)
        self.assertEqual(rows[0]["encoder_implementation_rc"], 0)

    def test_exit_zero_but_rc_nonzero_fails(self):
        exit_code, _, rows = self.run_runner(
            environment={"FAKE_MODE": "rc_nonzero_exit_zero"})
        self.assertEqual(exit_code, 1)
        self.assertEqual(rows[0]["encoder_process_exit_code"], 0)
        self.assertEqual(rows[0]["encoder_implementation_rc"], 5)
        self.assertFalse(rows[0]["A_encoder_roundtrip"])

    def test_exit_nonzero_but_rc_zero_is_flagged_inconsistent(self):
        exit_code, _, rows = self.run_runner(
            environment={"FAKE_MODE": "exit_nonzero_rc_zero"})
        self.assertEqual(exit_code, 1)
        self.assertEqual(rows[0]["encoder_process_exit_code"], 4)
        self.assertEqual(rows[0]["encoder_implementation_rc"], 0)
        self.assertIn("inconsistent status", rows[0]["encoder_compress_error"])

    def test_both_nonzero_keeps_both_codes(self):
        exit_code, _, rows = self.run_runner(environment={"FAKE_MODE": "both_nonzero"})
        self.assertEqual(exit_code, 1)
        self.assertEqual(rows[0]["encoder_process_exit_code"], 4)
        self.assertEqual(rows[0]["encoder_implementation_rc"], 9)

    def test_declared_mode_must_match_invocation(self):
        exit_code, _, rows = self.run_runner(environment={"FAKE_MODE": "bad_mode"})
        self.assertEqual(exit_code, 1)
        self.assertIn("is not one of", rows[0]["encoder_compress_error"])

    def test_declared_input_bytes_must_match_disk(self):
        exit_code, _, rows = self.run_runner(
            environment={"FAKE_MODE": "wrong_input_bytes"})
        self.assertEqual(exit_code, 1)
        self.assertIn("declared input_bytes", rows[0]["encoder_compress_error"])

    def test_declared_output_bytes_must_match_disk(self):
        exit_code, _, rows = self.run_runner(
            environment={"FAKE_MODE": "wrong_output_bytes"})
        self.assertEqual(exit_code, 1)
        self.assertIn("declared output_bytes", rows[0]["encoder_compress_error"])

    def test_missing_output_with_rc_zero_fails(self):
        exit_code, _, rows = self.run_runner(environment={"FAKE_MODE": "no_output"})
        self.assertEqual(exit_code, 1)
        self.assertIn("no output written", rows[0]["encoder_compress_error"])

    def test_invalid_json_is_detected(self):
        exit_code, _, rows = self.run_runner(environment={"FAKE_MODE": "bad_json"})
        self.assertEqual(exit_code, 1)
        self.assertIn("invalid JSON", rows[0]["encoder_compress_error"])

    def test_missing_required_keys_detected(self):
        exit_code, _, rows = self.run_runner(environment={"FAKE_MODE": "missing_keys"})
        self.assertEqual(exit_code, 1)
        self.assertIn("missing required field", rows[0]["encoder_compress_error"])

    def test_wrong_field_types_detected(self):
        exit_code, _, rows = self.run_runner(environment={"FAKE_MODE": "wrong_types"})
        self.assertEqual(exit_code, 1)
        message = rows[0]["encoder_compress_error"]
        self.assertIn("'rc' must be an integer", message)
        self.assertIn("'output_bytes' must be >= 0", message)

    def test_error_field_must_be_a_string(self):
        exit_code, _, rows = self.run_runner(
            environment={"FAKE_MODE": "bad_error_type"})
        self.assertEqual(exit_code, 1)
        self.assertIn("'error' must be a string", rows[0]["encoder_compress_error"])

    def test_reported_impl_must_not_be_empty(self):
        exit_code, _, rows = self.run_runner(environment={"FAKE_MODE": "empty_impl"})
        self.assertEqual(exit_code, 1)
        self.assertIn("'impl' must not be empty", rows[0]["encoder_compress_error"])

    def test_absent_json_detected(self):
        exit_code, _, rows = self.run_runner(environment={"FAKE_MODE": "no_json"})
        self.assertEqual(exit_code, 1)
        self.assertIn("produced no JSON", rows[0]["encoder_compress_error"])

    def test_encoder_error_is_surfaced(self):
        exit_code, _, rows = self.run_runner(
            environment={"FAKE_MODE": "compress_error"})
        self.assertEqual(exit_code, 1)
        self.assertEqual(rows[0]["encoder_implementation_rc"], 7)

    def test_missing_executable(self):
        config_path = self.write_config_file(entries=[
            {"name": "impl-a", "executable": os.path.join(self.temp_dir, "absent")},
            {"name": "impl-b", "executable": self.fake_executable},
        ])
        exit_code, _, rows = self.run_runner(config_path=config_path)
        self.assertEqual(exit_code, 1)
        self.assertIn("not found", rows[0]["encoder_compress_error"])

    def test_timeout_only_blocks_the_targeted_implementation(self):
        hanging = os.path.join(self.temp_dir, "hanging.py")
        write_env_wrapper(hanging, self.fake_executable, {"FAKE_MODE": "hang"})
        config_path = self.write_config_file(entries=[
            {"name": "impl-a", "executable": self.fake_executable},
            {"name": "impl-b", "executable": hanging, "timeout": 1},
        ])

        started = time.monotonic()
        exit_code, _, rows = self.run_runner(config_path=config_path)
        elapsed = time.monotonic() - started

        self.assertEqual(exit_code, 1)
        self.assertIn("timeout", rows[0]["decoder_compress_error"])
        self.assertLess(elapsed, 20, "the healthy implementation must not hang")


class TestRoundTripStrictness(RunnerTestBase):
    def test_wrong_reconstruction_fails(self):
        exit_code, _, rows = self.run_runner(
            environment={"FAKE_MODE": "bad_decompress"})
        self.assertEqual(exit_code, 1)
        self.assertFalse(rows[0]["A_encoder_roundtrip"])

    def test_one_extra_zero_byte_fails(self):
        exit_code, _, rows = self.run_runner(
            environment={"FAKE_MODE": "extra_zero_byte"})
        self.assertEqual(exit_code, 1)
        self.assertFalse(rows[0]["A_encoder_roundtrip"])


class TestBitIdentityStates(RunnerTestBase):
    def test_true_when_both_streams_present_and_equal(self):
        _, _, rows = self.run_runner()
        self.assertIs(rows[0]["E_bit_identical"], True)
        self.assertEqual(rows[0]["E_detail"], "")

    def test_false_when_both_streams_present_and_different(self):
        divergent = os.path.join(self.temp_dir, "divergent.py")
        write_env_wrapper(divergent, self.fake_executable, {"FAKE_KEY": "255"})
        config_path = self.write_config_file(entries=[
            {"name": "impl-a", "executable": self.fake_executable},
            {"name": "impl-b", "executable": divergent},
        ])

        exit_code, _, rows = self.run_runner(config_path=config_path)
        row = rows[0]
        self.assertIs(row["E_bit_identical"], False)
        self.assertIsNotNone(row["E_first_difference"])
        self.assertTrue(row["A_encoder_roundtrip"])
        self.assertTrue(row["B_decoder_roundtrip"])
        # Cross decoding uses the other key, so the reconstruction is wrong.
        self.assertFalse(row["C_encoder_to_decoder"])
        self.assertEqual(exit_code, 1)

    def test_null_when_a_stream_was_never_produced(self):
        _, _, rows = self.run_runner(environment={"FAKE_MODE": "no_output"})
        self.assertIsNone(rows[0]["E_bit_identical"])
        self.assertIsNone(rows[0]["E_first_difference"])
        self.assertIn("not produced", rows[0]["E_detail"])


class TestConfigurationValidation(RunnerTestBase):
    def assert_usage_error(self, entries=None, raw_config=None, extra_args=()):
        config_path = os.path.join(self.temp_dir, "invalid.json")
        if raw_config is None:
            raw_config = {"implementations": entries}
        write_json_file(config_path, raw_config)

        exit_code, stderr, _ = self.run_runner(config_path=config_path,
                                               extra_args=extra_args)
        self.assertEqual(exit_code, 2)
        self.assertNotIn("Traceback", stderr)
        self.assertTrue(stderr.startswith("error:"), stderr)
        return stderr

    def test_root_is_a_list(self):
        self.assert_usage_error(raw_config=[{"name": "a"}])

    def test_implementations_is_not_a_list(self):
        self.assert_usage_error(raw_config={"implementations": "impl-a"})

    def test_implementation_entry_is_not_an_object(self):
        self.assert_usage_error(entries=["impl-a", "impl-b"])

    def test_single_implementation(self):
        self.assert_usage_error(
            entries=[{"name": "only", "executable": self.fake_executable}])

    def test_missing_executable_key(self):
        self.assert_usage_error(entries=[
            {"name": "impl-a"},
            {"name": "impl-b", "executable": self.fake_executable}])

    def test_empty_executable(self):
        self.assert_usage_error(entries=[
            {"name": "impl-a", "executable": ""},
            {"name": "impl-b", "executable": self.fake_executable}])

    def test_extra_args_is_a_string(self):
        self.assert_usage_error(entries=[
            {"name": "impl-a", "executable": self.fake_executable,
             "extra_args": "--flag"},
            {"name": "impl-b", "executable": self.fake_executable}])

    def test_extra_args_contains_a_non_string(self):
        self.assert_usage_error(entries=[
            {"name": "impl-a", "executable": self.fake_executable,
             "extra_args": ["--flag", 3]},
            {"name": "impl-b", "executable": self.fake_executable}])

    def test_zero_timeout(self):
        self.assert_usage_error(entries=[
            {"name": "impl-a", "executable": self.fake_executable, "timeout": 0},
            {"name": "impl-b", "executable": self.fake_executable}])

    def test_negative_timeout(self):
        self.assert_usage_error(entries=[
            {"name": "impl-a", "executable": self.fake_executable, "timeout": -1},
            {"name": "impl-b", "executable": self.fake_executable}])

    def test_boolean_timeout(self):
        self.assert_usage_error(entries=[
            {"name": "impl-a", "executable": self.fake_executable, "timeout": True},
            {"name": "impl-b", "executable": self.fake_executable}])

    def test_global_timeout_must_be_positive(self):
        self.assert_usage_error(entries=[
            {"name": "impl-a", "executable": self.fake_executable},
            {"name": "impl-b", "executable": self.fake_executable}],
            extra_args=("--timeout", "0"))

    def test_duplicate_implementation_names(self):
        stderr = self.assert_usage_error(entries=[
            {"name": "same", "executable": self.fake_executable},
            {"name": "same", "executable": self.fake_executable}])
        self.assertIn("unique", stderr)

    def test_unsafe_implementation_names(self):
        for unsafe in ("../escape", "/tmp/escape", "a/b", "", "."):
            with self.subTest(name=unsafe):
                self.assert_usage_error(entries=[
                    {"name": unsafe, "executable": self.fake_executable},
                    {"name": "impl-b", "executable": self.fake_executable}])


class TestCaseValidation(RunnerTestBase):
    def assert_case_error(self, cases):
        cases_path = os.path.join(self.temp_dir, "invalid_cases.json")
        write_json_file(cases_path, cases)

        exit_code, stderr, _ = self.run_runner(cases_path=cases_path)
        self.assertEqual(exit_code, 2)
        self.assertNotIn("Traceback", stderr)
        self.assertTrue(stderr.startswith("error:"), stderr)
        return stderr

    def test_cases_file_is_not_a_list(self):
        self.assert_case_error({"name": "not-a-list"})

    def test_cases_file_is_empty(self):
        self.assert_case_error([])

    def test_case_is_not_an_object(self):
        self.assert_case_error(["fixture"])

    def test_missing_input(self):
        self.assert_case_error([build_case("nope", "/nonexistent/file.bin")])

    def test_input_is_a_directory(self):
        self.assert_case_error([build_case("dir", self.temp_dir)])

    def test_input_size_is_not_a_multiple_of_packet_bytes(self):
        stderr = self.assert_case_error(
            [build_case("odd", self.input_path, packet_bytes=7)])
        self.assertIn("multiple of", stderr)

    def test_packet_bytes_must_be_positive(self):
        self.assert_case_error(
            [build_case("zero", self.input_path, packet_bytes=0)])

    def test_robustness_out_of_range(self):
        self.assert_case_error([build_case("bad-r", self.input_path, robustness=8)])

    def test_robustness_must_not_be_boolean(self):
        self.assert_case_error([build_case("bool-r", self.input_path, robustness=True)])

    def test_period_must_be_an_integer(self):
        self.assert_case_error([build_case("bad-pt", self.input_path, pt="ten")])

    def test_duplicate_case_names(self):
        stderr = self.assert_case_error([
            build_case("twice", self.input_path),
            build_case("twice", self.input_path),
        ])
        self.assertIn("duplicate case name", stderr)

    def test_unsafe_case_names(self):
        for unsafe in ("../escape", "/tmp/escape", "a/b", "", ".."):
            with self.subTest(name=unsafe):
                self.assert_case_error([build_case(unsafe, self.input_path)])

    def test_no_artefact_escapes_the_workdir(self):
        """Unsafe names are rejected before any artefact can be written."""
        outside = os.path.join(self.temp_dir, "outside")
        os.makedirs(outside, exist_ok=True)
        self.assert_case_error([build_case("../outside/escape", self.input_path)])
        self.assertEqual(os.listdir(outside), [])


class TestSchemaAgreement(unittest.TestCase):
    def test_schema_file_and_code_agree(self):
        """The JSON schema must not drift from the checks enforced in code."""
        schema_path = os.path.join(RUNNER_DIR, "schemas", "result.schema.json")
        schema = read_json_file(schema_path)

        self.assertEqual(sorted(schema["required"]),
                         sorted(run_correctness.REQUIRED_RESULT_FIELDS))

        json_to_python = {"string": str, "integer": int}
        for field, expected_type in run_correctness.REQUIRED_RESULT_FIELDS.items():
            declared = schema["properties"][field]["type"]
            self.assertEqual(json_to_python[declared], expected_type,
                             f"schema and code disagree on type of '{field}'")

        self.assertEqual(tuple(schema["properties"]["mode"]["enum"]),
                         run_correctness.VALID_MODES)

        # The minimums the schema advertises are the ones the code enforces.
        for field in ("input_bytes", "output_bytes"):
            self.assertEqual(schema["properties"][field]["minimum"], 0)
        self.assertEqual(schema["properties"]["impl"]["minLength"], 1)

    def test_schema_declares_no_unconsumed_field(self):
        """Every documented field is one the runner actually reads."""
        schema_path = os.path.join(RUNNER_DIR, "schemas", "result.schema.json")
        schema = read_json_file(schema_path)
        consumed = set(run_correctness.REQUIRED_RESULT_FIELDS) | {"error"}
        self.assertEqual(set(schema["properties"]), consumed)


if __name__ == "__main__":
    unittest.main(verbosity=2)
