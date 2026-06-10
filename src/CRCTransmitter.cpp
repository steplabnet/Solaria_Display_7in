#include <Arduino.h>
#include <ArduinoJson.h>
#include "CRCTransmitter.h"
#include <HardwareSerial.h>

// Enable/Disable debug printing
#define DEBUG_ON true

/**
 * @brief CRC16-CCITT Calculation
 * 
 * Matches the implementation in serial_ping.py for bidirectional compatibility.
 * 
 * Parameters:
 *   - Polynomial: 0x1021 (CCITT standard)
 *   - Initial Value: 0xFFFF
 *   - Input Reflection: No
 *   - Output Reflection: No
 *   - Final XOR: No (use raw result)
 * 
 * @param data Pointer to data bytes
 * @param len Number of bytes
 * @param poly Polynomial (default: 0x1021)
 * @param init Initial CRC value (default: 0xFFFF)
 * @return 16-bit CRC value
 */
uint16_t CRCTransmitter::crc16_ccitt(const uint8_t *data, size_t len, 
                                     uint16_t poly, uint16_t init)
{
    uint16_t crc = init & 0xFFFF;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b)
        {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ poly);
            else
                crc <<= 1;
        }
    }
    return crc & 0xFFFF;
}

/**
 * @brief Adds CRC16 as a JSON element and transmits.
 * 
 * REVISED FORMAT (matching serial_ping.py):
 *   OLD: {"field":"value"}|XXXX\n
 *   NEW: {"field":"value","crc16":"XXXX"}\n
 * 
 * This ensures the CRC is part of the JSON structure, making it compatible
 * with the Python master controller (serial_ping.py) which expects CRC as
 * a JSON element, not pipe-separated.
 * 
 * Process:
 * 1. Convert document to JsonObject
 * 2. Remove any existing "crc16" field
 * 3. Serialize JSON to string (without crc16)
 * 4. Calculate CRC16-CCITT over the bytes
 * 5. Add "crc16" field as uppercase 4-digit hex string
 * 6. Serialize final JSON with CRC field included
 * 7. Transmit with RS485 handshake and newline terminator
 * 
 * @param doc ArduinoJson document to process and transmit
 * @param out Output stream (typically RS485Serial)
 * @param fieldName Name of the CRC field (default: "crc16")
 * @param newline Whether to append newline (default: true)
 * @return CRC16 value that was calculated and transmitted
 */
