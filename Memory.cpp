#include <Arduino.h>
// disable warnings in EEPROM.h
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#include <EEPROM.h>
#pragma GCC diagnostic pop
#include <avr/pgmspace.h>

#include "Memory.h"
#include "Cpu.h"
#include "Programs.h"
#include "Config.h"
#include "Clock.h"

// Is there a way to find out at runtime?
#define kEEPROMSize (1024)  

#if ARDUINO >= 10604
// prog_uchar (etc) deprecated at 1.6.4, possibly earlier
#define prog_uchar const byte
#endif

// the original plan was to have all sample programs as PROGMEM
// BUT it's convenient to have the "source" of the more complicated ones, so they are
// "assembled" on demand (see Programs.cpp).  The upside is they're easier to develop
// the downside is they're a little slower to load, and they increase Arduino sketch size.
// STOP PRESS: to free up some space, the Sieve program is now stored as PROGMEM, the source is #ifdef'd out
prog_uchar programCounter[]  PROGMEM = {
    0000, 0000, 0000, 0004, 0103, 0001, 0134, 0200, 0344, 0004
};

prog_uchar programCylon[]  PROGMEM = {
    0000, 0000, 0000, 0004, 0023, 0001, 0034, 0200, 0211, 0034, 0200, 0372, 0200, 0343, 0010, 0011, 0034, 0200, 0202, 0200, 0343, 0010, 0343, 0017
};


 // index of the last available byte, excluding any used for confg settings when the RTC has none
int Memory::GetEEPROMTopIdx()
{
  int top = kEEPROMSize - 1;
  if (Clock::RTC_I2C_ADDR == 0x00 || Clock::RTC_USER_SRAM_OFFSET == 0x00)
  {
    top -= 8; // reserve 8 bytes at the top off EEPROM;
  }
  return top;
}

void Memory::Init()
{
  BuildSlots(config.m_iEEPROMSlotMap);
}

bool Memory::LoadStandardProgram(byte Index)
{
  byte* pMem = CPU::cpu->Memory();
  if (Index == 0)
  {
    memcpy_P(pMem, programCounter, 10);
  }
  else if (Index == 1)
  {
    memcpy_P(pMem, programCylon, 24);
  }
  else if (Index == 2)
  {
    Programs::AssembleCountClock(pMem);
  }
  else if (Index == 3)
  {
    Programs::AssembleBCDClock(pMem);
  }
  else if (Index == 4)
  {
    Programs::AssembleBinClock(pMem);
  }
  else if (Index == 5)
  {
    Programs::AssembleDBL(pMem);
  }
  else if (Index == 6)
  {
    Programs::AssembleSieve(pMem);
  }
  else if (Index == 7)
  {
    Programs::AssembleSetRTC(pMem);
  }
  return true;
}

bool Memory::ReadMemoryFromEEPROMSlot(byte Slot)
{
  byte* pMem = CPU::cpu->Memory();
  if (Slot <= 0x07 && m_pSlotSize[Slot])
  {
    int Base = m_pSlotStartAddr[Slot];
    int Size = m_pSlotSize[Slot];
    for (int Offset = 0;  Offset < Size; Offset++)
    {
      pMem[Offset] = EEPROM.read(Base + Offset);
    }
    return true;
  }
  return false;
}

bool Memory::WriteMemoryToEEPROMSlot(byte Slot)
{
  byte* pMem = CPU::cpu->Memory();
  if (Slot <= 0x07 && m_pSlotSize[Slot])
  {
    int Base = m_pSlotStartAddr[Slot];
    int Size = m_pSlotSize[Slot];
    for (int Offset = 0;  Offset < Size; Offset++)
    {
       EEPROM.write(Base + Offset, pMem[Offset]);
    }
    return true;
  }
  return false;
}

void Memory::BuildSlots(byte Map)
{
  // Partitions the EEPROM into up to 8 slots starting with #0 of 256 bytes
  // Bits in the Map indicate if subsequent slots should be half the size, a 1 means halve, a 0 leaves the size as-is.
  // The least significant bit applies to slot #1 -- if it should be half the size of #0 (i.e. 128 bytes)
  // Thus for example, a Map of 0x0A creates the following slots (this is the default):
  //   256, 256, 128, 128, 64, 64, 64
  // 0x00 creates 4 full-size slots (higher slots are ignored):
  //   256, 256, 256, 256
  // These examples are for 1k of EEPROM (ATmega328).

  int Addr = 0;
  int Size = 256;
  int EEPROMSize = GetEEPROMTopIdx() + 1;
  for (int Slot = 0; Slot < 8; Slot++)
  {
    if (Addr < EEPROMSize && Size != 0)
    {
      m_pSlotStartAddr[Slot] = Addr;
      m_pSlotSize[Slot] = min(Size, EEPROMSize - Addr);
    }
    else
    {
      // run out
      m_pSlotStartAddr[Slot] = 0;
      m_pSlotSize[Slot] = 0;
    }
    Addr += Size;
    if (Map & (0x01 << Slot))
    {
      Size /= 2;
    }
  }
}

int Memory::SlotStartAddr(byte Slot)
{
  return m_pSlotStartAddr[Slot % 8];
}

int Memory::SlotSize(byte Slot)
{
  return m_pSlotSize[Slot % 8];
}

Memory memory = Memory();


