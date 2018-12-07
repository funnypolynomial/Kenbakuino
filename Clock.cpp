#include <Arduino.h>
#include <Wire.h>
#include "Clock.h"

Clock::Clock():
  m_iAddress(0x68)
{
}

void Clock::Init()
{
  Wire.begin();
}

byte Clock::BCD2Dec(byte BCD)
{
  return (BCD/16*10) + (BCD & 0x0F);
}

byte Clock::Dec2BCD(byte Dec)
{
  return (Dec/10*16) + (Dec % 10);
}


// 0:CLK_SECONDS
// 1:CLK_MINUTES
// 2:CLK_HOURS
// 3:CLK_DAY
// 4:CLK_DATE
// 5:CLK_MONTH
// 6:CLK_YEAR
// 7:CLK_CONTROL

byte Clock::ReadByte(byte Index)
{
  Wire.beginTransmission(m_iAddress);
  Wire.write(Index);
  Wire.endTransmission();
 
  Wire.requestFrom(m_iAddress, 1);
  return Wire.read();
}

void Clock::WriteByte(byte Index, byte Value)
{
   Wire.beginTransmission(m_iAddress);
   Wire.write(Index);
   Wire.write(Value);
   Wire.endTransmission();
}

Clock clock = Clock();
