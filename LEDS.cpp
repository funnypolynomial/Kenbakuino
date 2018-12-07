#include <Arduino.h>
#include "PINS.h"
#include "MCP.h"
#include "LEDS.h"

void LEDs::Init()
{
  // the 595 controls the 8 data LEDs
  pinMode(PIN_LEDS_DS, OUTPUT);
  pinMode(PIN_LEDS_ST, OUTPUT);
  pinMode(PIN_LEDS_SH, OUTPUT);
  ShiftOut(0x00);
  m_LastData = 0;

  // the 4 control LEDs have a direct link
  for (int Pin = MCP::eInput; Pin <= MCP::eRun; Pin++)
  {
    pinMode(m_pDirectControlPins[Pin], OUTPUT);
    digitalWrite(m_pDirectControlPins[Pin], LOW);
  }
  m_LastControl = 0;
}

void LEDs::ShiftOut(byte LEDs)
{
  digitalWrite(PIN_LEDS_ST, LOW);
  // shift out the bits:
  shiftOut(PIN_LEDS_DS, PIN_LEDS_SH, LSBFIRST, LEDs);  
  //take the latch pin high so the LEDs will light up:
  digitalWrite(PIN_LEDS_ST, HIGH);
}

void LEDs::Display(byte Data, byte Control)
{
  // update the data and control LEDs
  if (Data != m_LastData)
  {
    ShiftOut(Data);
    m_LastData = Data;
  }
  
  if (Control != m_LastControl)
  {
    for (int Pin = MCP::eInput; Pin < MCP::eRun; Pin++)
    {
      digitalWrite(m_pDirectControlPins[Pin], bitRead(Control, Pin)?HIGH:LOW);
    }
    
    if (bitRead(Control, MCP::eRun))
    {
      // the Run LED can do PWM, use the upper 4 bits; 0000 = max brightness (255) 1111 = min (16)
      analogWrite(m_pDirectControlPins[MCP::eRun], 0xFF - (Control & 0xF0));
    }
    else
    {
      digitalWrite(m_pDirectControlPins[MCP::eRun], LOW);
    }
    m_LastControl = Control;
  }
}

// pin mapping in the same order as MCP::tMode
byte LEDs::m_pDirectControlPins[] =
{
  PIN_LED_INP,
  PIN_LED_ADDR,
  PIN_LED_MEM,
  PIN_LED_RUN_PWM
};

LEDs leds = LEDs();
