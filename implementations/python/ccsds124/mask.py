"""
CCSDS 124.0-B-1 mask update logic.

Implements CCSDS 124.0-B-1 Section 4 (Mask Update):
- Build Vector Update (Equation 6)
- Mask Vector Update (Equation 7)
- Change Vector Computation (Equation 8)

MicroPython compatible - no typing module imports.
"""

# Import for type hints only
if False:  # noqa: SIM108
    from ccsds124.bitvector import BitVector


def update_build(
    build: "BitVector",
    input_vec: "BitVector",
    prev_input: "BitVector",
    new_mask_flag: bool,
    t: int,
) -> None:
    """
    Update the build vector (CCSDS Equation 6).

    Build vector accumulates bits that have changed over time.
    When new_mask_flag is set, the build is used to replace the mask
    and is then reset to zero.

    Equation:
    - Bt = (It XOR It-1) OR Bt-1  (if t > 0 and new_mask_flag = 0)
    - Bt = 0                      (if t = 0 or new_mask_flag = 1)

    Args:
        build: Build vector to update (modified in place)
        input_vec: Current input vector It
        prev_input: Previous input vector It-1
        new_mask_flag: Whether a new mask is being established
        t: Current time step
    """
    if t == 0 or new_mask_flag:
        # Reset build to zero
        build.zero()
    else:
        # Bt = (It XOR It-1) OR Bt-1
        # Create temporary for changes
        changes = input_vec.copy()
        changes.xor(changes, prev_input)

        # Update build: build = changes OR build
        build.or_(build, changes)


def update_mask(
    mask: "BitVector",
    input_vec: "BitVector",
    prev_input: "BitVector",
    build_prev: "BitVector",
    new_mask_flag: bool,
) -> None:
    """
    Update the mask vector (CCSDS Equation 7).

    The mask tracks which bits are unpredictable.
    When new_mask_flag is set, the mask is replaced with the build vector.

    Equation:
    - Mt = (It XOR It-1) OR Mt-1  (if new_mask_flag = 0)
    - Mt = (It XOR It-1) OR Bt-1  (if new_mask_flag = 1)

    Args:
        mask: Mask vector to update (modified in place)
        input_vec: Current input vector It
        prev_input: Previous input vector It-1
        build_prev: Previous build vector Bt-1
        new_mask_flag: Whether a new mask is being established
    """
    # Calculate changes: It XOR It-1
    changes = input_vec.copy()
    changes.xor(changes, prev_input)

    if new_mask_flag:
        # Mt = changes OR Bt-1
        mask.or_(changes, build_prev)
    else:
        # Mt = changes OR Mt-1
        mask.or_(mask, changes)


def compute_change(
    change: "BitVector",
    mask: "BitVector",
    prev_mask: "BitVector",
    t: int,
) -> None:
    """
    Compute the change vector (CCSDS Equation 8).

    The change vector tracks which mask bits changed between time steps.
    This is used in encoding to communicate mask updates.

    Equation:
    - Dt = Mt XOR Mt-1  (if t > 0)
    - Dt = 0            (if t = 0, CCSDS Eq. 8: no change at initialization)

    Args:
        change: Change vector to compute (modified in place)
        mask: Current mask vector Mt
        prev_mask: Previous mask vector Mt-1
        t: Current time step
    """
    if t == 0:
        # CCSDS Eq. 8: D0 = 0 — both encoder and decoder start from the
        # same user-specified M0, so there is no change to communicate
        change.zero()
    else:
        # Dt = Mt XOR Mt-1
        change.xor(mask, prev_mask)
