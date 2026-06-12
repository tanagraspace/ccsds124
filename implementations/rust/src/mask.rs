//! CCSDS 124.0-B-1 mask update logic.
//!
//! Implements CCSDS 124.0-B-1 Section 4 (Mask Update):
//! - Build Vector Update (Equation 6)
//! - Mask Vector Update (Equation 7)
//! - Change Vector Computation (Equation 8)

use crate::bitvector::BitVector;

/// Update the build vector.
///
/// CCSDS Equation 6:
/// - Bₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁ (if t > 0 and ṗₜ = 0)
/// - Bₜ = 0 (otherwise: t=0 or ṗₜ=1)
///
/// The build vector accumulates bits that have changed over time.
/// When `new_mask_flag` is set, the build is used to replace the mask
/// and is then reset to zero.
///
/// # Arguments
/// * `build` - Build vector to update (modified in place)
/// * `input` - Current input Iₜ
/// * `prev_input` - Previous input Iₜ₋₁
/// * `new_mask_flag` - True if new mask is being transmitted
/// * `t` - Current time step
pub fn update_build(
    build: &mut BitVector,
    input: &BitVector,
    prev_input: &BitVector,
    new_mask_flag: bool,
    t: usize,
) {
    // Case 1: t=0 or new_mask_flag set → reset build to 0
    if t == 0 || new_mask_flag {
        build.zero();
    } else {
        // Case 2: Normal operation (t > 0 and new_mask_flag = 0)
        // Bₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁

        // Calculate changes: Iₜ XOR Iₜ₋₁
        let changes = input.xor(prev_input);

        // Update build: Bₜ = changes OR Bₜ₋₁
        let new_build = changes.or(build);
        build.copy_from(&new_build);
    }
}

/// Update the mask vector.
///
/// CCSDS Equation 7:
/// - Mₜ = (Iₜ XOR Iₜ₋₁) OR Mₜ₋₁ (if ṗₜ = 0)
/// - Mₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁ (if ṗₜ = 1)
///
/// The mask tracks which bits are unpredictable.
/// When `new_mask_flag` is set, the mask is replaced with the build vector.
///
/// # Arguments
/// * `mask` - Mask vector to update (modified in place)
/// * `input` - Current input Iₜ
/// * `prev_input` - Previous input Iₜ₋₁
/// * `build_prev` - Previous build vector Bₜ₋₁
/// * `new_mask_flag` - True if new mask is being transmitted
pub fn update_mask(
    mask: &mut BitVector,
    input: &BitVector,
    prev_input: &BitVector,
    build_prev: &BitVector,
    new_mask_flag: bool,
) {
    // Calculate changes: Iₜ XOR Iₜ₋₁
    let changes = input.xor(prev_input);

    if new_mask_flag {
        // Case 1: new_mask_flag set → Mₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁
        let new_mask = changes.or(build_prev);
        mask.copy_from(&new_mask);
    } else {
        // Case 2: Normal operation → Mₜ = (Iₜ XOR Iₜ₋₁) OR Mₜ₋₁
        let new_mask = changes.or(mask);
        mask.copy_from(&new_mask);
    }
}

