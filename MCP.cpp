#include <Arduino.h>
#include "Config.h"
#include "Clock.h"
#include "LEDS.h"
#include "Buttons.h"
#include "CPU.h"
#include "Memory.h"
#include "MCP.h"

void ExtendedCPU::Init()
{
  CPU::Init();
  // unlike the real thing, clear memory
  ClearAllMemory();
}

bool ExtendedCPU::OnNOOPExtension(byte Op)
{
  // implement extension op-code 
  if (Op == 0360)
  {
    // operation+index in A, arg in B
    byte A = Read(REG_A_IDX);
    byte B = Read(REG_B_IDX);
    if (mcp.SystemCall(A, B))
    {
      Write(REG_A_IDX, A);
      Write(REG_B_IDX, B);
      config.SetCPUSpeed(0);
      return true;
    }
    else
    {
      return false;
    }
  }
  return CPU::OnNOOPExtension(Op);
}

////////////////////////////////////////////////////
// "master control program" (heh)

// Extension: Stop+Clear: blank display
// Extension: Stop+BitN: load pre-defined program N
// Extension: Clear+Store clear memory
// Extension: BitN+Read read from EEPROM slot N
// Extension: BitN+Store write to EEPROM slot N
// Extension: Stop+Read sys read
// Extension: Stop+Store sys write
// Extension: BitN+Stop set CPU speed to N


void MCP::Init()
{
  m_bRunning = false;
  Splash();
  m_Data = 0x00;
  m_Control = 0x00;
  m_Address = 0x00;
  SetMode(eInput);
  leds.Display(m_Data, m_Control);
}

void MCP::Splash()
{
  // put on a little light show
  leds.Display(0xFF, 0x0F);
  delay(250);
  int Bit;
  for (Bit = 7; Bit >= 0; Bit--)
  {
    leds.Display(bit(Bit), 0);
    delay(100);
  }
  for (Bit = eInput; Bit <= eRun; Bit++)
  {
    leds.Display(0, bit(Bit));
    delay(100);
  }
}

void MCP::Loop()
{
  // main loop -- step the CPU, look for buttons
  word State;
  word Pressed;
  if (m_bRunning)
  {
    if (buttons.GetButtons(State, Pressed, false))
    {
      HandleButtonRunning(State, Pressed);
    }
    
    if (m_bRunning)
    {
      m_bRunning = CPU::cpu->Step();
      m_Data = CPU::cpu->Read(REG_OUTPUT_IDX);
      // slow things down...
      if (config.m_iCycleDelayMilliseconds)
      {
        delay(config.m_iCycleDelayMilliseconds);
      }
    }
  }
  else
  {
    if (buttons.GetButtons(State, Pressed, true))  // debounce when not running
    {
      HandleButtonHalted(State, Pressed);
    }
  }
  leds.Display(m_Data, m_Control);
}

void MCP::SetControlLEDs(byte LEDs)
{
  m_Control = LEDs;
}

void MCP::SetMode(byte Mode)
{
  m_Control = (Mode != eNone)?bit(Mode):0;
}

void MCP::HandleButtonHalted(word State, word Pressed)
{
  // button-press while we're not running
  int Btn;
  if (buttons.GetButtonDown(Pressed, Btn))
  {
    int Chord = Buttons::eUnused;
    for (int Btn2 = Buttons::eBit0; Btn2 < Buttons::eUnused; Btn2++)
    {
      if (Btn2 != Btn && buttons.IsPressed(State, Btn2))
      {
        Chord = Btn2;
        break;
      }
    }
    switch (Btn)
    {
      case Buttons::eBit0:
      case Buttons::eBit1:
      case Buttons::eBit2:
      case Buttons::eBit3:
      case Buttons::eBit4:
      case Buttons::eBit5:
      case Buttons::eBit6:
      case Buttons::eBit7:
      {
        OnInputButton(Btn, Chord);
        break;
      }
      case Buttons::eInputClear:
      {
        OnInputClear(Chord);
        break; 
      }
      case Buttons::eAddressDisplay:
      {
        OnAddressDisplay(Chord);
        break; 
      }
      case Buttons::eAddressSet:
      {
        OnAddressSet(Chord);
        break; 
      }
      case Buttons::eMemoryRead:
      {
        OnMemoryRead(Chord);
        break; 
      }
      case Buttons::eMemoryStore:
      {
        OnMemoryStore(Chord);
        break; 
      }
      case Buttons::eRunStart:
      {
        OnRunStart(Chord);
        break; 
      }
      case Buttons::eRunStop:
      {
        OnRunStop(Chord);
        break; 
      }
    }    
  }
}

void MCP::HandleButtonRunning(word State, word Pressed)
{
  // button-press while we ARE running
  int Btn;
  if (buttons.GetButtonDown(Pressed, Btn))
  {
    int Chord = Buttons::eUnused;  // no extensions while running
    switch (Btn)
    {
      case Buttons::eBit0:
      case Buttons::eBit1:
      case Buttons::eBit2:
      case Buttons::eBit3:
      case Buttons::eBit4:
      case Buttons::eBit5:
      case Buttons::eBit6:
      case Buttons::eBit7:
      {
        OnInputButton(Btn, Chord);
        break;
      }
      case Buttons::eInputClear:
      {
        OnInputClear(Chord);
        break; 
      }
      case Buttons::eRunStop:
      {
        OnRunStop(Chord);
        break; 
      }
    }    
  }
}

