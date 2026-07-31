#!/usr/bin/env python3
"""Pairwise interoperability runner for CCSDS 124.0-B-1 implementations.

Drives external executables declared in a configuration file, as separate
processes, and compares their compressed streams and reconstructions. It holds no
knowledge of any particular implementation and does not build or download them.

Complements run_crossvalidation.sh: that script validates one pair of harness
binaries against the UAB/CNES suite, this one compares two implementations
against each other on the same inputs. See README.md.

Executable contract: schemas/result.schema.json

Exit status:
  0  every check passed
  1  a reconstruction was incorrect, or an executable failed
  2  configuration or usage error
"""

import argparse
import csv
import hashlib
import json
import os
import re
import subprocess
import sys

# The result contract. Checked here rather than through `jsonschema` to keep the
# runner dependency-free; a test asserts this table matches the schema file.
REQUIRED_RESULT_FIELDS = {
    "impl": str,
    "mode": str,
    "rc": int,
    "input_bytes": int,
    "output_bytes": int,
}

VALID_MODES = ("compress", "decompress")

# Names end up in artefact filenames, so they must not escape the work directory.
SAFE_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]*$")

MIN_ROBUSTNESS = 0
MAX_ROBUSTNESS = 7

READ_CHUNK_BYTES = 1024 * 1024

DEFAULT_TIMEOUT_SECONDS = 300


class ConfigurationError(Exception):
    """Raised for any invalid configuration or test case. Reported as exit 2."""


def is_plain_integer(value):
    """True for a real integer. Booleans are rejected: bool subclasses int."""
    return isinstance(value, int) and not isinstance(value, bool)


def require_non_empty_string(value, description):
    if not isinstance(value, str) or not value.strip():
        raise ConfigurationError(f"{description} must be a non-empty string")
    return value


def require_positive_integer(value, description):
    if not is_plain_integer(value) or value <= 0:
        raise ConfigurationError(f"{description} must be a positive integer")
    return value


def require_safe_name(value, description):
    require_non_empty_string(value, description)
    if not SAFE_NAME.match(value):
        raise ConfigurationError(
            f"{description} {value!r} is not a safe name; allowed pattern is "
            f"{SAFE_NAME.pattern}")
    return value


def validate_result_contract(payload, implementation_name):
    """Return a list of human-readable contract violations, empty when valid."""
    if not isinstance(payload, dict):
        return [f"{implementation_name}: JSON root is {type(payload).__name__}, "
                f"expected an object"]

    problems = []
    for field_name, expected_type in REQUIRED_RESULT_FIELDS.items():
        if field_name not in payload:
            problems.append(f"{implementation_name}: missing required field "
                            f"'{field_name}'")
            continue

        value = payload[field_name]
        actual_type = type(value).__name__
        if expected_type is int and not is_plain_integer(value):
            problems.append(f"{implementation_name}: field '{field_name}' must be "
                            f"an integer, got {actual_type}")
        elif expected_type is str and not isinstance(value, str):
            problems.append(f"{implementation_name}: field '{field_name}' must be "
                            f"a string, got {actual_type}")

    mode = payload.get("mode")
    if isinstance(mode, str) and mode not in VALID_MODES:
        problems.append(f"{implementation_name}: mode '{mode}' is not one of "
                        f"{VALID_MODES}")

    impl = payload.get("impl")
    if isinstance(impl, str) and not impl.strip():
        problems.append(f"{implementation_name}: field 'impl' must not be empty")

    for field_name in ("input_bytes", "output_bytes"):
        value = payload.get(field_name)
        if is_plain_integer(value) and value < 0:
            problems.append(f"{implementation_name}: field '{field_name}' must be "
                            f">= 0, got {value}")

    # 'error' is optional, but when present it must be a string: converting an
    # object or a number to text here would hide a contract violation.
    if "error" in payload and not isinstance(payload["error"], str):
        problems.append(f"{implementation_name}: field 'error' must be a string, "
                        f"got {type(payload['error']).__name__}")

    return problems


def sha256_file(path):
    """Return the SHA-256 hex digest of a file, or None if it cannot be read."""
    if not path or not os.path.isfile(path):
        return None

    digest = hashlib.sha256()
    try:
        with open(path, "rb") as handle:
            while chunk := handle.read(READ_CHUNK_BYTES):
                digest.update(chunk)
    except OSError:
        return None
    return digest.hexdigest()