/// Compute the change vector.
///
/// CCSDS Equation 8:
/// - Dₜ = Mₜ XOR Mₜ₋₁ (if t > 0)
/// - Dₜ = 0 (if t = 0: no change at initialization)
///
/// The change vector tracks which mask bits changed between time steps.
/// This is used in encoding to communicate mask updates.
///
/// # Arguments
/// * `mask` - Current mask vector Mₜ
/// * `prev_mask` - Previous mask vector Mₜ₋₁
/// * `t` - Current time step
///
/// # Returns
/// The change vector Dₜ
pub fn compute_change(mask: &BitVector, prev_mask: &BitVector, t: usize) -> BitVector {
    if t == 0 {
        // CCSDS Eq. 8: D₀ = 0 — both encoder and decoder start from the
        // same user-specified M₀, so there is no change to communicate
        let mut zero = mask.clone();
        zero.zero();
        zero
    } else {
        // Dₜ = Mₜ XOR Mₜ₋₁
        mask.xor(prev_mask)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_update_build_at_t0() {
        let mut build = BitVector::from_bytes(&[0xFF], 8);
        let input = BitVector::new(8);
        let prev_input = BitVector::new(8);

        update_build(&mut build, &input, &prev_input, false, 0);

        // At t=0, build should be zeroed
        assert_eq!(build.hamming_weight(), 0);
    }

    #[test]
    fn test_update_build_with_new_mask_flag() {
        let mut build = BitVector::from_bytes(&[0xFF], 8);
        let input = BitVector::new(8);
        let prev_input = BitVector::new(8);

        update_build(&mut build, &input, &prev_input, true, 5);

        // With new_mask_flag, build should be zeroed
        assert_eq!(build.hamming_weight(), 0);
    }

    #[test]
    fn test_update_build_accumulates_changes() {
        let mut build = BitVector::from_bytes(&[0b0000_1111], 8);
        let input = BitVector::from_bytes(&[0b1010_0000], 8);
        let prev_input = BitVector::from_bytes(&[0b0000_0000], 8);

        update_build(&mut build, &input, &prev_input, false, 1);

        // Build = (input XOR prev_input) OR build
        // = 0b1010_0000 OR 0b0000_1111 = 0b1010_1111
        let expected = BitVector::from_bytes(&[0b1010_1111], 8);
        assert_eq!(build, expected);
    }

    #[test]
    fn test_update_mask_normal() {
        let mut mask = BitVector::from_bytes(&[0b0000_1111], 8);
        let input = BitVector::from_bytes(&[0b1100_0000], 8);
        let prev_input = BitVector::from_bytes(&[0b0000_0000], 8);
        let build_prev = BitVector::new(8);

        update_mask(&mut mask, &input, &prev_input, &build_prev, false);

        // Mask = (input XOR prev_input) OR mask
        // = 0b1100_0000 OR 0b0000_1111 = 0b1100_1111
        let expected = BitVector::from_bytes(&[0b1100_1111], 8);
        assert_eq!(mask, expected);
    }

    #[test]
    fn test_update_mask_with_new_mask_flag() {
        let mut mask = BitVector::from_bytes(&[0b0000_1111], 8);
        let input = BitVector::from_bytes(&[0b1100_0000], 8);
        let prev_input = BitVector::from_bytes(&[0b0000_0000], 8);
        let build_prev = BitVector::from_bytes(&[0b0011_0000], 8);

        update_mask(&mut mask, &input, &prev_input, &build_prev, true);

        // Mask = (input XOR prev_input) OR build_prev
        // = 0b1100_0000 OR 0b0011_0000 = 0b1111_0000
        let expected = BitVector::from_bytes(&[0b1111_0000], 8);
        assert_eq!(mask, expected);
    }

    #[test]
    fn test_compute_change_at_t0() {
        let mask = BitVector::from_bytes(&[0b1010_1010], 8);
        let prev_mask = BitVector::new(8);

        let change = compute_change(&mask, &prev_mask, 0);

        // CCSDS Eq. 8: D0 = 0 regardless of M0 (no change at initialization)
        assert_eq!(change, BitVector::new(8));
    }

    #[test]
    fn test_compute_change_normal() {
        let mask = BitVector::from_bytes(&[0b1111_0000], 8);
        let prev_mask = BitVector::from_bytes(&[0b1010_1010], 8);

        let change = compute_change(&mask, &prev_mask, 1);

        // Change = mask XOR prev_mask
        // = 0b1111_0000 XOR 0b1010_1010 = 0b0101_1010
        let expected = BitVector::from_bytes(&[0b0101_1010], 8);
        assert_eq!(change, expected);
    }
}
