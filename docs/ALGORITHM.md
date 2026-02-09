# POCKET+ Algorithm Specification

## Overview

POCKET+ (CCSDS 124.0-B-1) is a lossless compression algorithm designed for fixed-length housekeeping telemetry data. It exploits the property that many bits in consecutive packets remain unchanged, achieving compression by only transmitting the bits that change.

**Key Features:**
- **Lossless compression** of fixed-length binary vectors
- **Low complexity** - uses only bitwise operations (XOR, OR, AND, shifts)
- **Robust to packet loss** - configurable resilience to N consecutive lost packets
- **No latency** - produces one output packet per input packet
- **Adaptive** - tracks which bits change over time using a mask

## ⚠️ Critical Implementation Notes

**READ THIS FIRST!** These are the most common implementation pitfalls that will cause your output to diverge from the reference implementation:

### 1. Initialization Phase: First Rₜ+1 Packets (Not Rₜ+2!)

The CCSDS spec states that the **first Rₜ+1 packets** must be sent uncompressed with ḟₜ=1, ṙₜ=1, ṗₜ=0.

**Example for Rₜ=1:**
- Packet 0 (first packet): init phase → ḟₜ=1, ṙₜ=1, ṗₜ=0
- Packet 1 (second packet): init phase → ḟₜ=1, ṙₜ=1, ṗₜ=0
- Packet 2 (third packet): **normal operation begins**

**Common mistake:** Applying init phase to Rₜ+2 packets instead of Rₜ+1 packets.

**Correct condition:** `packet_index ≤ Rₜ` triggers init phase
**Wrong condition:** `packet_index < Rₜ + 2` (applies to 3 packets when Rₜ=1)

### 2. Flag Timing: Countdown Counters, Not Modulo Arithmetic

The reference implementation uses **countdown counters** that start at the period limit and decrement each packet. Flags trigger when the counter reaches 1, then reset.

**Pattern for Rₜ=1, periods (10, 20, 50):**
- ṗₜ=1 (new mask) at packets: **11, 21, 31, 41...** (not 10, 20, 30!)
- ḟₜ=1 (send mask) at packets: **21, 41, 61, 81...** (not 20, 40, 60!)
- ṙₜ=1 (uncompressed) at packets: **51, 101, 151, 201...** (not 50, 100, 150!)

**Key insight:** First trigger happens at `period + Rₜ`, not at `period`.

**Example for pt_period=10, Rₜ=1:**
- pt_first_trigger = 10 + 1 = **packet 11**
- Subsequent triggers: packets 21, 31, 41... (every 10 packets)

**Common mistake:** Using `(packet_num % period) == 0` which triggers one packet too early (at packets 10, 20, 30 instead of 11, 21, 31).

### 3. Vₜ Calculation: Start from Rₜ+1

Per CCSDS Section 5.3.2.2, Cₜ counts consecutive packets with no mask changes, starting from position Rₜ+1 in the history buffer.

**Correct algorithm:**
1. For t ≤ Rₜ: Vₜ = Rₜ (initialization phase)
2. For t > Rₜ: Count backward starting from position Rₜ+1
   - Check D_{t-(Rₜ+1)}, D_{t-(Rₜ+2)}, ...
   - Stop when finding a change or reaching maximum Cₜ = 15 - Rₜ
   - Vₜ = Rₜ + Cₜ

**Example for Rₜ=2, t=5:**
- Start checking from i=3 (Rₜ+1=3)
- Check D_{t-3}, D_{t-4}, D_{t-5}, ...
- If all empty, Cₜ increases accordingly

**Common mistake:** Starting from i=2 regardless of Rₜ. This works for R=1 but fails for R=2.

### 4. Packet Indexing: 0-Based vs 1-Based in Flag Calculations

The CCSDS reference implementation uses **1-based packet numbering** (packets 1, 2, 3...). If your implementation uses **0-based loop indexing** (i=0, 1, 2...), you MUST convert to 1-based when calculating flag triggers.

