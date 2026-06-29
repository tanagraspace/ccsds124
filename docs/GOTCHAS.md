# CCSDS 124.0-B-1 Implementer's Guide

**⚠️ READ THIS BEFORE IMPLEMENTING!**

This is a practical guide to implementing **CCSDS 124.0-B-1** (the lossless housekeeping-telemetry compression standard, based on ESA's POCKET+ algorithm) so that your output is **byte-identical to the ESA reference implementation**. It documents 21 byte-level pitfalls — most of them not obvious from the standard alone — found by validating six independent implementations byte-for-byte against the ESA reference, with the C implementation additionally run against the UAB conformance suite.

## What this guide refers to

Four things are referenced throughout — keep them distinct:

- **The standard** — *CCSDS 124.0-B-1, Robust Compression of Fixed-Length Housekeeping Data* (Blue Book, February 2023), the normative specification. All section and equation citations in this guide refer to it.
- **The ESA reference implementation** — ESA/ESOC's original C program in [`test-vector-generator/c-reference/`](../test-vector-generator/c-reference/) (`pocket_compress.c` / `pocket_decompress.c`). It is the **conformance oracle**: "byte-identical output" everywhere in this guide means identical to what this program produces. It is kept verbatim — not modified, and not one of the implementations below.
- **This repository's implementations** — the six independent, interoperable implementations in [`implementations/`](../implementations/) (C, C++, Python, Go, Rust, Java) that this guide was written to get right.
- **The UAB conformance suite** — a 24,900-vector cross-validation set (valid and invalid parameters, packet loss, fuzzed packets) produced by the UAB team under CNES supervision. The C implementation is additionally validated against it; see [Conformance](CONFORMANCE.md).

Each pitfall is tagged by where the requirement comes from:

- 📘 **Spec-mandated** — defined by the standard (cited by section/equation). Getting it wrong is non-conformant.
- 🔧 **Conformance detail** — the standard is silent or ambiguous here; the behavior shown is what the ESA reference does, and matching it is what makes your output byte-identical and lets you pass cross-validation.

Most entries include:
- ✅ **What the spec says** (with the citation) — or, for conformance details, **what the reference does**
- ❌ **Common mistake** that seems reasonable but is wrong
- 🔧 **Correct implementation**
- 📊 **Impact** when you get it wrong

---

## Table of Contents

### Compression Gotchas

1. [Initialization Phase: First Rₜ+1 Packets (Not Rₜ+2!)](#1-initialization-phase-first-rₜ1-packets-not-rₜ2)
2. [Flag Timing: Countdown Counters, Not Modulo Arithmetic](#2-flag-timing-countdown-counters-not-modulo-arithmetic)
3. [Vₜ Calculation: Start from Rₜ+1, Not Position 2!](#3-vₜ-calculation-start-from-rₜ1-not-position-2)
4. [Component kₜ: Inverted Mask Values (Not Direct Mask Values!)](#4-component-kₜ-inverted-mask-values-not-direct-mask-values)
5. [Component kₜ: Forward Extraction Order (Not Reverse!)](#5--component-kₜ-forward-extraction-order-not-reverse)
6. [Reference Implementation's Final Padding (FIXED)](#-gotcha-6-reference-implementations-final-padding-fixed)
7. [cₜ Calculation: Include Current Packet's pₜ Flag!](#7-cₜ-calculation-include-current-packets-pₜ-flag)
18. [D₀ Must Be Zero at Initialization (Not M₀!)](#18-d₀-must-be-zero-at-initialization-not-m₀)
19. [Bitvector NOT: MSB-Aligned Masking for Non-Byte-Aligned Lengths](#19-bitvector-not-msb-aligned-masking-for-non-byte-aligned-lengths)

### High-Level API Gotchas

16. [Packet Byte-Boundary Padding](#16-packet-byte-boundary-padding)
17. [COUNT Extended Format ('111'): Include Terminating '1' in Value](#17-count-extended-format-111-include-terminating-1-in-value)

### Decompression Gotchas

8. [COUNT Decoding: '10' is Terminator, Not a Value](#8-count-decoding-10-is-terminator-not-a-value)
9. [kₜ Reading Order: Forward (Low to High Position)](#9-kₜ-reading-order-forward-low-to-high-position)
10. [kₜ Bit Interpretation: Inverted Mask Values](#10-kₜ-bit-interpretation-inverted-mask-values)
11. [Unpredictable Bits: Insert in Reverse Order (BE)](#11-unpredictable-bits-insert-in-reverse-order-be)
12. [Horizontal XOR Mask Decoding](#12-horizontal-xor-mask-decoding)
13. [ḋₜ Flag Optimization](#13-ḋₜ-flag-optimization)
14. [Vₜ=0 Special Case: Toggle Mask Bits](#14-vₜ0-special-case-toggle-mask-bits)
15. [Extraction Mask: cₜ Affects Which Bits to Read](#15-extraction-mask-cₜ-affects-which-bits-to-read)
20. [Decoder Must Validate Bitstream Integrity to Reject Corrupt Packets](#20-decoder-must-validate-bitstream-integrity-to-reject-corrupt-packets)
21. [Decoder Cross-Validation: Mask Synchronization and Accuracy Guarantees](#21-decoder-cross-validation-mask-synchronization-and-accuracy-guarantees)

---

## 1. Initialization Phase: First Rₜ+1 Packets (Not Rₜ+2!)

📘 **Spec-mandated** — §3.3.2 c–d: ḟₜ=1 and ṙₜ=1 for the first Rₜ+1 packets.

### ✅ What the Spec Says

The CCSDS spec (Section 3.3.2 c–d) mandates ḟₜ=1 and ṙₜ=1 for the **first Rₜ+1 packets** (t ≤ Rₜ). ṗₜ is user-specified per the spec; the reference implementation uses ṗₜ=0 during initialization, and matching it is required for byte-identical output.

### ❌ Common Mistake

Applying init phase to Rₜ+2 packets instead of Rₜ+1 packets.

**Example for Rₜ=1:**
- Packet 0 (first packet): init phase → ḟₜ=1, ṙₜ=1, ṗₜ=0
- Packet 1 (second packet): init phase → ḟₜ=1, ṙₜ=1, ṗₜ=0
- Packet 2 (third packet): **❌ WRONG: still in init phase**

### 🔧 Correct Implementation

```c
// Correct condition (0-based indexing)
if (packet_index <= Rt) {
    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;
    params.new_mask_flag = 0;
}
```

**Example for Rₜ=1:**
- Packet 0 (i=0): init phase → ḟₜ=1, ṙₜ=1, ṗₜ=0
- Packet 1 (i=1): init phase → ḟₜ=1, ṙₜ=1, ṗₜ=0
- Packet 2 (i=2): ✅ **normal operation begins**

### 📊 Impact

- **Divergence:** Within first 10-20 packets
- **Symptom:** Flag timing is off by one packet throughout entire stream
- **Detection:** Compare flag values with reference for packets 0-10

---

## 2. Flag Timing: Countdown Counters, Not Modulo Arithmetic

🔧 **Conformance detail** — the ṗₜ/ḟₜ/ṙₜ flags and their period parameters are defined in §3.3.2; the exact first-trigger packet (`period + 1`) and countdown behavior match the ESA reference.

### ✅ What the reference does

The spec (§3.3.2) defines the ṗₜ/ḟₜ/ṙₜ flags and their period parameters but not the exact per-packet cadence. The ESA reference uses **countdown counters** that start at the period limit and decrement each packet; a flag triggers when its counter reaches 1, then the counter reloads.

### ❌ Common Mistake

Using modulo arithmetic: `(packet_num % period) == 0`

This triggers **one packet too early**:
- ṗₜ=1 at packets: **10, 20, 30, 40...** ❌ WRONG
- ḟₜ=1 at packets: **20, 40, 60, 80...** ❌ WRONG
- ṙₜ=1 at packets: **50, 100, 150, 200...** ❌ WRONG

### 🔧 Correct Implementation

**Pattern for Rₜ=1, periods (10, 20, 50):**
- ṗₜ=1 (new mask) at packets: **11, 21, 31, 41...** ✅ CORRECT
- ḟₜ=1 (send mask) at packets: **21, 41, 61, 81...** ✅ CORRECT
- ṙₜ=1 (uncompressed) at packets: **51, 101, 151, 201...** ✅ CORRECT

**Key insight:** The first trigger lands at `period + 1`, not `period`: the very first packet is the initialization packet and is emitted before the period counters start counting, so each counter completes its first full cycle one packet later. (For Rₜ=1 this happens to equal `period + Rₜ`, but the offset is always +1, regardless of Rₜ.)

**Example for pt_period=10** (countdown counter, matching the title — not modulo):
```c
// Counters start at the period limit. The first (init) packet is emitted
// before counting begins; from the next packet onward, decrement and fire
// when the counter reaches 1, then reload:
if (pt_counter == 1) {
    pt_flag = 1;
    pt_counter = pt_period;   // reload
} else {
    pt_flag = 0;
    pt_counter--;
}
// For pt_period = 10 this fires at packets 11, 21, 31, ...
```

### 📊 Impact

- **Divergence:** Byte 30-50 (depending on periods)
- **Symptom:** Wrong mask updates, missing qₜ components, wrong compression modes
- **Size error:** ±10% output size difference
- **Detection:** Check flag values for packets 10-12, 20-22

---

## 3. Vₜ Calculation: Start from Rₜ+1, Not Position 2!

📘 **Spec-mandated** — §5.3.2.2, Eq 14 (the Cₜ window and Vₜ = Rₜ + Cₜ).

### ✅ What the Spec Says

Per CCSDS Section 5.3.2.2 (Equation 14), Cₜ is the maximum integer with Cₜ ≤ min(t, 15) − Rₜ for which all **D_{t′} = 0** for **t′ ∈ {t−Rₜ−1, t−Rₜ−2, …, t−Rₜ−Cₜ}** — i.e., D_{t-i} = 0 for all **Rₜ < i ≤ Rₜ + Cₜ**.

### ❌ Common Mistake

Starting from i=2 regardless of Rₜ value:

```c
// WRONG: Always starts from i=2
for (int i = 2; i <= 15; i++) {
    if (Dt[t-i] != 0) break;
    Ct++;
}
```

**Why this seems reasonable:** For Rₜ=1 (the first test vectors most implementations validate against), the window starts at i=2, and it is easy to hard-code that.

**Why it's wrong:** The window starts at position **Rₜ+1**, not position 2. For Rₜ=1, this happens to be 2. For Rₜ=2, it's 3. The general formula is:

```
start_position = Rt + 1
```

### 🔧 Correct Implementation

```c
// CORRECT: Start from Rt+1 positions back
int Ct = 0;
for (int i = Rt + 1; i <= 15 && i <= t; i++) {  // i starts from Rt+1!
    size_t hist_idx = (history_index - i + HISTORY_SIZE) % HISTORY_SIZE;
    if (change_history[hist_idx] != 0) break;  // Found a change
    Ct++;
    if (Ct >= 15 - Rt) break;  // Maximum Ct
}
Vt = Rt + Ct;
```

**Example for Rₜ=1:**
- Start from i=2 (Rₜ+1=2)
- Check D_{t-2}, D_{t-3}, ...
- Skip D_{t-1}

**Example for Rₜ=2:**
- Start from i=3 (Rₜ+1=3)
- Check D_{t-3}, D_{t-4}, ...
- Skip D_{t-1} AND D_{t-2}

### 📊 Impact

- **Divergence:** Within first 5-10 packets for Rₜ>1
- **Symptom:** Wrong Vₜ values in component hₜ, byte-level mismatches
- **Size error:** Small (few bits per packet), but compounds over stream
- **Detection:** Print Vₜ values for packets 0-5 and compare with reference
- **Affected tests:** R=2 tests (housekeeping, venus-express) will have byte mismatches

---

## 4. Component kₜ: Inverted Mask Values (Not Direct Mask Values!)

📘 **Spec-mandated** — §5.3.3.1, Eq 17 (yₜ = BE(<~Mₜ>, Xₜ); the `~Mₜ` inversion is explicit here) and Eq 19 (kₜ = yₜ).

### ✅ What the Spec Says

The kₜ component encodes mask values at changed positions, but **outputs the INVERSE** of the mask bits.

**CCSDS spec defines (Equations 17 and 19):** kₜ = yₜ = BE(<~Mₜ>, Xₜ) — the inversion (`~Mₜ`) is explicit in the formula (the Section 5.1 overview prose calls it "information on the mask values for each change").

**In practice:** Output '1' for positive updates (mask changed to 0), '0' for negative updates (mask changed to 1)

### ❌ Common Mistake

Extracting mask values directly:

```c
// WRONG: Direct mask extraction
kt = BE(mask, Xt);  // ❌ Extracts mask bits directly
```

### 🔧 Correct Implementation

```c
// CORRECT: Extract INVERTED mask values
for (int i = 0; i < mask.length; i++) {
    inverted_mask[i] = !mask[i];  // Invert the entire mask
}
kt = BE(inverted_mask, Xt);  // Extract inverted values at changed positions
```

**Correct encoding:**
- When Xₜ has '1' at position i (bit changed):
  - If mask[i] = 0 (now predictable): output **1** in kₜ
  - If mask[i] = 1 (now unpredictable): output **0** in kₜ
- This is the **INVERSE** of the mask values

**Example:**
- Xₜ = '1' at positions [43, 142]
- Mask values at those positions: [0, 0] (both predictable)
- kₜ output: **11** (not 00!)

### 📊 Impact

- **Divergence:** Byte 200-300 (30-50% into stream)
- **Symptom:** 1-bit error per changed position
- **Size error:** No size change (bit-level corruption)
- **Why this matters:**
  - The eₜ flag indicates if there are "positive updates" (mask bits changed from 1→0)
  - kₜ outputs '1' to mark these positive updates
  - Extracting mask values directly gives the opposite encoding

---

## 5. ⭐ Component kₜ: Forward Extraction Order (Not Reverse!)

📘 **Spec-mandated** — §5.2.4 Bit Extraction (Eq 11; positions counted from the MSB) and §5.3.3.1, Eq 17/19 (kₜ = yₜ = BE(<~Mₜ>, Xₜ)).

**⭐ Latest Discovery - December 2025**

### ✅ What the Spec Says (Subtly!)

CCSDS 124.0-B-1 Equations 17 and 19 define:
```
kₜ = yₜ = BE(<~Mₜ>, Xₜ)
```

and the BE function is explicitly defined with reverse order:
```
BE(a, b) = a_{g_{H(b)-1}} ∥ ... ∥ a_{g₁} ∥ a_{g₀}
```
(gᵢ ordered from highest to lowest position)

**The subtlety:** both BE operands here are *reversed* vectors — Xₜ is defined reversed (Eq. 16) and the mask is reversed as <~Mₜ>. BE's highest-position-first order over reversed vectors maps back to **FORWARD ORDER over the original packet positions** (lowest position index to highest) — which is DIFFERENT from the order used for uₜ = BE(Iₜ, Mₜ), where the operands are not reversed.

### ❌ Common Mistake

Using the same extraction order as BE (Bit Extract) for unpredictable bits, which extracts in **REVERSE ORDER** (highest position to lowest):

```c
// WRONG: Reverse order (works for BE, but NOT for kt!)
for (int i = num_positions - 1; i >= 0; i--) {
    output_bit(mask_values[positions[i]]);
}
```

**Why this seems reasonable:**
- The BE operation (for uₜ component) uses reverse order
- The reference implementation processes words from high to low
- It's natural to assume all bit extraction uses the same order

**Example of wrong output:**
- Xₜ has '1' at positions: 141, 142, 431
- Mask values: [1, 1, 0] (at positions 141, 142, 431)
- Inverted: [0, 0, 1]
- **Wrong (reverse):** Extracts 431, 142, 141 → outputs `100` ❌
- **Correct (forward):** Extracts 141, 142, 431 → outputs `001` ✅

### 🔧 Correct Implementation

**Two separate functions needed:**

```c
// For kₜ component: FORWARD order (low to high position)
int ccsds124_bit_extract_forward(bitbuffer_t *output,
                                const bitvector_t *data,
                                const bitvector_t *mask) {
    // Collect positions where mask has '1' bits
    for (int i = 0; i < mask->length; i++) {
        if (get_bit(mask, i)) {
            positions[count++] = i;
        }
    }

    // Extract in FORWARD order (low to high position)
    for (int i = 0; i < count; i++) {  // ✅ Forward: i increasing
        output_bit(get_bit(data, positions[i]));
    }
}

// For uₜ component (BE): REVERSE order (high to low position)
int ccsds124_bit_extract(bitbuffer_t *output,
                        const bitvector_t *data,
                        const bitvector_t *mask) {
    // Collect positions where mask has '1' bits
    for (int i = 0; i < mask->length; i++) {
        if (get_bit(mask, i)) {
            positions[count++] = i;
        }
    }

    // Extract in REVERSE order (high to low position)
    for (int i = count - 1; i >= 0; i--) {  // ✅ Reverse: i decreasing
        output_bit(get_bit(data, positions[i]));
    }
}
```

**Usage:**
```c
// Component kₜ (mask values at changed positions)
inverted_mask = NOT(mask);
ccsds124_bit_extract_forward(output, inverted_mask, Xt);  // ✅ Forward order

// Component uₜ (unpredictable bits)
ccsds124_bit_extract(output, input, mask);  // ✅ Reverse order
```

### 📊 Impact

**Before fix (using reverse order for kₜ):**
- **Divergence in late-stream packets** (typically 40-60% into output)
- Symptom: Multi-bit shift errors in packets with mask changes
- Error pattern: Bit-reversed kₜ component causes cumulative offset
- Packets without mask changes still match (Xₜ = ∅, so no kₜ output)

**After fix (using forward order for kₜ):**
- **✅ PERFECT BYTE-FOR-BYTE MATCH!**
- No divergence in compressed data
- Perfect byte-for-byte match with fixed reference implementation
- Prefix match: **100%** of generated compressed data

### 🔍 Why This Was Hard to Find

1. **Double reversal is subtle** - kₜ = BE(<~Mₜ>, Xₜ) applies BE to *reversed* vectors (Xₜ is reversed by Eq. 16, the mask by <~Mₜ>), flipping BE's reverse order into forward packet order
2. **Spec DOES specify BE as reverse** - Natural to assume kₜ uses same order as the explicitly-defined BE function over unreversed operands (as uₜ does)
3. **Reasonable assumption fails** - Reusing your uₜ bit-extraction routine for kₜ seems logical but is wrong
4. **Late divergence** - Error only appears when kₜ is encoded (packets with H(Xₜ) > 0 and eₜ=1)
5. **Subtle symptom** - Produces valid output, just with bits in wrong order
6. **Reference code complexity** - Reference builds kₜ in a temporary buffer with backwards indexing, then reverses when concatenating
7. **Easy to misderive from spec** - The double reversal is invisible unless you expand Eq. 19 carefully; testing against reference output is what exposed it

### 🎯 Key Lesson

**Don't assume all bit extraction uses the same order!**

- **BE (unpredictable bits):** Reverse order (highest position first)
- **kₜ (mask values):** Forward order (lowest position first)

The reference implementation has different code paths for these operations, and the order matters!

---

## Detection Strategies

### Quick Smoke Tests

1. **Packet 0-2:** Compare first 3 packets byte-by-byte
   - Tests: Init phase, Vₜ calculation
   - Should match perfectly if #1-3 are correct

2. **Packets 10-12:** Check flag values
   - Tests: Flag timing
   - Print pt, ft, rt for each packet

3. **Packets 20-30:** Check for divergence
   - Tests: All flag-related issues
   - Should match if #1-3 are correct

4. **Packets 30-50:** Check kₜ component
   - Tests: kₜ inversion and extraction order
   - Look for systematic 1-bit or 2-bit errors

### Debugging Workflow

```bash
# 1. Generate debug output
./compress input.bin > output.bin 2> debug.log

# 2. Compare sizes
ls -l output.bin reference.bin  # Should match exactly

# 3. Find first divergence
diff -y <(xxd reference.bin) <(xxd output.bin) | head -50

# 4. Check flags
grep "packet [0-9]*: pt=" debug.log | head -30

# 5. Check Vt values
grep "Vt=" debug.log | head -10

# 6. Check kt encoding
grep "kt.*bits" debug.log
```

### Reference Values (Rₜ=1, periods 10/20/50)

**Packet indices (0-based) vs packet numbers (1-based):**
```
i=0  → packet 1  : ft=1, rt=1, pt=0  (init)
i=1  → packet 2  : ft=1, rt=1, pt=0  (init)
i=2  → packet 3  : ft=0, rt=0, pt=0  (normal starts)
i=10 → packet 11 : ft=0, rt=0, pt=1  (first pt trigger)
i=20 → packet 21 : ft=1, rt=0, pt=1  (first ft trigger + pt)
i=30 → packet 31 : ft=0, rt=0, pt=1  (pt only)
i=40 → packet 41 : ft=1, rt=0, pt=1  (ft + pt)
i=50 → packet 51 : ft=0, rt=1, pt=1  (first rt trigger + pt)
```

**Vₜ progression for typical input (M₀ = 0):**
```
Packet 0: Vt=1 (Rt, init phase)
Packet 1: Vt=1 (Rt, init phase)
Packet 2: Vt=2 (Rt + Ct, where Ct=1 from D0=0)
Packet 3+: Varies based on mask changes
```

**Note:** D₀ is always 0 (see [Gotcha #18](#18-d₀-must-be-zero-at-initialization-not-m₀)). At t=0 the caller sets M₋₁ = M₀, so D₀ = M₀ XOR M₀ = 0 regardless of M₀'s value.

---

## Testing Checklist

Before declaring your implementation "working":

- All compressed bytes match reference output exactly
- Output size matches reference byte-for-byte
- First 10 packets compress correctly
- Flag triggers occur at correct boundaries (test pt, ft, rt periods)
- Mask transmission packets are correct
- Uncompressed packets trigger at correct intervals
- Vₜ values match reference for initialization and steady-state phases
- Vₜ calculation starts from Rₜ+1, not always from i=2
- cₜ calculation includes current packet's pₜ flag (Vₜ+1 total entries)
- No divergence before byte 300 (indicates kₜ issues)
- No systematic bit-shift errors (indicates ordering issues)
- kₜ component uses forward extraction order
- BE operation uses reverse extraction order
- Mask inversion is applied before kₜ extraction
- Both R=1 AND R=2 test vectors pass (different code paths!)
- D₀ = 0 at initialization (RLE(X₀) = '10' for any M₀)
- Bitvector NOT preserves zero padding in MSB-aligned non-byte-aligned vectors

---

## Common Symptoms and Diagnosis

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Divergence at byte 10-20 | Init phase wrong (#1) | Check Rₜ+1 condition |
| Divergence at byte 30-50 | Flag timing wrong (#2) | Check countdown logic |
| Divergence at byte 5-15 | Vₜ wrong (#3) | Start from Rₜ+1, not i=2 |
| Byte mismatch in R=2 tests | Vₜ start position wrong (#3) | Use i=Rₜ+1 not i=2 |
| 1-bit errors in kₜ | kₜ not inverted (#4) | Invert mask before extraction |
| 2-bit shift at byte 300+ | kₜ extraction order wrong (#5) | Use forward order for kₜ |
| Size off by 10%+ | Multiple flag issues | Check #1 and #2 |
| Size matches but content wrong | Bit-level issues | Check #4 and #5 |
| Size off by ~100 bytes (10KB test) | cₜ missing current flag (#7) | Include current pₜ in cₜ count |
| edge-cases fails, simple passes | cₜ calculation wrong (#7) | Check Vₜ+1 entries for cₜ |
| First 2 bits `11...` not `10` with non-zero M₀ | D₀ = M₀ instead of 0 (#18) | Set prev_mask = mask before t=0 |
| Extra 1-bits in non-byte-aligned vectors | NOT masks low bits not high (#19) | Use MSB-aligned byte mask in NOT |

---

## Final Notes

**Trust your test vectors!**

When the first 40 packets match perfectly, your fundamental algorithm is correct. Don't second-guess working code based on later divergences—investigate the specific point of failure.

**The spec is not always clear.**

Many of these gotchas are not explicitly stated in CCSDS 124.0-B-1. The only way to discover them is through:
1. Careful reading of the reference implementation
2. Byte-by-byte comparison with reference output
3. Systematic debugging of divergence points

**Order matters more than you think.**

- Packet numbering: 0-based vs 1-based
- Bit indexing: LSB-first vs MSB-first
- Bit extraction: forward vs reverse order
- Flag timing: countdown vs modulo

Each of these seemingly minor differences will cause your implementation to fail.

---

## 🎯 Gotcha #6: Reference Implementation's Final Padding (FIXED)

🔧 **Conformance detail** — a fixed defect in the ESA reference's final-byte padding, not a spec requirement.

### ✅ Status: FIXED

The original ESA/ESOC reference implementation had a bug that added **2 extra null bytes** (`0x00 0x00`) at the end of the compressed output. This has been **fixed** in the current reference implementation.

### 📖 What Was Happening (Historical)

The original reference's `write_to_file()` function:
1. Wrote complete 32-bit words to the file
2. Called `fseek()` to rewind before unused bytes
3. Program exited **WITHOUT** calling `fclose(outputFile)`
4. **Bug:** The fseek didn't take effect, leaving extra padding bytes

### 🔧 The Fix

The reference implementation has been updated with two changes:
1. Added `fclose(outputFile)` before program exit
2. Added `ftruncate()` in `write_to_file()` to properly truncate the file at the correct position

See [../test-vector-generator/c-reference/CHANGES.md](../test-vector-generator/c-reference/CHANGES.md) for details.

### ✅ Current Status

**Test vectors have been regenerated with the fix:**
- All test vectors now contain only compressed data with no spurious padding
- Your implementation should match the reference byte-for-byte
- No workarounds needed for the 2-byte difference

### 💡 Correct Approach

- Your output should be exactly the compressed data
- Byte-boundary padding per packet (if used) is standard
- Expect perfect byte-for-byte matches with current test vectors

---

## 7. cₜ Calculation: Include Current Packet's pₜ Flag!

📘 **Spec-mandated** — §5.3.3.1, Eq 20 (cₜ counts the current packet's ṗₜ plus the previous Vₜ entries).

**⭐ Discovery - December 2025**

### ✅ What the Spec Says

Per CCSDS Section 5.3.3.1 (Equation 20), cₜ = 1 if the new_mask_flag (pₜ) was set **2 or more times** in the last Vₜ iterations.

### ❌ Common Mistake

Only checking historical pₜ flags without including the **current packet's** pₜ flag:

```c
// WRONG: Only checks history, misses current packet
int count = 0;
for (int i = 0; i < Vt; i++) {
    size_t hist_idx = (flag_history_index - 1 - i) % HISTORY_SIZE;
    if (new_mask_flag_history[hist_idx]) count++;
}
return (count >= 2) ? 1 : 0;
```

**Why this seems reasonable:** The current pₜ flag hasn't been stored in history yet when cₜ is computed.

**Why it's wrong:** The reference implementation stores the current pₜ flag BEFORE computing cₜ, so it includes Vₜ+1 total entries (current + Vₜ historical). Your implementation must match this behavior.

### 🔧 Correct Implementation

Either store the current flag before computing cₜ, or include it explicitly:

```c
// CORRECT: Include current packet's flag in the count
int ccsds124_compute_ct_flag(
    const ccsds124_compressor_t *comp,
    uint8_t Vt,
    int current_new_mask_flag  // Pass current packet's pt flag
) {
    if (Vt == 0) return 0;

    int count = 0;

    // Include current packet's flag
    if (current_new_mask_flag) count++;

    // Check Vt historical entries
    for (size_t i = 0; i < Vt && i < comp->t; i++) {
        size_t hist_idx = (comp->flag_history_index - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
        if (comp->new_mask_flag_history[hist_idx]) count++;
    }

    return (count >= 2) ? 1 : 0;  // ct=1 if 2+ flags set
}
```

**Reference behavior:**
- Stores `pt_history[pt_history_index] = pt` before computing cₜ
- Loops from `pt_history_index` to `pt_history_index + Vt` (inclusive)
- Total entries checked: Vₜ + 1 (current + Vₜ historical)

### 📊 Impact

- **Divergence:** Mid-stream (typically 500+ bytes into output)
- **Symptom:** Output size mismatch, wrong extraction mask used
- **Size error:** Significant (~100+ bytes on 10KB test)
- **Detection:** Compare with edge-cases test vector (exercises this path)
- **Affected tests:** edge-cases.bin shows ~109 byte size difference without this fix

### 🔍 Why This Was Hard to Find

1. **R=1 tests may pass** - simple.bin passed because of specific data patterns
2. **R=2 tests have different issues** - Vₜ calculation bug masked this in housekeeping/venus-express
3. **Late divergence** - Only triggers when pₜ is set multiple times within Vₜ window
4. **Order of operations** - Reference stores flag then computes cₜ; easy to compute first then store
5. **Size difference misleading** - Looks like encoding bug, not flag counting bug

---

---

# Decompression Gotchas

The following gotchas are specific to implementing the decompressor.

---

## 8. COUNT Decoding: '10' is Terminator, Not a Value

📘 **Spec-mandated** — §5.2.2 Counter Encoding (table 5-1) and §5.2.3 RLE (note 2: a vector with no ‘1’ bits encodes as the ‘10’ terminator).

### ✅ What the Spec Says

COUNT encoding uses prefixes: '0' for 1, '110xxxxx' for 2-33, '111...' for larger values.

### ❌ Common Mistake

Treating '10' as a normal encoded value:

```c
// WRONG: Doesn't handle terminator
if (bit0 == 1 && bit1 == 0) {
    // This is NOT a value encoding!
}
```

### 🔧 Correct Implementation

```c
if (bit0 == 0) {
    *value = 1;  // '0' → 1
} else if (bit1 == 0) {
    *value = 0;  // '10' → TERMINATOR (return 0)
} else if (bit2 == 0) {
    // '110' + 5 bits → value + 2
} else {
    // '111' + variable bits
}
```

**Key insight:** In RLE context, COUNT returning 0 signals end of run-length sequence.

### 📊 Impact

- **Symptom:** Infinite loop or reading past end of RLE data
- **Detection:** First packet decompression fails or hangs

---

## 9. kₜ Reading Order: Forward (Low to High Position)

📘 **Spec-mandated** — §5.2.4 Bit Extraction (Eq 11): gᵢ is the position of the i-th ‘1’ bit in 𝒃, counted from the MSB.

### ✅ What Happens During Compression

The encoder extracts kₜ bits in **forward order** (lowest position index to highest).

### ❌ Common Mistake

Reading kₜ bits in reverse order (matching BE extraction):

```c
// WRONG: Reverse order (like BE)
for (int i = count - 1; i >= 0; i--) {
    kt_bits[i] = read_bit();
}
```

### 🔧 Correct Implementation

```c
// CORRECT: Forward order (matching encoder)
size_t kt_idx = 0;
for (size_t i = 0; i < F; i++) {
    if (bitvector_get_bit(&Xt, i)) {
        kt_bits[kt_idx++] = read_bit();
    }
}
```

### 📊 Impact

- **Symptom:** Mask reconstructed with wrong values at each changed position
- **Detection:** All packets after first mask change will be wrong

---

## 10. kₜ Bit Interpretation: Inverted Mask Values

📘 **Spec-mandated** — §5.3.3.1, Eq 17/19: kₜ = yₜ = BE(<~Mₜ>, Xₜ) encodes the inverted mask values (`~Mₜ`).

### ✅ What the Encoder Outputs

The encoder outputs the **inverse** of mask values at changed positions:
- kt=1 means mask bit is 0 (positive update, now predictable)
- kt=0 means mask bit is 1 (negative update, now unpredictable)

### ❌ Common Mistake

Directly using kₜ bits as mask values:

```c
// WRONG: Direct assignment
mask[pos] = kt_bits[i];
```

### 🔧 Correct Implementation

```c
// CORRECT: Invert kt to get mask value
if (kt_bits[kt_idx] == 1) {
    bitvector_set_bit(&mask, pos, 0);  // kt=1 → mask=0 (predictable)
    bitvector_set_bit(&Xt_positive, pos, 1);  // Track for ct logic
} else {
    bitvector_set_bit(&mask, pos, 1);  // kt=0 → mask=1 (unpredictable)
}
```

### 📊 Impact

- **Symptom:** Mask completely inverted, wrong bits extracted
- **Detection:** Output is garbage after first mask update packet

---

## 11. Unpredictable Bits: Insert in Reverse Order (BE)

📘 **Spec-mandated** — §5.2.4 Bit Extraction (Eq 11) and §5.3.3.3, Eq 22 (uₜ).

### ✅ What the Encoder Does

BE extraction outputs bits from highest position to lowest (reverse order).

### ❌ Common Mistake

Inserting bits in forward order:

```c
// WRONG: Forward order
for (int i = 0; i < count; i++) {
    set_bit(data, positions[i], read_bit());
}
```

### 🔧 Correct Implementation

```c
// CORRECT: Reverse order (matching BE)
for (size_t i = count; i > 0; i--) {
    int bit = read_bit();
    bitvector_set_bit(data, positions[i - 1], bit);
}
```

**Remember:**
- **kₜ (mask values):** Read/write in forward order
- **uₜ (unpredictable bits via BE):** Read/write in reverse order

### 📊 Impact

- **Symptom:** Bits placed at wrong positions
- **Detection:** Output has correct number of 1s but in wrong positions

---

## 12. Horizontal XOR Mask Decoding

📘 **Spec-mandated** — §5.3.3.2, Eq 21 (qₜ = ‘1’ ∥ RLE of Mₜ XOR (Mₜ)≪ — the mask XORed with its 1-bit left shift — sent when ḟₜ=1), via §5.2.3 RLE (Eq 10).

### ✅ What the Encoder Sends

When ft=1, the encoder sends RLE(M XOR (M<<1)), a horizontal XOR of the mask.

### ❌ Common Mistake

Using the decoded RLE directly as the mask:

```c
// WRONG: RLE output is NOT the mask
rle_decode(reader, &mask);  // ❌ This is HXOR, not M
```

### 🔧 Correct Implementation

```c
// Decode horizontal XOR
bitvector_t hxor;
rle_decode(reader, &hxor, F);

// Reverse the horizontal XOR to get actual mask
// HXOR[i] = M[i] XOR M[i+1], with HXOR[F-1] = M[F-1]
// Reversal: M[F-1] = HXOR[F-1], M[i] = HXOR[i] XOR M[i+1]

int current = bitvector_get_bit(&hxor, F - 1);
bitvector_set_bit(&mask, F - 1, current);

for (size_t i = F - 1; i > 0; i--) {
    int hxor_bit = bitvector_get_bit(&hxor, i - 1);
    current = hxor_bit ^ current;
    bitvector_set_bit(&mask, i - 1, current);
}
```

### 📊 Impact

- **Symptom:** Completely wrong mask after ft=1 packet
- **Detection:** All subsequent packets decompress incorrectly

---

## 13. ḋₜ Flag Optimization

📘 **Spec-mandated** — §5.3.2.1, Eq 13 (ḋₜ).

### ✅ What the Spec Says (Equation 13)

ḋₜ = 1 implies **both** ḟₜ = 0 and ṙₜ = 0 (compressed packet with no mask transmission).

### ❌ Common Mistake

Always reading ft and rt after dt:

```c
// WRONG: Reads ft/rt even when dt=1
int dt = read_bit();
int ft = read_bit();  // ❌ Not in stream when dt=1!
int rt = read_bit();  // ❌ Not in stream when dt=1!
```

### 🔧 Correct Implementation

```c
int dt = read_bit();
int ft = 0, rt = 0;

if (dt == 0) {
    ft = read_bit();
    if (ft == 1) {
        // Decode full mask...
    }
    rt = read_bit();
}
// When dt=1, ft=0 and rt=0 implicitly
```

### 📊 Impact

- **Symptom:** Bit stream misalignment after first dt=1 packet
- **Detection:** Every packet after first optimization is corrupted

---

## 14. Vₜ=0 Special Case: Toggle Mask Bits

📘 **Spec-mandated** — §5.3.2.2, Eq 14 (the Vₜ = 0 case).

### ✅ What Happens

When Vₜ=0 and there are changes (Xₜ ≠ ∅), there's no eₜ or kₜ in the stream. The mask bits at changed positions are simply **toggled**.

### ❌ Common Mistake

Trying to read eₜ and kₜ when Vt=0:

```c
// WRONG: Tries to read et/kt regardless of Vt
if (change_count > 0) {
    int et = read_bit();  // ❌ Not present when Vt=0!
    // ...
}
```

### 🔧 Correct Implementation

```c
if (Vt > 0 && change_count > 0) {
    // Read et, kt, ct
    int et = read_bit();
    // ...
} else if (Vt == 0 && change_count > 0) {
    // Toggle mask bits at change positions
    for (size_t i = 0; i < F; i++) {
        if (bitvector_get_bit(&Xt, i)) {
            int current = bitvector_get_bit(&mask, i);
            bitvector_set_bit(&mask, i, current ? 0 : 1);
        }
    }
}
```

### 📊 Impact

- **Symptom:** Bit stream misalignment in low-robustness packets
- **Detection:** Fails on packets where Vt=0 with mask changes

---

## 15. Extraction Mask: cₜ Affects Which Bits to Read

📘 **Spec-mandated** — §5.3.3.1, Eq 20 (cₜ) and §5.3.3.3, Eq 22 (uₜ): cₜ selects whether changed bits are read against Mₜ or Xₜ ∣ Mₜ.

### ✅ What the Spec Says

The unpredictable bits component uses:
- **BE(Iₜ, Mₜ)** when cₜ=0 or Vₜ=0
- **BE(Iₜ, Xₜ OR Mₜ)** when cₜ=1 and Vₜ>0

### ❌ Common Mistake

Always using just the mask for extraction:

```c
// WRONG: Ignores ct flag
bit_insert(reader, output, &mask);
```

### 🔧 Correct Implementation

```c
bitvector_t extraction_mask;

if (ct == 1 && Vt > 0) {
    // BE(Iₜ, Xₜ OR Mₜ)
    bitvector_or(&extraction_mask, &mask, &Xt_positive);
} else {
    // BE(Iₜ, Mₜ)
    bitvector_copy(&extraction_mask, &mask);
}

bit_insert(reader, output, &extraction_mask);
```

**Note:** Xₜ_positive tracks only the **positive** changes (mask 1→0), not all changes.

### 📊 Impact

- **Symptom:** Wrong number of bits read, stream misalignment
- **Detection:** Fails on packets where ct=1 and there are positive mask changes

---

## Decompression Testing Checklist

Before declaring your decompressor "working":

- Round-trip test: compress → decompress → compare with original
- All test vectors decompress correctly
- First 3 packets (init phase) decompress correctly
- Packets with ft=1 (full mask) decompress correctly
- Packets with rt=1 (uncompressed) handled correctly
- Packets with dt=1 (optimized) don't read extra ft/rt bits
- Vt=0 packets toggle mask without reading et/kt
- kₜ read in forward order, applied as inverted values
- Unpredictable bits inserted in reverse order (BE)
- Horizontal XOR mask properly reversed
- ct=1 uses extended extraction mask (Xt OR Mt)

---

---

# High-Level API Gotchas

---

## 16. Packet Byte-Boundary Padding

🔧 **Conformance detail** — the output binary vector is a bitstream (§5.3.1, Eq 12); padding the final packet to a byte boundary matches the ESA reference's packetization.

**⭐ Discovery - December 2025**

### ✅ What the Reference Does

The C reference implementation converts **each compressed packet to bytes separately** before concatenating them into the output buffer:

```c
// Convert packet to bytes with byte-boundary padding
size_t packet_size = bitbuffer_to_bytes(&packet_output, packet_bytes, sizeof(packet_bytes));

// Append to output buffer
memcpy(&output_buffer[total_output_bytes], packet_bytes, packet_size);
total_output_bytes += packet_size;
```

### ❌ Common Mistake

Concatenating packet bits into a single buffer, then converting to bytes only at the end:

```java
// WRONG: Concatenate bits, convert once at end
for (int i = 0; i < numPackets; i++) {
    BitBuffer packetOutput = compressor.compressPacket(input);
    output.appendBitBuffer(packetOutput);  // ❌ Appends bits!
}
return output.toBytes();  // ❌ Single conversion at end
```

**Why this seems reasonable:** Bit-level concatenation is more efficient and simpler.

**Why it's wrong:** Each packet needs its own byte-boundary padding. Without per-packet padding, you lose ~0.3 bytes per packet on average.

### 🔧 Correct Implementation

```java
// CORRECT: Convert each packet to bytes, then concatenate
ByteArrayOutputStream output = new ByteArrayOutputStream();
for (int i = 0; i < numPackets; i++) {
    BitBuffer packetOutput = compressor.compressPacket(input);
    // Convert this packet to bytes with padding
    byte[] packetBytes = packetOutput.toBytes();
    output.write(packetBytes, 0, packetBytes.length);
}
return output.toByteArray();
```

### 📊 Impact

- **Divergence:** Immediate - first packet boundary
- **Symptom:** Output size smaller than reference by ~0.3 bytes/packet
- **Size error:** For 100 packets: ~29 bytes shorter
- **Example:** simple.bin with 100 packets: 612 bytes instead of 641 bytes
- **Detection:** Compare total compressed size with reference

### 🔍 Why This Was Hard to Find

1. **Not in CCSDS spec** - The spec doesn't mention packet concatenation at all
2. **Compression still works** - Output is valid, just different from reference
3. **Decompression might still work** - If decompressor aligns to byte boundaries
4. **Subtle size difference** - Easy to miss ~5% size difference

---

## 17. COUNT Extended Format ('111'): Include Terminating '1' in Value

📘 **Spec-mandated** — §5.2.2 Counter Encoding (table 5-1, the ‘111’ extended-format row).

**⭐ Discovery - December 2025**

### ✅ What the C Reference Does

For COUNT values >= 34 (the '111' extended format), the reference uses a do-while loop that **includes the terminating '1' in the size count**, then backs up to include it in the value:

```c
// '111' case
size_t size = 0;
int next_bit;

// Count zeros to determine field size
do {
    next_bit = bitreader_read_bit(reader);
    size++;  // Increments even for the terminating '1'!
} while ((next_bit == 0) && (remaining > 0));

// Size includes the '1' we just read
size_t value_bits = size + 5;

// Back up one bit since the '1' is part of the value
reader->bit_pos--;

// Read the full value (starting from the '1')
uint32_t raw = bitreader_read_bits(reader, value_bits);
*value = raw + 2;
```

### ❌ Common Mistake

Using peek/read pattern that doesn't include the terminating '1':

```java
// WRONG: Misses the terminating '1' in size calculation
int zeros = 0;
while (reader.peekBit() == 0) {
    reader.readBit();
    zeros++;
}
int numBits = zeros + 5;  // ❌ Off by 1!
int value = reader.readBits(numBits);
```

**Example with '11101XXXXX' (1 zero before '1'):**
- C approach: size=2 (includes '1'), value_bits=7, reads 7 bits starting from '1'
- Wrong approach: zeros=1, numBits=6, reads 6 bits AFTER '1'

### 🔧 Correct Implementation

Either back up like C, or account for the '1' differently:

```java
// CORRECT: Match C behavior
int size = 0;
int nextBit;
do {
    nextBit = reader.readBit();
    size++;
} while (nextBit == 0);

// The '1' is part of the value, account for it
int numRemainingBits = size + 5 - 1;  // We already consumed the leading '1'
int value = (1 << numRemainingBits) | reader.readBits(numRemainingBits);

return value + 2;
```

### 📊 Impact

- **Divergence:** First packet with full uncompressed data (COUNT(720) uses extended format)
- **Symptom:** Output appears bit-shifted by 1
- **Example:** `08 d4 f1 ab` becomes `04 6a 78 d5 80` (right-shifted by 1 bit)
- **Detection:** First decompressed packet data is shifted

### 🔍 Why This Was Hard to Find

1. **Only affects large counts** - Small counts (1-33) use different encoding
2. **Common in first packets** - COUNT(720) for full packet uses extended format
3. **Subtle shift** - Data looks almost right, just shifted
4. **C uses do-while** - Different from typical while-peek pattern

---

---

## 18. D₀ Must Be Zero at Initialization (Not M₀!)

📘 **Spec-mandated** — §4.2.3, Eq 8: the change vector is Dₜ = Mₜ XOR Mₜ₋₁, so D₀ = M₋₁ XOR M₀ = 0 when the caller sets M₋₁ = M₀ at t=0.

**⭐ Discovery - February 2026 (CCSDS Cross-Validation)**

### ✅ What the Spec Says

CCSDS Equation 8 defines the change vector as:
```
Dₜ = Mₜ XOR Mₜ₋₁
```

At t=0, there is no previous mask, so D₀ should represent "no change" — i.e., D₀ = 0.

### ❌ Common Mistake

Special-casing t=0 by copying the mask:

```c
// WRONG: D₀ = M₀ (treats the initial mask as a "change")
if (t == 0) {
    bitvector_copy(change, mask);  // ❌ D₀ = M₀
} else {
    bitvector_xor(change, mask, prev_mask);
}
```

**Why this seems reasonable:** At t=0 there is no M₋₁, so you might assume D₀ = M₀ XOR 0 = M₀, treating the "previous mask" as all zeros.

**Why it's wrong:** The standard treats initialization as a known state — both encoder and decoder start with the same M₀, so there is no change to communicate. D₀ = M₀ would incorrectly encode the initial mask as a change in the first packet's X₀ vector, producing a longer RLE(X₀) and wrong compressed output.

### 🔧 Correct Implementation

Set `prev_mask = mask` before the first call, then always use XOR:

```c
// In compressor init or before first packet:
bitvector_copy(&comp->prev_mask, &comp->mask);  // M₋₁ = M₀

// In ccsds124_compute_change (no special case for t=0):
bitvector_xor(change, mask, prev_mask);  // D₀ = M₀ XOR M₀ = 0
```

**What changes in the compressed output:**
- **D₀ = 0** → X₀ is zero → RLE(X₀) = `'10'` (just the terminator, 2 bits)
- **D₀ = M₀ (wrong)** → X₀ is non-zero when M₀ ≠ 0 → RLE(X₀) is longer

### 📊 Impact

- **Divergence:** First packet when M₀ is non-zero
- **Symptom:** First 2 bits of compressed output are `'11...'` instead of `'10'` (RLE encodes mask changes that don't exist)
- **Size error:** First packet is larger than expected (extra bits for phantom mask change)
- **Detection:** Check first 2 bits: should be `'10'` (RLE terminator for empty X₀)
- **Affected tests:** Any test with non-zero initial mask; all 24,900 CCSDS cross-validation vectors

### 🔍 Why This Was Hard to Find

1. **M₀ = 0 hides the bug** — When M₀ is all zeros (the typical case for reference test vectors), D₀ = M₀ = 0 = M₀ XOR M₀, so both approaches give the same result
2. **Only triggers with non-zero M₀** — The CCSDS cross-validation suite exercises non-zero initial masks, which exposed this
3. **Easy to miss the spec's t=0 case** — Equation 8 defines Dₜ by cases: Mₜ XOR Mₜ₋₁ for t > 0, and **0 otherwise**; implementations that only code the XOR branch need M₋₁ = M₀ to reproduce the t=0 case
4. **Small output difference** — For small M₀ hamming weights, the size difference is only a few bits

---

## 19. Bitvector NOT: MSB-Aligned Masking for Non-Byte-Aligned Lengths

📘 **Spec-mandated** — §1.6.1: bit N−1 is the MSB and is transmitted first; masking must be MSB-aligned.

**⭐ Discovery - February 2026 (CCSDS Cross-Validation)**

### ✅ What CCSDS 124.0-B-1 Requires

CCSDS 124.0-B-1 uses **MSB-first bit packing** (CCSDS convention). In a bitvector of length F, the valid bits occupy the **high** (most significant) bits of each byte. When F is not a multiple of 8, the last byte has padding bits in the **low** positions.

### ❌ Common Mistake

Masking the low bits as valid when computing NOT on non-byte-aligned vectors:

```c
// WRONG: Assumes valid bits are in the LOW positions
uint8_t byte_mask = (uint8_t)((1U << bits_in_last_byte) - 1U);
result_byte = (~input_byte) & byte_mask;
```

**Example for 4-bit vector `0000`:**
- Input byte: `0x00` (bits: `0000 0000`)
- Wrong mask: `0x0F` (low 4 bits)
- Wrong NOT result: `0x0F` = `0000 1111` — puts 1s in padding bits!
- Correct mask: `0xF0` (high 4 bits)
- Correct NOT result: `0xF0` = `1111 0000` — 1s in valid bits only

### 🔧 Correct Implementation

```c
// CORRECT: MSB-aligned — valid bits are at the TOP of the byte
uint8_t byte_mask = (uint8_t)(0xFFU << (8U - bits_in_last_byte));
result_byte = (~input_byte) & byte_mask;
```

**Why this matters for CCSDS 124.0-B-1:**
- The NOT operation is used to compute the inverted mask for kₜ extraction
- If padding bits become `1` instead of `0`, the hamming weight is wrong, changing eₜ and kₜ encoding
- Downstream: wrong RLE, wrong bit counts, complete output divergence

### 📊 Impact

- **Divergence:** First packet where NOT is applied to a non-byte-aligned vector
- **Symptom:** Extra `1` bits in padding positions corrupt hamming weight calculations
- **Size error:** Variable — depends on how padding bits propagate through encoding
- **Detection:** Check `bitvector_not` output for vectors with length not divisible by 8; padding bits must remain `0`
- **Affected tests:** Any F that is not a multiple of 8 (e.g., F=4, F=12, F=100)

### 🔍 Why This Was Hard to Find

1. **F=720 (90 bytes) is byte-aligned** — All reference test vectors use byte-aligned lengths, so this never triggered
2. **Only exposed by cross-validation** — The CCSDS cross-validation suite includes vectors with non-byte-aligned F values
3. **Subtle bit-level corruption** — The extra `1` bits in padding positions only affect operations that inspect bit values (hamming weight, extraction), not simple copies or XORs

---

## 20. Decoder Must Validate Bitstream Integrity to Reject Corrupt Packets

🔧 **Conformance detail** — the standard does not specify decoder error handling; rejecting corrupt bitstreams is required for UAB/CNES cross-validation compatibility.

**Category:** Decompression
**Discovery:** CCSDS cross-validation (decoder vectors include intentionally corrupt/fuzzed packets)

### The Problem

For UAB/CNES cross-validation compatibility, a CCSDS 124.0-B-1 decoder must not silently produce wrong output when given corrupt compressed data. The UAB decoder implements strict validation checks (documented in README_crossvalidation.md v1.4-v1.13) that reject invalid packets. Without these checks, the decoder happily decompresses garbage into wrong output, causing cross-validation failures where the UAB/CNES expected output records an error status (0x01).

### Three Categories of Validation

**Bitstream underflow detection:** Every `bitreader_read_bit` and `bitreader_read_bits` call can fail if the packet is truncated or the RLE/COUNT encoding is corrupt. The decoder must check return values and abort with an error if the bitstream is exhausted mid-packet.

Key locations:
- `bitreader_read_bits`: check `bitreader_remaining(reader) >= num_bits` upfront
- `ccsds124_count_decode`: check remaining bits before Case 3 (5-bit read) and Case 4 (variable-length read); check `bitreader_read_bit` return during zero-counting
- `ccsds124_bit_insert`: check `bitreader_remaining(reader) >= hamming` before insert loop
- `ccsds124_decompress_packet`: check return values for Vt, et, kt, ct, dt, ft, rt, and I_t reads

**RLE delta bounds checking:** In `ccsds124_rle_decode`, when a delta exceeds the remaining bit position, the mask position is invalid. Return an error instead of silently ignoring the delta.

```c
/* Before fix: silently ignored invalid deltas */
if (delta <= bit_position) {
    bit_position -= delta;
    bitvector_set_bit(result, bit_position, 1);
}

/* After fix: return error on invalid delta */
if (delta > bit_position) {
    return CCSDS124_ERROR_OVERFLOW;
}
bit_position -= delta;
bitvector_set_bit(result, bit_position, 1);
```

**Post-decompression padding verification (v1.10):** After successfully decompressing a single packet, at most 7 padding bits should remain in the bitreader. If 8 or more bits remain, the packet was not fully consumed — this indicates corrupt data. Note: this check must be applied per-packet (not in multi-packet stream decompression where remaining bits include subsequent packets).

### 📊 Impact

- **Symptom:** Decoder produces output that doesn't match expected SHA-256 for corrupt test vectors
- **Scope:** Affects only invalid/fuzzed packets — all valid packets continue to decompress correctly
- **Cross-validation improvement:** Decoder pass count increased from 10,496 to 12,315 (of 16,965 vectors)
- **Detection:** Cross-validation with UAB test suite; the remaining 4,650 failures likely require additional validation rules

---

## 21. Decoder Cross-Validation: Mask Synchronization and Accuracy Guarantees

🔧 **Conformance detail** — §2 defines the accuracy guarantee (a packet stays decodable after ≤ the effective robustness level of consecutive losses); the exact decoder status policy for corrupt bitstreams is underspecified in the standard and was reverse-engineered from the UAB/CNES cross-validation vectors.

**Category:** Decompression
**Discovery:** CCSDS cross-validation (decoder vectors with unknown initial mask and fuzzed packets)

### The Problem

When the decoder doesn't know the encoder's initial mask (`large_m_0`), it starts with an all-zero mask. This creates a mask desynchronization that persists until a full mask transmission (`ft=1`) is received. A naive decoder that ignores desynchronization will produce too many "guaranteed" outputs (status `0x00`) for packets that the UAB/CNES expected outputs mark as unguaranteed (`0x01`).

### Library Support

The C implementation internalizes all accuracy guarantee logic in `ccsds124_decompress_packet_checked()`, which handles mask synchronization tracking, status history, state save/restore, and the guarantee decision tree. This means implementations using this function get correct guarantee behavior automatically — the cross-validation decoder harness is reduced to thin I/O glue.

The function returns `CCSDS124_OK` for guaranteed packets, `CCSDS124_STATUS_UNGUARANTEED` for unguaranteed packets (with state restored), or a negative error code. An optional `ccsds124_decompress_result_t` struct provides Vt, ft, and rt values.

### Key Behaviors the Decoder Must Implement

These behaviors are handled automatically by `ccsds124_decompress_packet_checked()` in the C library. Other language implementations must replicate this logic:

**1. Mask synchronization tracking (`mask_synced`):**
The decoder must track whether its mask has been synchronized with the encoder's via a full mask transmission (`ft=1`). Start with `mask_synced=0`. Set to `1` only when a guaranteed (`0x00`) packet with `ft=1` is successfully decoded. Reset to `0` on decompression failure, packet loss, or mask inconsistency detection.

**2. Reference packet guarantee (rt=1):**
A reference packet (uncompressed, `rt=1`) is treated as guaranteed by the UAB/CNES expected outputs only if the mask is synchronized (`mask_synced=1`) or the packet itself provides a full mask (`ft=1`) to resynchronize. Without this check, desynchronized decoders incorrectly accept reference packets for cross-validation purposes.

**3. Mask inconsistency detection (ft=1):**
When `ft=1`, compare the delta-updated mask with the full mask received. If they differ (`mask_inconsistent`), the packet is corrupt or the decoder is desynchronized. When detected while synced: restore state, lose sync, output `0x01`. When detected while not synced: UAB/CNES expected behavior is to keep state (full mask is correct) but still output `0x01`.

**4. COUNT(F) validation:**
For `rt=1` packets, the self-delimiting `COUNT(F)` field encodes the packet length. If `COUNT(F)` doesn't match the expected `F`, the packet bitstream is corrupt (wrong bit offset for `I_t` data). Flag as diagnostic (`count_f_mismatch`) and let the harness decide whether to accept or reject.

**5. Robustness window for non-reference packets (rt=0):**
Non-reference packets are treated as guaranteed by the UAB/CNES expected outputs only if the preceding `Vt` received packets were all successful (`0x00`). Skip lost packets (`0x02`) in the window since the robustness guarantee applies to decoded packets only.

**6. State management on failure:**
On decompression failure or unguaranteed status: restore the decompressor state from a saved copy to prevent error propagation to subsequent packets.

### 📊 Impact

- **Cross-validation improvement:** Decoder pass count increased from 12,315 to 14,924, then to 15,102 with additional reverse-engineered rules (of 16,965 vectors)
- **Remaining failures (1,863):** Fuzzed packets with corrupted `COUNT(F)` fields and complex mask synchronization edge cases where the reference implementation handles specific corruption patterns differently — see CONFORMANCE.md "Known Gaps" for the full categorization

### Additional Reverse-Engineered Rules (June 2026)

Three more reference behaviors were identified from the test vectors and implemented in `ccsds124_discover_packet_length()` and `ccsds124_decompress_packet_checked()`:

1. **Truncated reference packets signal F:** `ccsds124_discover_packet_length()` returns `CCSDS124_STATUS_TRUNCATED_LENGTH` with the signaled length when the bitstream ends inside `I_t` — per the cross-validation rule that a decodable `COUNT(F)` "is to be considered" even from a packet that cannot be fully decoded. Harnesses report it in the output trailer when no fully-valid reference packet exists.
2. **Signaled-length validity (v1.6):** a signaled `COUNT(F)` is only trusted when it is in range (1–65535) and the packet's own RLE spans fit within it (X_t span ≤ F, full-mask span ≤ F). Corrupt `COUNT(F)` values betray themselves by encoding bit positions beyond the signaled length.
3. **Excess-bits tolerance for reference packets:** `rt=1` packets are self-delimiting via `COUNT(F)`, so excess trailing bits are ignored rather than rejected; `rt=0` packets keep the strict at-most-7-padding-bits rule (v1.10).

---

**If you discover new gotchas, please document them here!**
