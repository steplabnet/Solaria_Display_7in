#ifndef CRCTRANSMITTER_H
#define CRCTRANSMITTER_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @class CRCTransmitter
 * @brief Handles CRC16-CCITT calculation and transmission for JSON payloads
 * 
 * This class provides methods for:
 * 1. Calculating CRC16-CCITT checksums (matching serial_ping.py)
 * 2. Adding CRC as a JSON element (not pipe-separated)
 * 3. Transmitting with proper RS485 handshake
 * 4. Verifying received messages with CRC validation
 * 
 * Format: {"field":"value","crc16":"XXXX"}\n
 * 
 * The CRC is calculated over the JSON string WITHOUT the crc16 field,
 * then added back before transmission. This matches the format and
 * algorithm used by the Python master controller (serial_ping.py).
 */
class CRCTransmitter
{
public:
    /**
     * @brief Calculates CRC16-CCITT checksum
     * 
     * Algorithm Parameters (CCITT standard):
     *   - Polynomial: 0x1021
     *   - Initial Value: 0xFFFF
     *   - Input Reflection: No
     *   - Output Reflection: No
     *   - Final XOR: No
     * 
     * Compatible with:
     *   - Python: serial_ping.py crc16_ccitt()
     *   - C/C++: Standard CRC16-CCITT implementations
     * 
     * @param data Pointer to byte array
     * @param len Number of bytes
     * @param poly Polynomial (default: 0x1021)
     * @param init Initial value (default: 0xFFFF)
     * @return 16-bit CRC value
     */
    static uint16_t crc16_ccitt(const uint8_t *data, size_t len,
                                uint16_t poly = 0x1021,
                                uint16_t init = 0xFFFF);

    /**
     * @brief Adds CRC16 as JSON element and transmits
     * 
     * Process:
     * 1. Removes any existing "crc16" field
     * 2. Serializes JSON to string (without crc16)
     * 3. Calculates CRC16-CCITT over the bytes
     * 4. Adds "crc16" field as uppercase hex string
     * 5. Serializes final JSON with CRC field
     * 6. Transmits with RS485 handshake and newline
     * 
     * Format: {"field":"value","crc16":"XXXX"}\n
     * 
     * RS485 Transmission Sequence:
     *   - Wake-up byte (0x20) for auto-direction circuit
     *   - 500µs stabilization delay
     *   - Send complete JSON packet
     *   - Hardware flush for UART FIFO
     *   - 2ms hold delay for line release safety
     * 
     * @param doc ArduinoJson document to serialize and send
     * @param out Output stream (typically RS485Serial)
     * @param fieldName CRC field name (default: "crc16")
     * @param newline Append newline terminator (default: true)
     * @return CRC16 value that was calculated
     */
    static uint16_t addCrcAndSend(JsonDocument &doc, Stream &out,
                                  const char *fieldName = "crc16",
                                  bool newline = true);

    /**
     * @brief Calculates CRC for a document without transmission
     * 
     * Use this for validation, logging, or debugging without sending.
     * The document is not modified.
     * 
     * @param doc Document to calculate CRC for
     * @return CRC16 value
     */
    static uint16_t calculateCrc(const JsonDocument &doc);

    /**
     * @brief Verifies a received message's CRC
     * 
     * Process:
     * 1. Extracts the crc16 field from the document
     * 2. Parses the hex string to uint16_t
     * 3. Removes crc16 field from document
     * 4. Recalculates CRC over remaining data
     * 5. Compares calculated vs. received CRC
     * 
     * WARNING: This method MODIFIES the document by removing the crc16 field.
     * Make a copy first if you need to preserve the original.
     * 
     * @param doc Document to verify (modified: crc16 field removed)
     * @param fieldName CRC field name (default: "crc16")
     * @return true if CRC matches, false otherwise
     */
    static bool verifyCrc(JsonDocument &doc, const char *fieldName = "crc16");
};

#endif // CRCTRANSMITTER_H