**The Reference Uses 1-Based Packet Numbers:**
- Packet 1 = first packet (our i=0)
- Packet 11 = eleventh packet (our i=10) ← first ṗₜ trigger for Rₜ=1
- Packet 21 = twenty-first packet (our i=20) ← first ḟₜ trigger for Rₜ=1

**Example for Rₜ=1, pt_period=10:**

| Loop Index (i) | Packet Number | Expected ṗₜ | Wrong (using i) | Impact |
|----------------|---------------|-------------|-----------------|---------|
| i=10 | Packet 11 | **1** (trigger!) | 0 | Flag missed! |
| i=20 | Packet 21 | **1** (trigger!) | 0 | Flag missed! |
| i=30 | Packet 31 | **1** (trigger!) | 0 | Flag missed! |

**The Fix:** Use `packet_num = i + 1` when calculating flag triggers.
- For i=10: packet_num=11, check if `11 % 10 == 1` ✓ Triggers correctly!
- For i=20: packet_num=21, check if `21 % 10 == 1` ✓ Triggers correctly!

**Common mistake:** Using loop index `i` directly for modulo arithmetic when flag triggers are defined in 1-based terms.

**Why this matters:**
- Wrong flags corrupt the dₜ calculation (flag_no_mask_updates_in_Xt)
- Wrong ṗₜ causes mask/build updates at incorrect times
- Wrong ḟₜ omits required mask transmission
- Wrong ṙₜ sends compressed data when uncompressed is expected
- **Result:** Complete divergence within 20-30 packets

### 5. Component kₜ: Inverted Mask Values (Not Direct Mask Values!)

The kₜ component encodes mask values at changed positions, but **outputs the INVERSE** of the mask bits.

**CCSDS spec says:** "kₜ: mask values for changed positions"
**What this actually means:** Output '1' for positive updates (mask changed to 0), '0' for negative updates (mask changed to 1)

**Correct encoding:**
- When Xₜ has '1' at position i (bit changed):
  - If mask[i] = 0 (now predictable): output **1** in kₜ
  - If mask[i] = 1 (now unpredictable): output **0** in kₜ
- This is the **INVERSE** of the mask values

**Example:**
- Xₜ = '1' at positions [43, 142]
- Mask values at those positions: [0, 0] (both predictable)
- kₜ output (extracted in reverse order): **11** (not 00!)

**Why this matters:**
- The eₜ flag indicates if there are "positive updates" (mask bits changed from 1→0, becoming predictable)
- kₜ outputs '1' to mark these positive updates
- Extracting mask values directly gives the opposite encoding
- **Result:** 2-bit error per changed position, causing divergence at byte ~30% into output

**Implementation:**
```
inverted_mask[i] = !mask[i]  // Invert the entire mask
kt = BE(inverted_mask, Xt)   // Extract inverted values at changed positions
```

### Summary

See [GOTCHAS.md](GOTCHAS.md) for the complete list of implementation pitfalls.

## High-Level Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                    INPUT PACKET (Fixed Length F)                │
│                         Binary Vector Iₜ                        │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
                   ┌──────────────────┐
                   │   MASK UPDATE    │
                   │  (Section 4)     │
                   │                  │
                   │  • Update Build  │
                   │  • Update Mask   │
                   │  • Compute Change│
                   └────────┬─────────┘
                            │
                            ▼
                   ┌──────────────────┐
                   │    ENCODING      │
                   │   (Section 5)    │
                   │                  │
                   │  hₜ = RLE(changes)│
                   │  qₜ = mask (opt) │
                   │  uₜ = unpred bits│
                   └────────┬─────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────────────┐
│              OUTPUT PACKET (Variable Length)                   │
│                    oₜ = hₜ ∥ qₜ ∥ uₜ                          │
└────────────────────────────────────────────────────────────────┘
```

## Core Concepts

### 1. The Mask

The **mask** (Mₜ) is a binary vector of length F where each bit indicates:
- **`0`** = **Predictable** - bit expected to stay the same as previous packet
- **`1`** = **Unpredictable** - bit may change from previous packet

```
Input Packet Iₜ:     [ 1 0 1 1 0 0 1 1 ]
Previous Packet:     [ 1 0 1 0 0 1 1 1 ]
Changed bits:        [ 0 0 0 1 0 1 0 0 ]  (XOR result)
                               ↑   ↑