void MCP::OnInputButton(int Btn, byte Chord)
{
  // one of the input buttons has been hit
  byte Data = CPU::cpu->Read(REG_INPUT_IDX);
  if (Chord == Buttons::eRunStop)
  {
    // Extension: Stop+BitN:load pre-defined program N
    if (memory.LoadStandardProgram(Btn))
    {
      Data = bit(Btn);
    }
  }
  else
  {
    if (config.m_bToggleBits)
    {
      bitWrite(Data, Btn, !bitRead(Data, Btn));
    }
    else
    {
      bitSet(Data, Btn);
    }
  }
  if (!m_bRunning)
  {
    m_Data = Data;
    SetMode(eInput);
  }
  CPU::cpu->Write(REG_INPUT_IDX, Data);
}

void MCP::OnInputClear(byte Chord)
{
  CPU::cpu->Write(REG_INPUT_IDX, 0);
  if (!m_bRunning)
  {
    m_Data = 0;
    SetMode(eInput);
  }
  if (Chord == Buttons::eRunStop)
  {
    // Extension: Stop+Clear= blank display
    SetMode(eNone);
  }
}

void MCP::OnAddressDisplay(byte Chord)
{
  m_Data = m_Address;
  SetMode(eAddress);
}

void MCP::OnAddressSet(byte Chord)
{
  m_Address = CPU::cpu->Read(REG_INPUT_IDX);
  Blink(eAddress);
}

void MCP::OnMemoryRead(byte Chord)
{
  if (Buttons::eBit0 <= Chord && Chord <= Buttons::eBit7)
  {
    // Extension: BitN+Read read from EEPROM page N
    m_Data = memory.ReadMemoryFromEEPROMSlot(Chord)?bit(Chord):0;
    SetMode(eNone);
  }
  else if (Chord == Buttons::eRunStop)
  {
    // Extension: Stop+Read sys read
    byte A = m_Address & 0x7F;
    byte B = 0;
    if (SystemCall(A, B))
    {
      CPU::cpu->Write(REG_OUTPUT_IDX, B);
    }
    m_Data = B;
    SetMode(eNone);
  }
  else
  {
    m_Data = CPU::cpu->Read(m_Address++);
    SetMode(eMemory);
    Blink(eRun);
  }
}

void MCP::OnMemoryStore(byte Chord)
{
  if (Buttons::eBit0 <= Chord && Chord <= Buttons::eBit7)
  {
    // Extension: BitN+Store write to EEPROM page N
    m_Data = memory.WriteMemoryToEEPROMSlot(Chord)?bit(Chord):0;
    SetMode(eNone);
  }
  else if (Chord == Buttons::eRunStop)
  {
    // Extension: Stop+Store sys write
    byte A = m_Address | 0x80;
    byte B = CPU::cpu->Read(REG_INPUT_IDX);
    if (SystemCall(A, B))
    {
      CPU::cpu->Write(REG_OUTPUT_IDX, B);
    }
    m_Data = B;
    SetMode(eNone);
  }
  else if (Chord == Buttons::eInputClear)
  {
    // Extension: Clear+Store clear memory
    CPU::cpu->ClearAllMemory();
    m_Address = REG_P_IDX + 1;
    config.m_iCycleDelayMilliseconds = 0;
    CPU::cpu->Write(REG_P_IDX, m_Address);
    SetMode(eNone);
  }
  else
  {
    byte Value = CPU::cpu->Read(REG_INPUT_IDX);
    CPU::cpu->Write(m_Address++, Value);
    Blink(eRun);
  }
}

void MCP::OnRunStart(byte Chord)
{
  SetMode(eRun);
  if (Chord == Buttons::eRunStop)  // single step
  {
    CPU::cpu->Step();
    m_Data = CPU::cpu->Read(REG_OUTPUT_IDX);
    Blink(eRun);
  }
  else
  {
    // just go
    m_bRunning = true;
  }
}

void MCP::OnRunStop(byte Chord)
{
  if (Buttons::eBit0 <= Chord && Chord <= Buttons::eBit7)
  {
    // Extension: BitN+Stop set CPU speed to N
    config.SetCPUSpeed(Chord);
    Blink(eRun);
  }
  m_bRunning = false;
  SetMode(eRun);
}

bool MCP::NOOPExtensionCallback(void* pThis, byte Op)
{
  MCP* pMCP = (MCP*)pThis;
  if (pMCP)
  {
    return pMCP->OnNOOPExtension(Op);
  }
  return true;
}

void MCP::Blink(byte LED)
{
  byte Control = m_Control;
  bitWrite(Control, LED, !bitRead(Control, LED));
  leds.Display(m_Data, Control);
  delay(50);
  leds.Display(m_Data, m_Control);
}

bool MCP::SystemCall(byte& A, byte& B)
{
  byte Write = A & 0x80;
  byte Index = A & 0x7F;
  if (Write)
  {
    if (Index == 0x7F)
    {
      A = 0;  // A = 0 means extensions supported
    }
    else
    {
      return config.Write(Index, B);
    }
  }
  else
  {
    B = config.Read(Index);
  }
  return true;
}


MCP mcp = MCP();