def file_size(path):
    """Return the size of a file in bytes, or None if it cannot be read."""
    if not path:
        return None
    try:
        if not os.path.isfile(path):
            return None
        return os.path.getsize(path)
    except OSError:
        return None


def remove_previous_artifact(path):
    """Delete a leftover artefact so a failed run cannot reuse an older one."""
    try:
        os.remove(path)
    except FileNotFoundError:
        pass
    except OSError as error:
        raise ConfigurationError(f"cannot remove stale artefact {path}: {error}")


def files_are_identical(first_path, second_path):
    """Compare two files byte by byte, in chunks.

    Returns (identical, difference). `difference` locates the first divergence,
    with `value_a`/`value_b` set to None on whichever side ended first.
    """
    offset = 0
    with open(first_path, "rb") as first, open(second_path, "rb") as second:
        while True:
            first_chunk = first.read(READ_CHUNK_BYTES)
            second_chunk = second.read(READ_CHUNK_BYTES)
            if not first_chunk and not second_chunk:
                return True, None

            for index, (first_byte, second_byte) in enumerate(
                    zip(first_chunk, second_chunk)):
                if first_byte != second_byte:
                    byte_index = offset + index
                    bit_in_byte = 8 - (first_byte ^ second_byte).bit_length()
                    return False, {
                        "byte": byte_index,
                        "bit": byte_index * 8 + bit_in_byte,
                        "value_a": first_byte,
                        "value_b": second_byte,
                    }

            if len(first_chunk) != len(second_chunk):
                shorter = min(len(first_chunk), len(second_chunk))
                byte_index = offset + shorter
                first_value = first_chunk[shorter] if shorter < len(first_chunk) else None
                second_value = second_chunk[shorter] if shorter < len(second_chunk) else None
                return False, {
                    "byte": byte_index,
                    "bit": byte_index * 8,
                    "value_a": first_value,
                    "value_b": second_value,
                }

            offset += len(first_chunk)


def compare_streams(first_path, second_path):
    """Compare two compressed streams.

    Returns (identical, difference, detail). `identical` is None when the
    comparison could not be made at all, and `detail` then says why.
    """
    for path in (first_path, second_path):
        if not os.path.isfile(path):
            return None, None, f"stream not produced: {os.path.basename(path)}"

    try:
        identical, difference = files_are_identical(first_path, second_path)
    except OSError as error:
        return None, None, f"cannot read stream: {error}"
    return identical, difference, "" if identical else "streams differ"


def reconstruction_matches(original_path, produced_path):
    """Check that a decompressed file reproduces the original exactly.

    Returns (matches, detail). No trailing bytes are tolerated: the compressed
    stream may be byte-padded, the reconstruction may not.
    """
    if not os.path.isfile(produced_path):
        return False, f"no output produced: {os.path.basename(produced_path)}"

    original_size = file_size(original_path)
    produced_size = file_size(produced_path)
    if produced_size != original_size:
        return False, f"output size {produced_size} != input size {original_size}"

    try:
        identical, difference = files_are_identical(original_path, produced_path)
    except OSError as error:
        return False, f"cannot read output: {error}"
    if not identical:
        return False, f"payload differs from original at byte {difference['byte']}"
    return True, ""


class ExecutionOutcome:
    """The result of invoking one external executable once.

    The process exit code and the `rc` the implementation declares in its JSON
    are two different things; both are kept.
    """

    def __init__(self, succeeded, process_exit_code, implementation_rc,
                 error_message, result_payload):
        self.succeeded = succeeded
        self.process_exit_code = process_exit_code
        self.implementation_rc = implementation_rc
        self.error_message = error_message
        self.result_payload = result_payload

    @property
    def reported_impl(self):
        """The identifier the wrapper declared. Informational, never checked."""
        return self.result_payload.get("impl")


class RoundTripCheck:
    """Outcome of decompressing one stream and comparing it to the original."""

    def __init__(self, reconstructed_ok, detail, output_sha256):
        self.reconstructed_ok = reconstructed_ok
        self.detail = detail
        self.output_sha256 = output_sha256


