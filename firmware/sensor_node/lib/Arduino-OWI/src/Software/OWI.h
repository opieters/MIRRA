/**
 * @file Software/OWI.h
 * @version 1.1
 *
 * @section License
 * Copyright (C) 2017, Mikael Patel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef SOFTWARE_OWI_H
#define SOFTWARE_OWI_H

#include "OWIBase.h"
#include <Arduino.h>

/**
 * One Wire Interface (OWI) Bus Manager template class using GPIO.
 * @param[in] PIN board pin for 1-wire bus.
 */
namespace Software {
class OWI : public OWIBase {
public:
  /**
   * Construct one wire bus connected to the given template pin
   * parameter.
   */
  OWI(uint8_t m_pin) : m_pin(m_pin)
  {
    pinMode(m_pin, OUTPUT_OPEN_DRAIN);
    //m_pin.open_drain();
  }

  /**
   * @override{OWI}
   * Reset the one wire bus and check that at least one device is
   * presence.
   * @return true(1) if successful otherwise false(0).
   */
  virtual bool reset()
  {
    uint8_t retry = OWIBase::RESET_RETRY_MAX;
    bool res;
    do {
      pinMode(m_pin, OUTPUT);
      //m_pin.output();
      delayMicroseconds(490);
      noInterrupts();
      pinMode(m_pin, INPUT);
      //m_pin.input();
      delayMicroseconds(70);
      res = digitalRead(m_pin);
      //res = m_pin;
      interrupts();
      delayMicroseconds(410);
    } while (retry-- && res);
    return (res == 0);
  }

  /**
   * @override{OWI}
   * Read the given number of bits from the one wire bus. Default
   * number of bits is 8.
   * @param[in] bits to be read.
   * @return value read.
   */
  virtual uint8_t read(uint8_t bits = CHARBITS)
  {
    uint8_t adjust = CHARBITS - bits;
    uint8_t res = 0;
    while (bits--) {
      noInterrupts();
      pinMode(m_pin, OUTPUT);
      //m_pin.output();
      delayMicroseconds(6);
      pinMode(m_pin, INPUT);
      //m_pin.input();
      delayMicroseconds(9);
      res >>= 1;
      res |= (digitalRead(m_pin) ? 0x80 : 0x00);
      interrupts();
      delayMicroseconds(55);
    }
    res >>= adjust;
    return (res);
  }

  /**
   * @override{OWI}
   * Write the given value to the one wire bus. The bits are written
   * from LSB to MSB.
   * @param[in] value to write.
   * @param[in] bits to be written.
   */
  virtual void write(uint8_t value, uint8_t bits = CHARBITS)
  {
    while (bits--) {
      noInterrupts();
      pinMode(m_pin, OUTPUT);
      //m_pin.output();
      if (value & 0x01) {
        delayMicroseconds(6);
        pinMode(m_pin, INPUT);
        //m_pin.input();
        delayMicroseconds(64);
            }
            else {
        delayMicroseconds(60);
        pinMode(m_pin, INPUT);
        //m_pin.input();
        delayMicroseconds(10);
      }
      interrupts();
      value >>= 1;
    }
  }

  using ::OWIBase::read;
  using ::OWIBase::write;

protected:
  /** 1-Wire bus pin. */
  uint8_t m_pin;
};
};
#endif
