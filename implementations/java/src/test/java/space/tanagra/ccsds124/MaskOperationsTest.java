package space.tanagra.ccsds124;

import static org.junit.jupiter.api.Assertions.assertEquals;

import org.junit.jupiter.api.Test;

/** Tests for {@link MaskOperations} (CCSDS 124.0-B-1 Section 4). */
class MaskOperationsTest {

  /**
   * Regression test for GOTCHAS #19 (issue #98): CCSDS Eq. 8 defines D0 = 0 regardless of the
   * initial mask M0 — there is no change to communicate at initialization. The pre-fix behavior (D0
   * = M0) wrongly encoded a non-zero initial mask as a change.
   */
  @Test
  void computeChangeAtT0IsZero() {
    BitVector mask = BitVector.fromBytes(new byte[] {(byte) 0xF0}, 8);
    BitVector prevMask = BitVector.fromBytes(new byte[] {(byte) 0xFF}, 8);

    BitVector change = MaskOperations.computeChange(mask, prevMask, 0);
    assertEquals(0, change.hammingWeight(), "D0 must be 0 per CCSDS Eq. 8");
  }

  @Test
  void computeChangeIntoAtT0IsZero() {
    BitVector mask = BitVector.fromBytes(new byte[] {(byte) 0xF0}, 8);
    BitVector prevMask = new BitVector(8);
    BitVector result = BitVector.fromBytes(new byte[] {(byte) 0xFF}, 8); // must be overwritten

    MaskOperations.computeChangeInto(result, mask, prevMask, 0);
    assertEquals(0, result.hammingWeight(), "D0 must be 0 per CCSDS Eq. 8");
  }

  @Test
  void computeChangeAtTPositiveIsXor() {
    BitVector mask = BitVector.fromBytes(new byte[] {(byte) 0xCC}, 8);
    BitVector prevMask = BitVector.fromBytes(new byte[] {(byte) 0xAA}, 8);

    BitVector change = MaskOperations.computeChange(mask, prevMask, 1);
    assertEquals(4, change.hammingWeight(), "Dt = Mt XOR Mt-1 (0xCC ^ 0xAA = 0x66)");
  }
}