class Implementation:
    """One external executable honouring the documented CLI/JSON contract."""

    def __init__(self, spec, default_timeout, config_dir=""):
        if not isinstance(spec, dict):
            raise ConfigurationError(
                f"implementation entry must be an object, got {type(spec).__name__}")

        for required_key in ("name", "executable"):
            if required_key not in spec:
                raise ConfigurationError(
                    f"implementation entry is missing '{required_key}'")

        self.name = require_safe_name(spec["name"], "implementation name")
        executable = require_non_empty_string(
            spec["executable"], f"implementation {self.name!r} 'executable'")

        # Relative paths resolve against the configuration file, not the CWD.
        if os.path.isabs(executable):
            self.executable = executable
        else:
            self.executable = os.path.normpath(os.path.join(config_dir, executable))

        extra_args = spec.get("extra_args", [])
        if not isinstance(extra_args, list):
            raise ConfigurationError(
                f"implementation {self.name!r} 'extra_args' must be a list")
        for argument in extra_args:
            if not isinstance(argument, str):
                raise ConfigurationError(
                    f"implementation {self.name!r} 'extra_args' must contain only "
                    f"strings, got {type(argument).__name__}")
        self.extra_args = list(extra_args)

        self.timeout = require_positive_integer(
            spec.get("timeout", default_timeout),
            f"implementation {self.name!r} 'timeout'")

    def build_command(self, mode, input_path, output_path, result_json_path,
                      packet_bytes, robustness, pt=None, ft=None, rt=None):
        """Assemble the argument list for one invocation."""
        command = [
            self.executable, mode,
            "--in", input_path,
            "--out", output_path,
            "--json", result_json_path,
            "--packet-bytes", str(packet_bytes),
            "--R", str(robustness),
        ]
        if mode == "compress":
            command += ["--pt", str(pt), "--ft", str(ft), "--rt", str(rt)]
        return command + self.extra_args

    def invoke_process(self, command):
        """Run one command. Returns (completed_process, launch_error)."""
        try:
            completed = subprocess.run(command, capture_output=True,
                                       timeout=self.timeout)
        except FileNotFoundError:
            return None, f"executable not found: {self.executable}"
        except PermissionError:
            return None, f"executable not runnable: {self.executable}"
        except subprocess.TimeoutExpired:
            return None, f"timeout after {self.timeout}s"
        return completed, None

    def load_result_json(self, result_json_path):
        """Read and contract-check the JSON produced. Returns (payload, error)."""
        try:
            with open(result_json_path, encoding="utf-8") as handle:
                payload = json.load(handle)
        except (OSError, UnicodeDecodeError) as error:
            return {}, f"cannot read JSON from {self.name}: {error}"
        except json.JSONDecodeError as error:
            return {}, f"invalid JSON from {self.name}: {error}"

        problems = validate_result_contract(payload, self.name)
        if problems:
            reported = payload if isinstance(payload, dict) else {}
            return reported, "contract violation: " + "; ".join(problems)

        return payload, None

    def check_against_invocation(self, payload, mode, input_path, output_path):
        """Check the declared JSON against what actually happened on disk."""
        if payload["mode"] != mode:
            return f"declared mode '{payload['mode']}' but was invoked as '{mode}'"

        actual_input = file_size(input_path)
        if payload["input_bytes"] != actual_input:
            return (f"declared input_bytes {payload['input_bytes']} but "
                    f"{os.path.basename(input_path)} is {actual_input} bytes")

        if not os.path.isfile(output_path):
            return f"no output written to {os.path.basename(output_path)}"

        actual_output = file_size(output_path)
        if payload["output_bytes"] != actual_output:
            return (f"declared output_bytes {payload['output_bytes']} but "
                    f"{os.path.basename(output_path)} is {actual_output} bytes")

        return None

    def run(self, mode, input_path, output_path, result_json_path,
            packet_bytes, robustness, pt=None, ft=None, rt=None):
        """Invoke the executable once and return an ExecutionOutcome."""
        # A run that produces nothing must not be able to reuse older artefacts.
        remove_previous_artifact(output_path)
        remove_previous_artifact(result_json_path)

        command = self.build_command(mode, input_path, output_path,
                                     result_json_path, packet_bytes, robustness,
                                     pt, ft, rt)

        completed, launch_error = self.invoke_process(command)
        if launch_error:
            return ExecutionOutcome(False, None, None, launch_error, {})

        exit_code = completed.returncode
        stdout = completed.stdout.decode("utf-8", errors="replace").strip()
        stderr = completed.stderr.decode("utf-8", errors="replace").strip()

        if not os.path.isfile(result_json_path):
            message = f"{self.name} produced no JSON at {result_json_path}"
            if stdout:
                message += f" (stdout: {stdout[:200]})"
            return ExecutionOutcome(False, exit_code, None, message, {})

        payload, json_error = self.load_result_json(result_json_path)
        if json_error:
            return ExecutionOutcome(False, exit_code, payload.get("rc"),
                                    json_error, payload)

        implementation_rc = payload["rc"]
        declared_error = payload.get("error", "")

        if exit_code != 0 and implementation_rc == 0:
            return ExecutionOutcome(
                False, exit_code, implementation_rc,
                f"inconsistent status: process exited {exit_code} but declared rc=0",
                payload)

        if exit_code != 0 or implementation_rc != 0:
            message = stderr or declared_error or (
                f"process exit {exit_code}, rc {implementation_rc}")
            return ExecutionOutcome(False, exit_code, implementation_rc,
                                    message, payload)

        mismatch = self.check_against_invocation(payload, mode, input_path,
                                                 output_path)
        if mismatch:
            return ExecutionOutcome(False, exit_code, implementation_rc,
                                    mismatch, payload)

        return ExecutionOutcome(True, exit_code, implementation_rc,
                                stderr or declared_error, payload)