Mask (unpredictable):[ 0 0 0 1 1 1 0 0 ]
                               └───┘
                            These positions
                         track as unpredictable
```

### 2. The Build Vector

The **build** (Bₜ) is a parallel mask constructed using the same rules as the mask. It can replace the mask when the `new_mask_flag` is set, allowing unpredictable bits to become predictable again.

### 3. The Change Vector

The **change vector** (Dₜ) tracks which mask bits changed between time t-1 and t:
```
Dₜ = Mₜ XOR Mₜ₋₁
```

This is used in the encoding stage to communicate mask updates to the decompressor.

## Algorithm Stages

### Stage 1: Mask Update (CCSDS 124.0-B-1 Section 4)

For each input packet Iₜ, update the mask and build vectors:

#### Build Vector Update
```
         ⎧ (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁,  if t > 0 and ṗₜ = 0
Bₜ =     ⎨
         ⎩ 0,                       otherwise (reset when new_mask_flag set)
```

#### Mask Vector Update
```
         ⎧ (Iₜ XOR Iₜ₋₁) OR Mₜ₋₁,  if ṗₜ = 0
Mₜ =     ⎨
         ⎩ (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁,  if ṗₜ = 1 (replace mask with build)
```

Initial condition: M₀ is user-specified (typically all zeros)

#### Change Vector Update
```
Dₜ = Mₜ XOR Mₜ₋₁
```

Initial condition: Set M₋₁ = M₀ so that D₀ = M₀ XOR M₀ = 0 (no change at initialization).

⚠️ **See [GOTCHAS.md #19](GOTCHAS.md#19-d₀-must-be-zero-at-initialization-not-m₀)** — A common mistake is special-casing t=0 with D₀ = M₀, which is wrong. The correct approach is to always use XOR, with the caller setting M₋₁ = M₀ before the first packet.

**Flow Diagram:**
```
Iₜ₋₁ ────┐
         XOR ──→ changes ──→ OR ──→ Bₜ (if ṗₜ=0)
Iₜ ──────┘                    ↑      └──→ Mₜ (if ṗₜ=1)
                              │
                         Bₜ₋₁ or Mₜ₋₁

Mₜ₋₁ ────┐
         XOR ──→ Dₜ (change vector)
Mₜ ──────┘
```

### Stage 2: Encoding (CCSDS 124.0-B-1 Section 5)

The output packet is a concatenation of three variable-length binary vectors:

```
oₜ = hₜ ∥ qₜ ∥ uₜ
```

#### Component hₜ: Mask Change Information

Encodes recent mask changes with robustness against packet loss:

```
hₜ = RLE(Xₜ) ∥ BIT₄(Vₜ) ∥ eₜ ∥ kₜ ∥ cₜ ∥ ḋₜ
```

Where:
- **Xₜ**: Reversed OR of change vectors over robustness window
  ```
           ⎧ <Dₜ>,                                    if Rₜ = 0
  Xₜ =     ⎨ <(D₁ OR D₂ OR ... OR Dₜ)>,             if (t - Rₜ) ≤ 0
           ⎩ <(Dₜ₋ᴿₜ OR Dₜ₋ᴿₜ₊₁ OR ... OR Dₜ)>,    otherwise
  ```
  (Note: `<a>` means reverse the bit order of vector a)

- **Vₜ**: Effective robustness level (4 bits, value 0-15)
- **eₜ**: Flag indicating if any mask changes resulted in predictable (0) vs unpredictable (1)
- **kₜ**: Mask values for changed positions
- **cₜ**: Flag indicating multiple new_mask_flag sets in robustness window
- **ḋₜ**: Flag indicating if both ḟₜ and ṙₜ are zero

#### Component qₜ: Optional Full Mask

Optionally encodes the entire mask when `send_mask_flag` (ḟₜ) is set:

```
         ⎧ ∅,                                        if ḋₜ = 1
qₜ =     ⎨ '1' ∥ RLE(<(Mₜ XOR (Mₜ≪))>),            if ḟₜ = 1
         ⎩ '0',                                      otherwise
```

Where `Mₜ≪` is the left bit-shift of Mₜ (shifts bits left, inserts 0 at LSB)

#### Component uₜ: Unpredictable Bits or Full Input

Encodes the data payload:

```
         ⎧ BE(Iₜ, (<Xₜ> OR Mₜ)),                   if ḋₜ = 1 and cₜ = 1
         ⎪ BE(Iₜ, Mₜ),                              if ḋₜ = 1 and cₜ ≠ 1
uₜ =     ⎨ '1' ∥ COUNT(F) ∥ Iₜ,                     if ṙₜ = 1
         ⎪ '0' ∥ BE(Iₜ, (<Xₜ> OR Mₜ)),             if ṙₜ = 0 and ḟₜ = 1 and cₜ = 1
         ⎩ '0' ∥ BE(Iₜ, Mₜ),                        otherwise
```

Where:
- **BE(a, b)**: Bit extraction - extracts bits from `a` at positions where `b` has '1' bits
- **COUNT(F)**: Counter encoding of the input vector length

## Encoding Functions

### 1. Counter Encoding: COUNT(A)

Encodes positive integers 1 ≤ A ≤ 2¹⁶-1:

| Input Range | Output Format | Example |
|-------------|---------------|---------|
| A = 1 | `'0'` | COUNT(1) = `'0'` |
| 2 ≤ A ≤ 33 | `'110'` ∥ BIT₅(A-2) | COUNT(2) = `'110'` ∥ `'00000'` |
| A ≥ 34 | `'111'` ∥ BIT_E(A-2) | COUNT(34) = `'111'` ∥ `'000000'` |

For A ≥ 34: E = 2⌊log₂(A-2) + 1⌋ - 6

**Decoding:** The number of leading zeros uniquely identifies the encoding length, enabling self-parsing.

### 2. Run-Length Encoding: RLE(a)

Encodes a binary vector by counting runs of zeros between '1' bits:

```
RLE(a) = COUNT(C₀) ∥ COUNT(C₁) ∥ ... ∥ COUNT(C_{H(a)-1}) ∥ '10'
```

Where:
- **Cᵢ**: One more than the number of consecutive '0' bits before the i-th '1' bit
- **H(a)**: Hamming weight (number of '1' bits in a)
- **'10'**: Terminator

**Example:**
```
a = '0001000001000010000010000100000010000000000000000'
     ↑         ↑     ↑     ↑    ↑   ↑              ↑
     C₀=4      C₁=6  C₂=1  C₃=6 C₄=5 C₅=7         C₆=3

RLE(a) = COUNT(4) ∥ COUNT(6) ∥ COUNT(1) ∥ COUNT(6) ∥
         COUNT(5) ∥ COUNT(7) ∥ COUNT(3) ∥ '10'
```

**Note:** Trailing zeros are not encoded (deducible from vector length and H(a))

### 3. Bit Extraction: BE(a, b)

Extracts bits from vector `a` at positions where vector `b` has '1' bits:

```
BE(a, b) = a_{g_{H(b)-1}} ∥ ... ∥ a_{g₁} ∥ a_{g₀}
```

Where gᵢ is the position of the i-th '1' bit in `b` (MSB to LSB order)

**Example:**
```
a = '10110011'
b = '01001010'
     ↑  ↑ ↑
BE(a, b) = '001' (extracts bits at positions 6, 3, 1 from a)
```

## Parameters

### Input Parameters

| Parameter | Symbol | Type | Range | Description |
|-----------|--------|------|-------|-------------|
| Input Vector Length | F | integer | 1 to 65535 | Fixed length of input binary vectors |
| Initial Mask | M₀ | binary vector | length F | Initial mask state (typically all zeros) |
| Min Required Robustness | Rₜ | integer | 0 to 7 | Minimum guaranteed consecutive packet loss tolerance |
| New Mask Flag | ṗₜ | bit | 0 or 1 | When 1, replaces mask with build (enables predictable←unpredictable) |
| Send Mask Flag | ḟₜ | bit | 0 or 1 | When 1, includes full mask in output |
| Uncompressed Flag | ṙₜ | bit | 0 or 1 | When 1, sends full input packet instead of just unpredictable bits |

**Special Constraints:**
- For the **first Rₜ+1 packets** (t ≤ Rₜ): ḟₜ = 1, ṙₜ = 1, ṗₜ = 0 (mandatory full mask and full input during initialization)
  - ⚠️ **See [Critical Implementation Note #1](#1-initialization-phase-first-rₜ1-packets-not-rₜ2)** - Common off-by-one error!
- The timing of ṗₜ, ḟₜ, ṙₜ flags depends on period counters that start during initialization
  - ⚠️ **See [Critical Implementation Note #2](#2-flag-timing-countdown-counters-not-modulo-arithmetic)** - Flags don't trigger when you expect!
- All parameters needed for decompression are encoded in the output bitstream

## Bit Numbering Convention

⚠️ **CRITICAL**: CCSDS 124.0-B-1 uses **MSB-first** indexing, which is **opposite** to most programming conventions!

**Traditional Bit Numbering (WRONG for CCSDS):**
```
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │  ← Bit indices (typical systems)
└───┴───┴───┴───┴───┴───┴───┴───┘
  ↑                               ↑
  MSB                            LSB
  (highest bit)                  (lowest bit)
```

**CCSDS Bit Numbering (CORRECT):**
```
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │  ← Bit indices (CCSDS convention)
└───┴───┴───┴───┴───┴───┴───┴───┘
  ↑                               ↑
  MSB (bit 0)                    LSB (bit N-1)
  Transmitted first              Transmitted last
```

**For an N-bit vector:**
- **Bit 0** = MSB (Most Significant Bit) = 2^(N-1)
- **Bit N-1** = LSB (Least Significant Bit) = 2^0

**Why this matters:**
- **Bitvector indexing**: `vector[0]` accesses the MSB, not the LSB
- **RLE encoding**: Bit positions are encoded MSB-first
- **Bit extraction**: Extract from position 0 (MSB) to position N-1 (LSB)
- **Interoperability**: Reference implementations assume this convention

**Example for 8-bit value 0xB3 (10110011 in binary):**
```
Traditional:  bit[7]=1, bit[6]=0, bit[5]=1, bit[4]=1, ...
CCSDS:        bit[0]=1, bit[1]=0, bit[2]=1, bit[3]=1, ...
              └─MSB                                  └─LSB
```

**Common mistake:** Implementing bit operations with LSB-first indexing will cause your output to be bit-reversed and completely incompatible with CCSDS-compliant decompressors!

## Bitwise Operations

| Operation | Symbol | Description | Example |
|-----------|--------|-------------|---------|
| XOR | `a XOR b` | Exclusive OR (1 if bits differ) | `'101' XOR '110' = '011'` |
| OR | `a OR b` | Logical OR (1 if either bit is 1) | `'101' OR '110' = '111'` |
| AND | `a AND b` | Logical AND (1 if both bits are 1) | `'101' AND '110' = '100'` |
| NOT | `~a` | Bitwise inverse | `~'101' = '010'` |
| Left Shift | `a≪` | Shift left, insert 0 at LSB | `'101'≪ = '010'` |
| Reverse | `<a>` | Reverse bit order | `<'101'> = '101'` |
| Concatenate | `a ∥ b` | Concatenate vectors | `'101' ∥ '11' = '10111'` |

## Robustness and Packet Loss

The algorithm guarantees decompression even if **up to Vₜ consecutive packets** are lost before the current packet, where:

```
Vₜ = Rₜ + Cₜ
```

- **Rₜ**: User-specified minimum required robustness level (0-7)
- **Cₜ**: Additional robustness from consecutive unchanged masks

**Important:** ⚠️ **See [Critical Implementation Note #3](#3-vₜ-calculation-skip-d_t-1)** for common pitfalls!

Per CCSDS 124.0-B-1 Section 5.3.2.2, Cₜ is defined as:

> The largest value for which **D_{t-i} = ∅** for all **1 < i ≤ Cₜ + Rₜ**

This means:
- For **t ≤ Rₜ**: Vₜ = Rₜ (initialization phase)
- For **t > Rₜ**: Count backward starting from **D_{t-2}** (skip D_{t-1}!)
- Stop when finding a change or reaching maximum Cₜ = 15 - Rₜ

Example for t=2, Rₜ=1:
- Check D₀ (skip D₁ per spec) → if D₀ = ∅, then Cₜ=1
- Result: Vₜ = Rₜ + Cₜ = 1 + 1 = 2

The mask change information in `hₜ` includes ORed changes from the previous Vₜ cycles, allowing the decompressor to resynchronize its mask even after packet loss.

## Implementation Complexity

**Operations per packet:**
- Fixed-length input → variable-length output
- Primarily bitwise operations: XOR, OR, AND, bit shifts
- Run-length encoding of sparse binary vectors
- No floating-point arithmetic
- No division (except for log₂ in COUNT, implementable with bit operations)
- Suitable for hardware and low-power embedded systems

## Example: Simple Compression Scenario

```
Given:
  F = 8 (packet length)
  M₀ = '00000000' (all bits start as predictable)
  Rₜ = 0 (no robustness requirement for this example)

Time t=0:
  I₀ = '10110011'
  Since t=0, must send full packet (ṙ₀ = 1, ḟ₀ = 1)

Time t=1:
  I₁ = '10110001' (last two bits changed)
         ↑↑

  Mask Update:
    D₁ = I₁ XOR I₀ = '00000010'
    M₁ = D₁ OR M₀ = '00000010' (bit 1 is now unpredictable)

  Encoding:
    hₜ = RLE(<'00000010'>) ∥ ... (mask change at position 1)
    qₜ = '0' (don't send full mask)
    uₜ = '0' ∥ BE(I₁, M₁) = '0' ∥ '0' (extract bit 1 from I₁)

  Output is much smaller than input!

Time t=2:
  I₂ = '10110001' (same as I₁)

  Mask Update:
    D₂ = I₂ XOR I₁ = '00000000' (no changes)
    M₂ = D₂ OR M₁ = '00000010' (mask unchanged)

  Encoding:
    hₜ = RLE(<'00000000'>) ∥ ... (no mask changes → very short)
    qₜ = '0'
    uₜ = '0' ∥ BE(I₂, M₂) = '0' ∥ '0'

  Highly compressed - only unpredictable bit sent!
```

## References

- **CCSDS 124.0-B-1**: Robust Compression of Fixed-Length Housekeeping Data, February 2023
- **Standard PDF**: https://ccsds.org/Pubs/124x0b1.pdf
- **ESA Reference Implementation**: https://opssat.esa.int/pocket-plus/
- **Implementation Paper**: Evans, D., et al. (2022). "Implementing the New CCSDS Housekeeping Data Compression Standard 124.0-B-1 (based on POCKET+) on OPS-SAT-1."

## Glossary

- **Binary Vector**: Sequence of bits (0s and 1s)
- **Fixed-Length**: All input packets have the same length F
- **Hamming Weight**: Number of '1' bits in a binary vector
- **Lossless**: Perfect reconstruction - decompressed data exactly matches original
- **LSB**: Least Significant Bit (rightmost, bit 0)
- **MSB**: Most Significant Bit (leftmost, bit N-1 for N-bit vector)
- **Robustness Level**: Number of consecutive packet losses tolerated without losing sync
- **Run-Length Encoding**: Encoding consecutive repeated values as count + value
