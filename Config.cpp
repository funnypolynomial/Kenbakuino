#include <Arduino.h>
#include "Config.h"
#include "MCP.h"
#include "Buttons.h"
#include "Clock.h"
#include "Memory.h"

Config::Config():
  m_bToggleBits(true),
  m_iCycleDelayMilliseconds(0),
  m_iEEPROMSlotMap(0x0A)
{
}

void Config::Init()
{
  UpdateFlags(Read(eControlFlags));
  m_iEEPROMSlotMap = Read(eControlEEPROMMap);
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
    case eControlUser6:
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
    case eControlUser6:
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
  m_bToggleBits = (Value & 0x01) == 0x01;
}


Config config = Config();