def pair_directory(workdir, case, encoder, decoder):
    """Create and return the directory holding one case-and-pair's artefacts.

    The three names are nested rather than concatenated: joining them into a
    single tag could produce the same string for two different pairs, for
    instance ("a-vs-b", "c") and ("a", "b-vs-c"). All three are already
    constrained by SAFE_NAME, so the path stays inside workdir.
    """
    path = os.path.join(workdir, case["name"], encoder.name, decoder.name)
    try:
        os.makedirs(path, exist_ok=True)
    except OSError as error:
        raise ConfigurationError(f"cannot create work directory {path}: {error}")
    return path


def compress_with(implementation, case, pair_dir, prefix):
    """Compress the case input. Returns (stream_path, outcome)."""
    stream_path = os.path.join(pair_dir, prefix + ".cmp")
    outcome = implementation.run(
        "compress", case["input"], stream_path,
        os.path.join(pair_dir, prefix + ".json"),
        case["packet_bytes"], case["R"], case["pt"], case["ft"], case["rt"],
    )
    return stream_path, outcome


def check_round_trip(implementation, compression_succeeded, compressed_path,
                     case, pair_dir, label):
    """Decompress one stream and check it reproduces the case input."""
    if not compression_succeeded:
        return RoundTripCheck(False, "upstream compress failed", None)

    decompressed_path = os.path.join(pair_dir, label + ".out")
    outcome = implementation.run(
        "decompress", compressed_path, decompressed_path,
        os.path.join(pair_dir, label + ".json"),
        case["packet_bytes"], case["R"],
    )
    if not outcome.succeeded:
        return RoundTripCheck(False, outcome.error_message, None)

    matches, detail = reconstruction_matches(case["input"], decompressed_path)
    return RoundTripCheck(matches, detail, sha256_file(decompressed_path))


def build_result_row(case, encoder, decoder, encoder_compression,
                     decoder_compression, encoder_stream, decoder_stream,
                     stream_comparison, round_trips):
    """Assemble one public result record."""
    identical, difference, comparison_detail = stream_comparison

    row = {
        "case": case["name"],
        "encoder": encoder.name,
        "decoder": decoder.name,
        "packet_bytes": case["packet_bytes"],
        "F_bits": case["packet_bytes"] * 8,
        "R": case["R"],
        "pt": case["pt"],
        "ft": case["ft"],
        "rt": case["rt"],
        "input_sha256": sha256_file(case["input"]),
        "input_bytes": file_size(case["input"]),
        "encoder_process_exit_code": encoder_compression.process_exit_code,
        "encoder_implementation_rc": encoder_compression.implementation_rc,
        "encoder_reported_impl": encoder_compression.reported_impl,
        "encoder_compress_error": encoder_compression.error_message,
        "encoder_compressed_bytes": file_size(encoder_stream),
        "encoder_compressed_sha256": sha256_file(encoder_stream),
        "decoder_process_exit_code": decoder_compression.process_exit_code,
        "decoder_implementation_rc": decoder_compression.implementation_rc,
        "decoder_reported_impl": decoder_compression.reported_impl,
        "decoder_compress_error": decoder_compression.error_message,
        "decoder_compressed_bytes": file_size(decoder_stream),
        "decoder_compressed_sha256": sha256_file(decoder_stream),
        "E_bit_identical": identical,
        "E_detail": comparison_detail,
        "E_first_difference": difference,
    }

    for letter, result_key, check in round_trips:
        row[result_key] = check.reconstructed_ok
        row[f"{letter}_detail"] = check.detail
        row[f"{letter}_output_sha256"] = check.output_sha256

    return row