uint16_t CRCTransmitter::addCrcAndSend(JsonDocument &doc, Stream &out,
                                       const char *fieldName, bool newline)
{
    // 1. Convert to Object
    JsonObject obj = doc.as<JsonObject>();

    // ---------------------------------------------------------
    // CASE A: Payload is NOT an object (e.g. simple array)
    // ---------------------------------------------------------
    if (!obj)
    {
        String safePayload;
        serializeJson(doc, safePayload);
        
        // For non-objects, wrap in a minimal JSON with CRC
        // This preserves the CRC mechanism even for array payloads
        JsonDocument wrapped;
        wrapped["data"] = serialized(safePayload);
        
        // Recalculate for the wrapped version
        String wrappedJson;
        serializeJson(wrapped, wrappedJson);
        uint16_t crc = crc16_ccitt((const uint8_t *)wrappedJson.c_str(), 
                                   wrappedJson.length());
        
        char hexbuf[5];
        snprintf(hexbuf, sizeof(hexbuf), "%04X", crc);
        wrapped[fieldName] = hexbuf;
        
        String finalPacket;
        serializeJson(wrapped, finalPacket);
        if (newline)
            finalPacket += '\n';

        // Transmit with RS485 handshake
        out.write(0x20);        // Wake-up byte for RS485 auto-direction
        delayMicroseconds(500); // Stabilization delay
        out.print(finalPacket);

        HardwareSerial *ser = static_cast<HardwareSerial *>(&out);
        if (ser)
            ser->flush();
        else
            out.flush();

        delay(2); // Hold delay
        
        if (DEBUG_ON)
        {
            Serial.print("[TX] Non-Object CRC: 0x");
            Serial.print(hexbuf);
            Serial.print(" | Payload: ");
            Serial.println(finalPacket);
        }

        return crc;
    }

    // ---------------------------------------------------------
    // CASE B: Standard JSON Object - PRIMARY PATH
    // ---------------------------------------------------------

    // 2. Remove any existing CRC field (for clean calculation)
    if (obj.containsKey(fieldName))
    {
        obj.remove(fieldName);
    }

    // 3. Serialize JSON WITHOUT the crc16 field
    //    This is the data that the CRC will be calculated over
    String canonical;
    canonical.reserve(512);
    serializeJson(doc, canonical);

    // 4. Calculate CRC16-CCITT
    //    Matches Python: serial_ping.py crc16_ccitt() function
    uint16_t crc = crc16_ccitt((const uint8_t *)canonical.c_str(), 
                               canonical.length());

    // 5. Add CRC back as a string field
    //    Format: "XXXX" (uppercase hex, 4 digits)
    char hexbuf[5];
    snprintf(hexbuf, sizeof(hexbuf), "%04X", crc);
    obj[fieldName] = hexbuf;

    // 6. Serialize FINAL JSON with CRC field included
    String finalPacket;
    finalPacket.reserve(1024);
    serializeJson(doc, finalPacket);

    // 7. Add newline terminator
    if (newline)
    {
        finalPacket += '\n';
    }

    // ============================================================
    // 8. RS485 HARDWARE TRANSMISSION SEQUENCE
    // ============================================================
    // This sequence ensures reliable transmission over RS485 by:
    // - Waking up the auto-direction circuit
    // - Stabilizing the bus
    // - Sending data with proper timing
    // - Ensuring all bytes are transmitted before releasing the line

    // A. Wake-up byte
    //    Sends a space (0x20) to activate the RS485 transceiver
    out.write(0x20);

    // B. Stabilization delay
    //    Allows the RS485 auto-direction circuit to activate
    //    500us is sufficient for 38400 baud (1 bit = 26us)
    delayMicroseconds(500);

    // C. Send full packet
    //    Transmits the entire JSON string with CRC in one burst
    out.print(finalPacket);

    // D. Hardware flush
    //    Ensures all bytes are sent from the UART FIFO
    HardwareSerial *ser = static_cast<HardwareSerial *>(&out);
    if (ser)
    {
        ser->flush();
    }
    else
    {
        out.flush();
    }

    // E. Hold delay
    //    Prevents auto-direction from cutting off the stop bit
    delay(2);

    // ============================================================

    if (DEBUG_ON)
    {
        Serial.print("[TX] CRC: 0x");
        Serial.print(hexbuf);
        Serial.print(" | Format: JSON-Element | Payload: ");
        Serial.println(finalPacket);
    }

    return crc;
}

/**
 * @brief Alternative method: Calculate CRC separately for validation
 * 
 * Use this when you need to verify CRC without modifying the document.
 * 
 * @param doc Document to calculate CRC for
 * @return CRC16 value
 */
uint16_t CRCTransmitter::calculateCrc(const JsonDocument &doc)
{
    String payload;
    serializeJson(doc, payload);
    return crc16_ccitt((const uint8_t *)payload.c_str(), payload.length());
}

/**
 * @brief Verify a received message's CRC
 * 
 * Extracts the crc16 field from the document, removes it, recalculates,
 * and compares. Returns true if CRC is valid.
 * 
 * @param doc Document containing the crc16 field
 * @param fieldName Name of the CRC field (default: "crc16")
 * @return true if CRC matches, false otherwise
 */
bool CRCTransmitter::verifyCrc(JsonDocument &doc, const char *fieldName)
{
    if (!doc.containsKey(fieldName))
    {
        if (DEBUG_ON)
            Serial.println("[CRC] Field not found");
        return false;
    }

    // Extract received CRC as string
    const char *receivedCrcStr = doc[fieldName];
    
    // Parse hex string to uint16_t
    uint16_t receivedCrc = 0;
    if (sscanf(receivedCrcStr, "%hX", &receivedCrc) != 1)
    {
        if (DEBUG_ON)
            Serial.printf("[CRC] Invalid format: %s\n", receivedCrcStr);
        return false;
    }

    // Remove CRC field from document
    doc.remove(fieldName);

    // Calculate CRC over remaining data
    String payload;
    serializeJson(doc, payload);
    uint16_t calculatedCrc = crc16_ccitt((const uint8_t *)payload.c_str(), 
                                         payload.length());

    // Compare
    bool isValid = (calculatedCrc == receivedCrc);
    
    if (DEBUG_ON)
    {
        Serial.printf("[CRC] Received: %04X | Calculated: %04X | %s\n",
                      receivedCrc, calculatedCrc,
                      isValid ? "VALID" : "MISMATCH");
    }

    return isValid;
}
