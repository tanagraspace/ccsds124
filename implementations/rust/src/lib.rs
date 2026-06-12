//! # CCSDS 124.0-B-1 Rust Implementation
//!
//! Rust implementation of the [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf)
//! lossless compression algorithm of fixed-length housekeeping data.
//!
//! ## Citation
//!
//! If CCSDS 124.0-B-1 contributes to your research, please cite:
//!
//! > D. Evans, G. Labrèche, D. Marszk, S. Bammens, M. Hernandez-Cabronero,
//! > V. Zelenevskiy, V. Shiradhonkar, M. Starcik, and M. Henkel. 2022.
//! > "Implementing the New CCSDS Housekeeping Data Compression Standard
//! > 124.0-B-1 (based on POCKET+) on OPS-SAT-1," *Proceedings of the
//! > Small Satellite Conference*, Communications, SSC22-XII-03.
//! > <https://digitalcommons.usu.edu/smallsat/2022/all2022/133/>
//!
//! ## Design
//!
//! - **Zero external dependencies** - Standard library only
//! - **Safe Rust** - `#![forbid(unsafe_code)]`
//! - **Byte-identical output** - Matches C reference implementation exactly
//! - **Ground systems** - Optimized for 64-bit systems
//!
//! ## API Overview
//!
//! ### High-Level Functions
//!
//! - [`compress()`] - Compress entire input buffer
//! - [`decompress()`] - Decompress entire compressed buffer
//!
//! ### Low-Level Components
//!
//! - [`BitVector`] - Fixed-length bit vectors with 32-bit word storage
//! - [`BitBuffer`] - Variable-length output buffer for compressed data
//! - [`BitReader`] - Sequential bit reading from compressed data
//!
//! ### Encoding Primitives (CCSDS Section 5.2)
//!
//! - [`count_encode`] / [`count_decode`] - Counter encoding (Equation 9)
//! - [`rle_encode`] / [`rle_decode`] - Run-length encoding (Equation 10)
//! - [`bit_extract`] / [`bit_insert`] - Bit extraction (Equation 11)
//!
//! ### Mask Operations (CCSDS Section 4)
//!
//! - [`update_build`] - Build vector update (Equation 6)
//! - [`update_mask`] - Mask vector update (Equation 7)
//! - [`compute_change`] - Change vector computation (Equation 8)
//!
//! ## Usage
//!
//! ```rust,ignore
//! use ccsds124::{compress, decompress};
//!
//! // Sample housekeeping data (90 bytes = 720 bits per packet)
//! let data: Vec<u8> = vec![0u8; 90];
//!
//! // Compress data
//! let compressed = compress(
//!     &data,
//!     720,    // packet_size in bits
//!     1,      // robustness (0-7)
//!     10,     // pt_limit
//!     20,     // ft_limit
//!     50,     // rt_limit
//! ).unwrap();
//!
//! // Decompress data
//! let decompressed = decompress(&compressed, 720, 1).unwrap();
//!
//! assert_eq!(data, decompressed);
//! ```
//!
//! ## References
//!
//! - [CCSDS 124.0-B-1 Standard](https://ccsds.org/Pubs/124x0b1.pdf)
//! - [POCKET+ on OPS-SAT-1 (SmallSat 2022)](https://digitalcommons.usu.edu/smallsat/2022/all2022/133/)

#![forbid(unsafe_code)]
#![warn(clippy::pedantic)]
#![allow(clippy::must_use_candidate)]
#![allow(clippy::missing_errors_doc)]
#![allow(clippy::missing_panics_doc)]

mod bitbuffer;
mod bitreader;
mod bitvector;
mod compress;
mod decode;
mod decompress;
mod encode;
mod error;
mod mask;

pub use bitbuffer::BitBuffer;
pub use bitreader::BitReader;
pub use bitvector::BitVector;
pub use compress::compress;
pub use decode::{bit_insert, count_decode, rle_decode};
pub use decompress::decompress;
pub use encode::{bit_extract, bit_extract_forward, count_encode, rle_encode};
pub use error::Ccsds124Error;
pub use mask::{compute_change, update_build, update_mask};

#[cfg(test)]
mod tests {
    #[test]
    fn test_library_loads() {
        // Basic smoke test - ensure library compiles and loads
        assert!(true);
    }
}
