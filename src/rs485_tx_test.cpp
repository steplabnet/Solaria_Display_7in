/**
 * @file rs485_tx_test.cpp
 * @brief Minimal RS485 periodic-transmit test firmware (ESP32-S3-Touch-LCD-7).
 *
 * Purpose: prove the RS485 transmit path works in isolation. This build brings
 * up ONLY the debug UART (UART0 -> CH343 -> COM port) and the RS485 UART (UART1
 * on GPIO15/16). The LCD/LVGL/touch/SD stack is NOT compiled, so nothing can
 * reconfigure GPIO15/16 or starve the RS485 task.
 *
 * Behaviour:
 *   - Every second, sends one newline-terminated JSON line over RS485 at
 *     38400 8N1, using the same auto-direction TX handshake as the main app
 *     (0x20 wake-up byte -> data -> flush -> short hold).
 *   - Mirrors each sent line to the debug serial (COM port) as "[TX] ...".
 *   - Echoes anything received on RS485 RX as "[RX] ...". Jumper GPIO15->GPIO16
 *     for an MCU-level loopback test (you should then see every [TX] echoed
 *     back as [RX], bypassing the transceiver).
 *
 * This file is the ONLY source compiled in this build; see build_src_filter in
 * platformio.ini on the rs485-tx-test branch.
 */

#include <Arduino.h>
#include <HardwareSerial.h>

// ESP32-S3-Touch-LCD-7 RS485 pins, per Waveshare's official 05_RS485 example:
//   ESP32 RX = GPIO15, ESP32 TX = GPIO16.
// NOTE: the docs' silkscreen labels ("TXD15/RXD16") are from the transceiver's
// side and are the REVERSE of the MCU side. The working example uses RX=15/TX=16.
#ifndef RS485_TX_PIN
#define RS485_TX_PIN 16
#endif
#ifndef RS485_RX_PIN
#define RS485_RX_PIN 15
#endif

static constexpr long     BAUD_DEBUG     = 115200;
static constexpr long     BAUD_RS485     = 38400;
static constexpr uint32_t TX_INTERVAL_MS = 1000;

// UART1 dedicated to RS485 (UART0 stays on the debug/CH343 port).
HardwareSerial RS485Serial(1);

static uint32_t txCounter = 0;

void setup()
{
  Serial.begin(BAUD_DEBUG);
  delay(200);
  Serial.println();
  Serial.println(F("=== RS485 periodic TX test (7\" board) ==="));
  Serial.printf("RS485 on UART1: TX=GPIO%d  RX=GPIO%d  @ %ld 8N1\n",
                RS485_TX_PIN, RS485_RX_PIN, BAUD_RS485);
  Serial.printf("Transmitting one line every %lu ms.\n",
                (unsigned long)TX_INTERVAL_MS);

  RS485Serial.begin(BAUD_RS485, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
}

void loop()
{
  // Simple, newline-terminated payload addressed to display id 14.
  String msg = String("{\"moduleId\":14,\"command\":\"ping\",\"n\":") +
               String(txCounter) + "}";

  // --- RS485 auto-direction TX handshake (mirrors the main firmware) ---
  RS485Serial.write(0x20);        // wake-up byte: flip the transceiver to TX
  delayMicroseconds(500);         // let the auto-direction circuit settle
  RS485Serial.print(msg);
  RS485Serial.write('\n');        // line terminator the receiver reads until
  RS485Serial.flush();            // block until the final stop bit is on the wire
  delay(2);                       // hold before the line is released to RX

  Serial.printf("[TX %lu] %s\n", (unsigned long)txCounter, msg.c_str());

  // Echo any received bytes (useful for the GPIO15->GPIO16 loopback test).
  while (RS485Serial.available())
  {
    int c = RS485Serial.read();
    Serial.printf("[RX] 0x%02X '%c'\n", c,
                  (c >= 32 && c < 127) ? (char)c : '.');
  }

  txCounter++;
  delay(TX_INTERVAL_MS);
}
