/**
 * @file error.hpp
 * @brief CCSDS 124.0-B-1 error handling.
 *
 * Provides both exception-based and error-code-based error handling
 * for embedded compatibility (-fno-exceptions).
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 */

#ifndef CCSDS124_ERROR_HPP
#define CCSDS124_ERROR_HPP

#include "config.hpp"

#if !CCSDS124_NO_EXCEPTIONS
#include <stdexcept>
#include <string>
#endif

namespace ccsds124 {

/**
 * @brief Error codes for error-code-based error handling.
 *
 * Used when exceptions are disabled (CCSDS124_NO_EXCEPTIONS=1).
 */
enum class Error {
    Ok = 0,          ///< Success
    InvalidArg = -1, ///< Invalid argument
    Overflow = -2,   ///< Buffer overflow
    Underflow = -3,  ///< Buffer underflow (not enough data)
    InvalidData = -4 ///< Invalid/corrupted data
};

/**
 * @brief Get error message for error code.
 * @param error Error code
 * @return Human-readable error message
 */
inline const char* error_string(Error error) noexcept {
    switch (error) {
    case Error::Ok:
        return "Success";
    case Error::InvalidArg:
        return "Invalid argument";
    case Error::Overflow:
        return "Buffer overflow";
    case Error::Underflow:
        return "Buffer underflow";
    case Error::InvalidData:
        return "Invalid or corrupted data";
    default:
        return "Unknown error";
    }
}

#if !CCSDS124_NO_EXCEPTIONS

/**
 * @brief Base exception for CCSDS 124.0-B-1 errors.
 */
class Ccsds124Exception : public std::runtime_error {
public:
    explicit Ccsds124Exception(const std::string& message, Error code = Error::InvalidArg)
        : std::runtime_error(message), error_code_(code) {}

    Error code() const noexcept {
        return error_code_;
    }

private:
    Error error_code_;
};

/**
 * @brief Exception for invalid arguments.
 */
class InvalidArgumentException : public Ccsds124Exception {
public:
    explicit InvalidArgumentException(const std::string& message)
        : Ccsds124Exception(message, Error::InvalidArg) {}
};

/**
 * @brief Exception for buffer overflow.
 */
class OverflowException : public Ccsds124Exception {
public:
    explicit OverflowException(const std::string& message)
        : Ccsds124Exception(message, Error::Overflow) {}
};

/**
 * @brief Exception for buffer underflow.
 */
class UnderflowException : public Ccsds124Exception {
public:
    explicit UnderflowException(const std::string& message)
        : Ccsds124Exception(message, Error::Underflow) {}
};

/**
 * @brief Exception for invalid/corrupted data.
 */
class InvalidDataException : public Ccsds124Exception {
public:
    explicit InvalidDataException(const std::string& message)
        : Ccsds124Exception(message, Error::InvalidData) {}
};

#endif // !CCSDS124_NO_EXCEPTIONS

} // namespace ccsds124

#endif // CCSDS124_ERROR_HPP
