//! Cross-language differential test (issue #105).
//!
//! Drives the low-level `Compressor` over the shared corpus in
//! `test-vectors/differential/cases.json` and asserts byte-identical output to
//! the C reference golden — the systemic guard against "fixed-in-C-not-ported"
//! drift (D0=M0 #98, MSB-aligned NOT #103), which only shows up for
//! non-byte-aligned F / non-zero M0.
//!
//! The low-level API is crate-internal, so this lives in `src/` (an integration
//! test in `tests/` would see only the byte-only public `compress`). The crate
//! has no JSON dependency, so the regular fixture is hand-parsed and the
//! per-packet flags are recomputed with the documented flag-timing algorithm
//! (validated against C by the generator's self-check).

use crate::bitvector::BitVector;
use crate::compress::{CompressionParams, Compressor};

fn fixture() -> String {
    let path = concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/../../test-vectors/differential/cases.json"
    );
    std::fs::read_to_string(path).unwrap_or_else(|e| panic!("read {path}: {e}"))
}

fn field_int(chunk: &str, key: &str) -> usize {
    let needle = format!("\"{key}\":");
    let start = chunk
        .find(&needle)
        .unwrap_or_else(|| panic!("missing {key}"))
        + needle.len();
    chunk[start..]
        .trim_start()
        .split(|c: char| !c.is_ascii_digit())
        .next()
        .unwrap()
        .parse()
        .unwrap_or_else(|_| panic!("bad int for {key}"))
}

fn field_str(chunk: &str, key: &str) -> String {
    let needle = format!("\"{key}\": \"");
    let start = chunk
        .find(&needle)
        .unwrap_or_else(|| panic!("missing {key}"))
        + needle.len();
    let end = chunk[start..].find('"').unwrap() + start;
    chunk[start..end].to_string()
}

fn field_str_array(chunk: &str, key: &str) -> Vec<String> {
    let needle = format!("\"{key}\": [");
    let start = chunk
        .find(&needle)
        .unwrap_or_else(|| panic!("missing {key}"))
        + needle.len();
    let end = chunk[start..].find(']').unwrap() + start;
    chunk[start..end]
        .split(',')
        .map(|s| s.trim().trim_matches('"').to_string())
        .filter(|s| !s.is_empty())
        .collect()
}

fn hex_to_bytes(s: &str) -> Vec<u8> {
    (0..s.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap())
        .collect()
}

fn bytes_to_hex(b: &[u8]) -> String {
    use std::fmt::Write as _;
    b.iter()
        .fold(String::with_capacity(b.len() * 2), |mut acc, x| {
            let _ = write!(acc, "{x:02x}");
            acc
        })
}

/// Recompute the per-packet flags exactly as the C reference does
/// (init phase forces ft=rt=1, pt=0 for the first R+1 packets; otherwise
/// countdown counters fire at 1 and reload). Validated against C.
fn flags_for(npk: usize, r: usize, pt: usize, ft: usize, rt: usize) -> Vec<(bool, bool, bool)> {
    let (mut ftc, mut ptc, mut rtc) = (ft, pt, rt);
    let mut out = Vec::with_capacity(npk);
    for p in 0..npk {
        let (mut pt_f, mut ft_f, mut rt_f);
        if p == 0 {
            ft_f = true;
            rt_f = true;
            pt_f = false;
        } else {
            ft_f = ftc == 1;
            if ft_f {
                ftc = ft;
            } else {
                ftc -= 1;
            }
            pt_f = ptc == 1;
            if pt_f {
                ptc = pt;
            } else {
                ptc -= 1;
            }
            rt_f = rtc == 1;
            if rt_f {
                rtc = rt;
            } else {
                rtc -= 1;
            }
        }
        if p <= r {
            ft_f = true;
            rt_f = true;
            pt_f = false;
        }
        out.push((pt_f, ft_f, rt_f));
    }
    out
}

#[test]
fn differential_compress_matches_c_golden() {
    let doc = fixture();
    let chunks: Vec<&str> = doc.split("\"id\":").skip(1).collect();
    assert!(!chunks.is_empty(), "no cases parsed");

    for chunk in chunks {
        let id = chunk.split('"').nth(1).unwrap().to_string();
        let f = field_int(chunk, "F");
        let r = field_int(chunk, "R");
        let pt = field_int(chunk, "pt");
        let ft = field_int(chunk, "ft");
        let rt = field_int(chunk, "rt");
        let m0 = hex_to_bytes(&field_str(chunk, "m0"));
        let packets = field_str_array(chunk, "packets");
        let expected = field_str(chunk, "compressed");

        let mask = BitVector::from_bytes(&m0, f);
        let r_u8 = u8::try_from(r).unwrap();
        let mut comp = Compressor::new(f, Some(&mask), r_u8, pt, ft, rt)
            .unwrap_or_else(|e| panic!("case {id}: Compressor::new: {e:?}"));

        let flags = flags_for(packets.len(), r, pt, ft, rt);
        let mut out: Vec<u8> = Vec::new();
        for (pkt, &(pt_f, ft_f, rt_f)) in packets.iter().zip(flags.iter()) {
            let input = BitVector::from_bytes(&hex_to_bytes(pkt), f);
            let params = CompressionParams {
                new_mask_flag: pt_f,
                send_mask_flag: ft_f,
                uncompressed_flag: rt_f,
            };
            let bb = comp
                .compress_packet(&input, &params)
                .unwrap_or_else(|e| panic!("case {id}: compress_packet: {e:?}"));
            out.extend_from_slice(&bb.to_bytes());
        }

        assert_eq!(
            bytes_to_hex(&out),
            expected,
            "case {id}: Rust compressed output diverges from the C reference"
        );
    }
}
