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

// Chords; Btn1+Btn2 means, press hold Btn1 then press Btn2
// Extension: Stop+Clear: blank display
// Extension: Stop+BitN: load pre-defined program N
// Extension: Clear+Store clear memory
// Extension: BitN+Read read from EEPROM slot N
// Extension: BitN+Store write to EEPROM slot N
// Extension: Stop+Read sys read
// Extension: Stop+Store sys write
// Extension: BitN+Stop set CPU speed to N
// Extension: BitN+Disp write memory out to Serial
// Extension: BitN+Set set memory from Serial
//
// Press at power on to configure program to auto-run
// * Stop & BitN  = load built-in program N
// * Read & BitN  = load from EEPROM slot N
// * Stop         = turn auto-run off
// and
// * Stop & Clear = reset config to defaults




void MCP::Init()
{
  m_bRunning = false;
  Splash();
  m_Data = 0x00;
  m_Control = 0x00;
  m_Address = 0x00;
  SetMode(eInput);
  leds.Display(m_Data, m_Control);

  AutoRun(config.m_iAutoRunProgram);
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

void MCP::HandleButtonRunning(word , word Pressed)
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
  if (Chord <= Buttons::eBit7)
  {
    // Extension: BitN+Disp = write program memory to serial
    SerializeMemory(false, Chord);
  }
  else
  {
    m_Data = m_Address;
    SetMode(eAddress);
  }
}

void MCP::OnAddressSet(byte Chord)
{
  if (Chord <= Buttons::eBit7)
  {
    // Extension: BitN+Stor = store program memory from serial
    SerializeMemory(true, Chord);
  }
  else
  {
    m_Address = CPU::cpu->Read(REG_INPUT_IDX);
    Blink(eAddress);
  }
}

void MCP::OnMemoryRead(byte Chord)
{
  if (Chord <= Buttons::eBit7)
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
  if (Chord <= Buttons::eBit7)
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
  if (Chord <= Buttons::eBit7)
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
    B = config.Read(Index, B);
  }
  return true;
}


void MCP::SerializeMemory(bool Input, byte Chord)
{
  unsigned long baud = 4800UL * (0x01 << ((Chord - Buttons::eBit0) % 4));  // 4800, 9600, 19k2, 38k4
  SetMode(eNone);
  Serial.flush();
  Serial.end();
  Serial.begin(baud); // potentially drop the baud rate
  byte Control = m_Control;
  word State;
  word Pressed;
  if (Input)  // READ program memory from Serial
  {
    int bitsPerDigit = 3; // octal
    int addr = 0;
    int value = -1;
    int ch = -1;
    byte sum = 0;
  
    Serial.print("[0");
    while (addr < 256)
    {
      if (buttons.GetButtons(State, Pressed, false) && buttons.IsPressed(Pressed, Buttons::eRunStop))
      {
        Serial.println(" STOP");
        break;
      }
      
      if (Serial.available() > 0) 
      {
        ch = Serial.read();
        if (ch == 'x')   // hex
        {
          bitsPerDigit = 4;
        }
        else if (ch == 'e' || 
                 ch == 's')   // end/stop
        {
          break;
        }
        else                  // digit?
        {
          int digit = (ch > '9')?ch - 'A' + 10:ch - '0';
          if ((bitsPerDigit == 3 && 0 <= digit && digit <= 7)  || // valid octal
              (bitsPerDigit == 4 && 0 <= digit && digit <= 15))   // valid hex
          {
            if (value == -1)
            {
              value = digit;
              bitsPerDigit = 3; // default is octal
            }
            else
              value = (value << bitsPerDigit) | digit;
          }
          else if (value != -1) // hit a non-digit and we've built up a value, save it
          {
            CPU::cpu->Write(addr++, value);
            sum += value;
            bitWrite(Control, eRun, !bitRead(Control, eRun)); // flash Run
            leds.Display(m_Data, Control);
            if ((addr % 16) == 0 && addr < 256)
              Serial.print(addr / 16, HEX); // progress
            value = -1;
          }
        }
      }
    }
    
    if (value != -1 && addr < 256)  // ended on a number with no delimeter, save it
    {
      CPU::cpu->Write(addr, value);
      sum += value;
    }
  
    Serial.print("] len=0x");
    Serial.print(addr, HEX);
    Serial.print(" chk=0x");
    Serial.println(sum, HEX);
    SetMode(eRun);
  }
  else  // WRITE program memory to Serial
  {
    for (int addr = 0; addr < 256; addr++)
    {
      if (buttons.GetButtons(State, Pressed, false) && buttons.IsPressed(Pressed, Buttons::eRunStop))
      {
        Serial.println(" STOP");
        break;
      }
      
      byte b = CPU::cpu->Read(addr);
      
      // don't flood the serial buffer
      int ctr = 0;
      while (Serial.availableForWrite() < 5 && ctr++ < 5)
        delay(10);  
     
#if 1 // OCTAL
        Serial.print('0');
        Serial.print((b >> 6) & 0x07, OCT);
        Serial.print((b >> 3) & 0x07, OCT);
        Serial.print((b >> 0) & 0x07, OCT);
        Serial.print(',');
#else // HEX
        Serial.print("0x");
        Serial.print((b >> 4) & 0x0F, HEX);
        Serial.print((b >> 0) & 0x0F, HEX);
        Serial.print(',');
#endif        
      if ((addr % 16) == 15)
        Serial.println();
      bitWrite(Control, eRun, !bitRead(Control, eRun)); // flash Run
      leds.Display(m_Data, Control);
    }
    SetMode(eInput);
  }
    
  Serial.flush();
  Serial.end();
  Serial.begin(38400);  // restore the baud
}

void MCP::AutoRun(byte Auto)
{
  // Run the program, Auto is 0b00XX0NNN where XX: 00 = off, 01 = built-in, 10 = EEPROM 
  // NNN is built-in program number or EEPROM slot
  byte Mode = Auto & 0b11111000;
  byte Prog = Auto & 0b00000111;
  if (Mode == AUTO_RUN_EEPROM || Mode == AUTO_RUN_BUILTIN)
  {
    if (Mode == AUTO_RUN_EEPROM)
      OnMemoryRead(Prog);                     // read from EEPROM slot NNN (BitN+Read)
    else
      OnInputButton(Prog, Buttons::eRunStop); // load pre-defined program NNN (Stop+BitN) 
    OnRunStart(Buttons::eUnused);
  }
}


MCP mcp = MCP();