def run_pair(encoder, decoder, case, workdir, results):
    """Evaluate one unique pair of implementations for one case.

    Both cross-decoding directions are evaluated. Appends one record to
    `results` and returns True when every reconstruction check passed;
    bit-identity alone does not affect the returned value.
    """
    pair_dir = pair_directory(workdir, case, encoder, decoder)

    encoder_stream, encoder_compression = compress_with(
        encoder, case, pair_dir, "encoder")
    decoder_stream, decoder_compression = compress_with(
        decoder, case, pair_dir, "decoder")

    round_trips = [
        # A: the encoder reads back its own stream.
        ("A", "A_encoder_roundtrip", check_round_trip(
            encoder, encoder_compression.succeeded, encoder_stream,
            case, pair_dir, "A")),
        # B: the decoder reads back its own stream.
        ("B", "B_decoder_roundtrip", check_round_trip(
            decoder, decoder_compression.succeeded, decoder_stream,
            case, pair_dir, "B")),
        # C: the decoder reads the encoder's stream.
        ("C", "C_encoder_to_decoder", check_round_trip(
            decoder, encoder_compression.succeeded, encoder_stream,
            case, pair_dir, "C")),
        # D: the encoder reads the decoder's stream.
        ("D", "D_decoder_to_encoder", check_round_trip(
            encoder, decoder_compression.succeeded, decoder_stream,
            case, pair_dir, "D")),
    ]

    # E: are both compressed streams byte-for-byte identical?
    stream_comparison = compare_streams(encoder_stream, decoder_stream)

    results.append(build_result_row(
        case, encoder, decoder, encoder_compression, decoder_compression,
        encoder_stream, decoder_stream, stream_comparison, round_trips))

    return all(check.reconstructed_ok for _, _, check in round_trips)


def load_json_file(path, description):
    """Read and parse a JSON file, reporting failures as configuration errors."""
    try:
        with open(path, encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, UnicodeDecodeError) as error:
        raise ConfigurationError(f"cannot read {description}: {error}")
    except json.JSONDecodeError as error:
        raise ConfigurationError(f"invalid JSON in {description}: {error}")


def build_implementations(config, default_timeout, config_dir):
    """Validate the configuration and build the Implementation objects."""
    if not isinstance(config, dict):
        raise ConfigurationError(
            f"configuration root must be an object, got {type(config).__name__}")

    declared = config.get("implementations")
    if not isinstance(declared, list):
        raise ConfigurationError("configuration 'implementations' must be a list")

    implementations = [
        Implementation(spec, default_timeout, config_dir) for spec in declared
    ]
    if len(implementations) < 2:
        raise ConfigurationError("at least two implementations are required")

    # Names index the artefact filenames, so duplicates would make two
    # implementations overwrite each other's results.
    seen_names = set()
    duplicate_names = set()
    for implementation in implementations:
        if implementation.name in seen_names:
            duplicate_names.add(implementation.name)
        else:
            seen_names.add(implementation.name)

    if duplicate_names:
        raise ConfigurationError("implementation names must be unique: "
                                 + ", ".join(sorted(duplicate_names)))

    return implementations


def validate_case(case, cases_dir):
    """Validate one test case and return it with its input path resolved."""
    if not isinstance(case, dict):
        raise ConfigurationError(f"case must be an object, got {type(case).__name__}")

    name = require_safe_name(case.get("name"), "case name")
    declared_input = require_non_empty_string(case.get("input"),
                                              f"case {name!r} 'input'")
    packet_bytes = require_positive_integer(case.get("packet_bytes"),
                                            f"case {name!r} 'packet_bytes'")

    robustness = case.get("R")
    if not is_plain_integer(robustness) or not (
            MIN_ROBUSTNESS <= robustness <= MAX_ROBUSTNESS):
        raise ConfigurationError(f"case {name!r} 'R' must be an integer in "
                                 f"[{MIN_ROBUSTNESS}, {MAX_ROBUSTNESS}]")

    # The standard fixes no range for the pt/ft/rt countdown periods, so only
    # the type is checked here.
    for field_name in ("pt", "ft", "rt"):
        if not is_plain_integer(case.get(field_name)):
            raise ConfigurationError(f"case {name!r} '{field_name}' must be an integer")

    # Relative paths resolve against the cases file, not the CWD.
    input_path = declared_input
    if not os.path.isabs(input_path):
        input_path = os.path.normpath(os.path.join(cases_dir, input_path))

    if not os.path.exists(input_path):
        raise ConfigurationError(f"case {name!r} input not found: {input_path}")
    if not os.path.isfile(input_path):
        raise ConfigurationError(f"case {name!r} input is not a regular file: "
                                 f"{input_path}")

    size = os.path.getsize(input_path)
    if size == 0 or size % packet_bytes != 0:
        raise ConfigurationError(
            f"case {name!r} input is {size} bytes, not a non-zero multiple of "
            f"packet_bytes={packet_bytes}")

    resolved = dict(case)
    resolved["input"] = input_path
    return resolved


