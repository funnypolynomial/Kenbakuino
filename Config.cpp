#include <Arduino.h>
#include "Config.h"
#include "MCP.h"
#include "Buttons.h"
#include "Clock.h"
#include "Memory.h"

#define TOGGLE_BITS_FLAG 0x01

Config::Config():
  m_bToggleBits(true),
  m_iCycleDelayMilliseconds(0),
  m_iEEPROMSlotMap(0x0A),
  m_iAutoRunProgram(0)
{
}

void Config::Init()
{
  CheckStartupConfig();
  UpdateFlags(Read(eControlFlags));
  byte slots = Read(eControlEEPROMMap);
  if (slots != 0xFF)  // probably uninitialised or no RTC battery-backed ram, ignore
    m_iEEPROMSlotMap = slots;
  m_iAutoRunProgram = Read(eControlAutoRun);
}

byte Config::Read(byte Item)
{
  // read the SysInfo item with the given index
  switch (Item)
  {
    case eClockSeconds:
    case eClockMinutes:
    case eClockHours24:
    case eClockDay:
    case eClockDate:
    case eClockMonth:
    case eClockYear:
    case eClockControl:
    {
      // read the clock values from the DS1307 RTC.  BCD
      byte Value = clock.ReadByte(Item);
      if (Item == eClockSeconds)
      {
        Value &= 0x7F;  // mask off CH
      }
      else if (Item == eClockHours24)
      {
        if (bitRead(Value, 6)) // clock is 12-hr, convert
        {
          byte Hour = clock.BCD2Dec(Value & 0x1F);
          if (bitRead(Value, 5))  // PM, add
          {
            Hour += 12;
          }
          if (Hour == 24)
            Hour = 0;
          Value = clock.Dec2BCD(Hour);
        }
      }
      return Value;
    }
  
    case eControlFlags:
    case eControlEEPROMMap:
    case eControlUser1:
    case eControlUser2:
    case eControlUser3:
    case eControlUser4:
    case eControlUser5:
    case eControlAutoRun:
      // read from the user memory on the DS1307 RTC
      return clock.ReadByte(Item);
      
    case eControlLEDs: 
      // ignore, nothing to read
      break;
    case eControlRandom:
      // return a random byte
      return random(0, 256);
    case eControlDelayMilliSec:
      // ignore, nothing to read
      break;
    case eControlSerial:
    {
      if (Serial.available() > 0) 
        return Serial.read();
      break;
    }
  }
  return 0;
}

bool Config::Write(byte Item, byte Value)
{
  // write the SysInfo item with the given index
  // false means HALT
  switch (Item)
  {
    case eClockSeconds:
    case eClockMinutes:
    case eClockHours24:
    case eClockDay:
    case eClockDate:
    case eClockMonth:
    case eClockYear:
    case eClockControl:
    {
      // write clock values to the DS1307 RTC.  No validation.  BCD
      if (Item == eClockHours24)
      {
        if (bitRead(clock.ReadByte(Item), 6))   // clock is 12-hr, adjust
        {
          byte Hour = clock.BCD2Dec(Value);
          bool PM = false;
          if (Hour > 12) 
          {
            Hour -= 12;
            PM = true;
          }
          else if (Hour == 0)
          {
            Hour = 12;
          }
          Value = clock.Dec2BCD(Hour);
          bitWrite(Value, 5, PM);
          bitWrite(Value, 6, 1);
        }
      }
      clock.WriteByte(Item, Value);
      break;
    }
    
    case eControlFlags:
    {
      // write thru to the user memory in the DS1307 RTC and update local settings
      UpdateFlags(Value);
      clock.WriteByte(Item, Value);
      break;
    }
    case eControlEEPROMMap:
    {
      // program slots map
      memory.BuildSlots(Value);
      clock.WriteByte(Item, Value);
      break;
    }
    case eControlUser1:
    case eControlUser2:
    case eControlUser3:
    case eControlUser4:
    case eControlUser5:
    case eControlAutoRun:
    {
      clock.WriteByte(Item, Value);
      break;
    }
    
    case eControlLEDs: 
    {
      // set the controls LEDs
      mcp.SetControlLEDs(Value);
      break;
    }
    case eControlRandom:
    {
      // *writing* seeds the random numbers
      if (Value)  // non-zero, use it
        randomSeed(Value);
      else  // zero, use the time as seed
        randomSeed(word(clock.ReadByte(eClockSeconds), clock.ReadByte(eClockMinutes)));
      break;
    }
    case eControlDelayMilliSec:
    {
      // delay, break it up and check for HALT
      while (Value > 50)
      {
        word State;
        word Pressed;
        if (buttons.GetButtons(State, Pressed, false) && buttons.IsPressed(State, Buttons::eRunStop))
        {
          return false;
        }
        delay(50);
        Value -= 50;
      }
      delay(Value);
      break;
    }
    case eControlSerial:
    {
      Serial.write(Value);
      break;
    }
  }
  return true;
}

void Config::SetCPUSpeed(byte Bit)
{
  // 2^N: 1ms..128ms
  // Wikipedia says "...actual execution speed averaged below 1000 instructions per second..." 
  m_iCycleDelayMilliseconds = 1 << Bit;
}


void Config::UpdateFlags(byte Value)
{
  m_bToggleBits = (Value & TOGGLE_BITS_FLAG) == TOGGLE_BITS_FLAG;
}

void Config::CheckStartupConfig()
{
  // Check buttons held at startup to configure program auto-run
  // * Stop & BitN   = load built-in program N
  // * Read & BitN   = load from EEPROM slot N
  // * Stop         = turn auto-run off
  // * Stop & Clear = reset config to defaults
  word State, NewPressed;
  if (buttons.GetButtons(State, NewPressed, false)) // read button state
  {
    if (buttons.IsPressed(State, Buttons::eRunStop) || buttons.IsPressed(State, Buttons::eMemoryRead))  // Stop or Read is down
    {
      byte Mode = buttons.IsPressed(State, Buttons::eRunStop)?AUTO_RUN_BUILTIN:AUTO_RUN_EEPROM;
      // which BitN?
      for (int Btn = Buttons::eBit0; Btn <= Buttons::eBit7; Btn++)
      {
        if (buttons.IsPressed(State, Btn))
        {
          Write(eControlAutoRun, Mode + Btn);
          return; // only the first we see
        }
      }
      
      if (Mode == AUTO_RUN_BUILTIN) // Stop but no BitN, clear
      {
        Write(eControlAutoRun, 0);
        if (buttons.IsPressed(State, Buttons::eInputClear)) // Stop & Clear, reset config to defaults
        {
          Write(eControlFlags, TOGGLE_BITS_FLAG);
          Write(eControlEEPROMMap, m_iEEPROMSlotMap);
        }
      }
    }
  }
}


Config config = Config();

