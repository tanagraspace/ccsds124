//! Error types for CCSDS 124.0-B-1 compression/decompression.

use std::fmt;

/// Errors that can occur during CCSDS 124.0-B-1 compression or decompression.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Ccsds124Error {
    /// Invalid packet size (must be > 0 and divisible by 8)
    InvalidPacketSize(usize),

    /// Invalid robustness parameter (must be 0-7)
    InvalidRobustness(usize),

    /// Input data length doesn't match expected packet count
    InvalidInputLength { expected: usize, actual: usize },

    /// Unexpected end of input during decompression
    UnexpectedEndOfInput,

    /// Invalid compressed data format
    InvalidFormat(String),

    /// Buffer overflow during compression
    BufferOverflow,

    /// Not enough bits remaining in input (underflow)
    Underflow,

    /// Invalid length parameter
    InvalidLength,
}

impl fmt::Display for Ccsds124Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidPacketSize(size) => {
                write!(
                    f,
                    "invalid packet size: {size} (must be > 0 and divisible by 8)"
                )
            }
            Self::InvalidRobustness(r) => {
                write!(f, "invalid robustness: {r} (must be 0-7)")
            }
            Self::InvalidInputLength { expected, actual } => {
                write!(f, "invalid input length: expected {expected}, got {actual}")
            }
            Self::UnexpectedEndOfInput => {
                write!(f, "unexpected end of input")
            }
            Self::InvalidFormat(msg) => {
                write!(f, "invalid format: {msg}")
            }
            Self::BufferOverflow => {
                write!(f, "buffer overflow")
            }
            Self::Underflow => {
                write!(f, "not enough bits remaining in input")
            }
            Self::InvalidLength => {
                write!(f, "invalid length parameter")
            }
        }
    }
}

impl std::error::Error for Ccsds124Error {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_error_display() {
        let err = Ccsds124Error::InvalidPacketSize(0);
        assert!(err.to_string().contains("invalid packet size"));

        let err = Ccsds124Error::InvalidRobustness(10);
        assert!(err.to_string().contains("invalid robustness"));

        let err = Ccsds124Error::InvalidInputLength {
            expected: 100,
            actual: 50,
        };
        assert!(err.to_string().contains("expected 100"));

        let err = Ccsds124Error::UnexpectedEndOfInput;
        assert!(err.to_string().contains("unexpected end"));

        let err = Ccsds124Error::InvalidFormat("test".to_string());
        assert!(err.to_string().contains("invalid format"));

        let err = Ccsds124Error::BufferOverflow;
        assert!(err.to_string().contains("buffer overflow"));

        let err = Ccsds124Error::Underflow;
        assert!(err.to_string().contains("not enough bits"));

        let err = Ccsds124Error::InvalidLength;
        assert!(err.to_string().contains("invalid length"));
    }
}