def validate_cases(cases, cases_dir):
    """Validate the whole cases file, preserving file order."""
    if not isinstance(cases, list):
        raise ConfigurationError(f"cases file must be a list, got "
                                 f"{type(cases).__name__}")
    if not cases:
        raise ConfigurationError("cases file is empty")

    validated = []
    seen_names = set()
    for case in cases:
        resolved = validate_case(case, cases_dir)
        if resolved["name"] in seen_names:
            raise ConfigurationError(f"duplicate case name: {resolved['name']}")
        seen_names.add(resolved["name"])
        validated.append(resolved)

    return validated


def write_json_results(path, results):
    """Write every result record as one JSON array."""
    try:
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(results, handle, indent=2, sort_keys=True)
    except OSError as error:
        raise ConfigurationError(f"cannot write {path}: {error}")


def write_csv_results(path, results):
    """Write every result record as one CSV table, nested objects as JSON text."""
    if not results:
        return

    try:
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        with open(path, "w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(results[0].keys()))
            writer.writeheader()
            for row in results:
                writer.writerow({
                    key: json.dumps(value) if isinstance(value, dict) else value
                    for key, value in row.items()
                })
    except OSError as error:
        raise ConfigurationError(f"cannot write {path}: {error}")


def parse_arguments(argv):
    """Define and parse the command line."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--config", required=True,
                        help="JSON file declaring the external executables")
    parser.add_argument("--cases", required=True,
                        help="JSON file declaring the test cases")
    parser.add_argument("--workdir", required=True)
    parser.add_argument("--out-json", required=True)
    parser.add_argument("--out-csv")
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)

    try:
        require_positive_integer(arguments.timeout, "--timeout")
        config = load_json_file(arguments.config, "configuration file")
        cases_document = load_json_file(arguments.cases, "cases file")

        implementations = build_implementations(
            config, arguments.timeout,
            os.path.dirname(os.path.abspath(arguments.config)))
        cases = validate_cases(
            cases_document, os.path.dirname(os.path.abspath(arguments.cases)))
    except ConfigurationError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    workdir = os.path.abspath(arguments.workdir)
    try:
        os.makedirs(workdir, exist_ok=True)
    except OSError as error:
        print(f"error: cannot create workdir {workdir}: {error}", file=sys.stderr)
        return 2

    results = []
    every_check_passed = True

    for case in cases:
        # Each unique pair once; C and D cover both cross-decoding directions.
        for encoder_index, encoder in enumerate(implementations):
            for decoder in implementations[encoder_index + 1:]:
                try:
                    passed = run_pair(encoder, decoder, case, workdir, results)
                except ConfigurationError as error:
                    print(f"error: {error}", file=sys.stderr)
                    return 2
                if not passed:
                    every_check_passed = False

                row = results[-1]
                print(f"{case['name']:24s} {encoder.name} vs {decoder.name}: "
                      f"A={row['A_encoder_roundtrip']} "
                      f"B={row['B_decoder_roundtrip']} "
                      f"C={row['C_encoder_to_decoder']} "
                      f"D={row['D_decoder_to_encoder']} "
                      f"E={row['E_bit_identical']}")

    try:
        write_json_results(arguments.out_json, results)
        if arguments.out_csv:
            write_csv_results(arguments.out_csv, results)
    except ConfigurationError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    print(f"\nresults: {arguments.out_json}")
    return 0 if every_check_passed else 1


if __name__ == "__main__":
    sys.exit(main())
