/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

/**
 * Exception thrown by CCSDS 124.0-B-1 compression/decompression operations.
 *
 * <p>This exception indicates an error during CCSDS 124.0-B-1 processing, such as invalid
 * parameters, buffer overflow/underflow, or corrupted data.
 */
public class Ccsds124Exception extends Exception {

  private static final long serialVersionUID = 1L;

  /**
   * Constructs a new Ccsds124Exception with the specified message.
   *
   * @param message the detail message
   */
  public Ccsds124Exception(String message) {
    super(message);
  }

  /**
   * Constructs a new Ccsds124Exception with the specified message and cause.
   *
   * @param message the detail message
   * @param cause the cause of this exception
   */
  public Ccsds124Exception(String message, Throwable cause) {
    super(message, cause);
  }
}
