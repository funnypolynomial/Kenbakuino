#include <Arduino.h>
#include "CPU.h"

// Kenbak-uino
// The "CPU"

/*
the op-codes:
----Bit Test & Manipulate----   R == 2
P--           -Q-         --R
0 = Set 0     0
1 = Set 1     1
2 = Skip 0    2           2
3 = Skip 1    3
              4
              5
              6
              7 = bit number
 
----Shifts/Rotates----          R == 1
P--           -Q-         --R
0 = Rt Shift  0 = A:4           *** Rt Shift "sign-fills"
1 = Rt Rot    1 = A:1     1
2 = Lt Shift  2 = A:2
3 = Lt Rot    3 = A:3
              4 = B:4
              5 = B:1
              6 = B:2
              7 = B:3
 
----Misc----                    R == 0
P--           -Q-         --R
0 = Halt      0           0
1 = Halt      1
2 = NOP       2
3 = NOP       3
              4
              5
              6
              7 = don't care
 
----Jumps----                   Q >= 4
P--           -Q-         --R
0 = A
1 = B
2 = X
3 = Unc                   3 != 0
              4 = JPD     4 == 0
              5 = JPI     5 =< 0
              6 = JMD     6 >= 0
              7 = JMI     7 > 0
 
----Or, And, LNeg----          P == 3
P--           -Q-         --R
3             0 = Or
              1 = (NOP)
              2 = And
              3 - LNeg    3 = Const
                          4 = Mem
                          5 = Ind
                          6 = Idx
                          7 = Ind/Idx

----Add, Sub, Load, Store----
P--           -Q-         --R
0 = A         0 = Add        
1 = B         1 = Sub
2 = X         2 = Load
              3 = Store   3 = Const
                          4 = Mem
                          5 = Ind
                          6 = Idx
                          7 = Ind/Idx
*/

#define OP_MODE_CONST      3 // or Immediate (Store)
#define OP_MODE_MEM        4
#define OP_MODE_INDIRECT   5
#define OP_MODE_INDEXED    6
#define OP_MODE_INDIND     7 

#define OP_TEST_NE   3
#define OP_TEST_EQ   4
#define OP_TEST_LT   5
#define OP_TEST_GE   6
#define OP_TEST_GT   7

// define to revert to 0-fill right shift and incorrect rolls of more than 1 bit
//#define CPU_LEGACY_SHIFT_ROLL

// define to revert to PC updating during opcode evaluation (vs at the end)
//#define CPU_LEGACY_PROGRAM_COUNTER

CPU* CPU::cpu = NULL;

CPU::CPU(void)
{
  cpu = this;
}


void CPU::Init()
{
}

byte CPU::Read(byte Addr)
{
  // get the byte at Addr
  return m_Memory[Addr];
}

void CPU::Write(byte Addr, byte Value)
{
  // set the byte at Addr
  m_Memory[Addr] = Value;
}


byte CPU::GetBitField(byte Byte, int BitNum, int NumBits)
{
  // a helper -- get a bit field from the Byte
  return (Byte >> BitNum) & (0xFF >> (8 - NumBits));
}

byte* CPU::GetAddr(byte* pByte, byte Mode)
{
  // do the addressing modes
  switch (Mode)
  {
    case OP_MODE_MEM:      return m_Memory + (*pByte);
    case OP_MODE_INDIRECT: return m_Memory + m_Memory[*pByte];
    case OP_MODE_INDEXED:  return m_Memory + (byte)((*pByte) + m_Memory[REG_X_IDX]);
    case OP_MODE_INDIND:   return m_Memory + (byte)(m_Memory[*pByte] + m_Memory[REG_X_IDX]);
    default:               return pByte; // OP_MODE_CONST or other
  }
}

void CPU::ClearAllMemory()
{
  memset(m_Memory, 0, 256);
}

bool CPU::OnNOOPExtension(byte )
{
  // by default a NOOP does nothing (but says "keep running")
  // this can be over-written.  return false to HALT
  return true;
}

byte* CPU::Memory()
{
  // a pointer to the 256 bytes of memory
  return m_Memory;
}


byte* CPU::GetNextByte()
{
  // note: doesn't do anything special if we wrap around.  We could HALT, but don't
#ifdef CPU_LEGACY_PROGRAM_COUNTER  
  return m_Memory + m_Memory[REG_P_IDX]++;
#else  
  return m_Memory + m_Memory[REG_P_IDX] + m_InstructionBytes++;
#endif
}

bool CPU::Step()
{
  // one instruction, false means HALT
  m_InstructionBytes = 0;
  bool go = Execute(*GetNextByte());
  m_Memory[REG_P_IDX] += m_InstructionBytes;  // if enabled, advance the PC at the *end* of the instruction
  return go;
}

bool CPU::Execute(byte Instruction)
{
  // decode/execute Instruction, false means HALT, processes the next byte if required
  byte P__ = GetBitField(Instruction, 6, 2);
  byte _Q_ = GetBitField(Instruction, 3, 3);
  byte __R = GetBitField(Instruction, 0, 3);

  if (__R == 0)  // ==================== Miscellaneous (one byte only)
  {
    if (P__ == 0 || P__ == 1)  // HALT
      return false;
    else // NOOP
      if (_Q_ != 0)
        return OnNOOPExtension(Instruction);
  }
  else if (__R == 1)  // ==================== Shifts, Rotates (one byte only)
  {
    byte Places = _Q_ & 0x03;
    byte Rotate = P__ & 0x01;
    byte Left   = P__ & 0x02;
    byte* pValue = m_Memory + ((_Q_ & 0x04)?REG_B_IDX:REG_A_IDX);
    if (Places == 0) 
      Places = 4;

#ifndef CPU_LEGACY_SHIFT_ROLL
    if (Left) // left
    {
      for (int n = 0; n < Places; n++)
      {
        byte Rot = *pValue & 0x80;  // grab that bit
        *pValue <<= 1;              // shift
        if (Rotate && Rot)          // or-in the bit that rolled off
          *pValue |= 0x01;
      }
    }
    else  // right
    {
      for (int n = 0; n < Places; n++)
      {
        byte Rot = *pValue & 0x01;  // grab that bit
        byte Sgn = *pValue & 0x80;  // grab the "sign"
        *pValue >>= 1;              // shift
        if (Rotate && Rot)          // or-in the bit that rolled off
          *pValue |= 0x80;
        if (!Rotate && Sgn)
          *pValue |= 0x80;          // or-in the sign
      }
    }
#else
    // "Legacy"
    // rolls of more than 1 bit are incorrect.
    // right shift is 0-filled (KENBAK-1 wasn't)
    if (Left) // left
    {
      byte Rot = *pValue & 0x80;  // grab that bit
      *pValue <<= Places;         // shift
      if (Rotate && Rot)          // or-in the bit that rolled off
        *pValue |= 0x01;
    }
    else  // right
    {
      byte Rot = *pValue & 0x01;  // grab that bit
      *pValue >>= Places;         // shift
      if (Rotate && Rot)          // or-in the bit that rolled off
        *pValue |= 0x80;
    }
#endif
  }
  else if (__R == 2)  // ==================== Bit Test and Manipulation
  {
    byte Mask = 0x01 << _Q_;
    byte* Addr = GetAddr(GetNextByte(), OP_MODE_MEM);
    byte One = P__ & 0x01;
    if (P__ & 0x02) // SKIP
    {
      byte Skip;
      if (One)
        Skip = *Addr & Mask;
      else
        Skip = !(*Addr & Mask);
      if (Skip) // skip the next instruction (2 bytes)
#ifdef CPU_LEGACY_PROGRAM_COUNTER
        m_Memory[REG_P_IDX] += 2;
#else
        m_InstructionBytes += 2;
#endif
    }
    else  // SET
    {
      if (One)
        *Addr |= Mask;
      else
        *Addr &= ~Mask;
    }
  }
  else if (_Q_ > 3)    // ==================== jumps
  {
    byte JumpAndMark = _Q_ & 0x02;
    byte AddressMode = (_Q_ & 0x01) + OP_MODE_CONST;
    byte TestByte = m_Memory[P__];
    byte Condition = 0;
    byte TargetAddr = *GetAddr(GetNextByte(), AddressMode);

    if (P__ == 3)
      Condition = 1;
    else if (__R == OP_TEST_NE)
      Condition = TestByte;
    else if (__R == OP_TEST_EQ)
      Condition = !TestByte;
    else if (__R == OP_TEST_LT)
      Condition = TestByte & 0x80;
    else if (__R == OP_TEST_GE)
      Condition = !(TestByte & 0x80) || (TestByte == 0);
    else if (__R == OP_TEST_GT)
      Condition = !(TestByte & 0x80) && (TestByte != 0);

    if (Condition)
    {
      if (JumpAndMark)
      {
        m_Memory[TargetAddr] = m_Memory[REG_P_IDX] + m_InstructionBytes;
        TargetAddr++;
      }
      m_Memory[REG_P_IDX] = TargetAddr;
      m_InstructionBytes = 0;
    }
  }
  else if (P__ == 3)  // ==================== Or, And, Lneg, Noop
  {
    byte* regA = m_Memory + REG_A_IDX;
    byte* operand = GetAddr(GetNextByte(), __R);
    if (_Q_ == 0)  // OR
      *regA |= *operand;
    else if (_Q_ == 1) // (NOOP)
      return OnNOOPExtension(Instruction);
    else if (_Q_ == 2) // AND
      *regA &= *operand;
    else if (_Q_ == 3) // LNEG
    {
      signed char Temp = -(signed char)(*operand);
      *regA = (byte)Temp;
      // flags are not set
    }
  }
  else  // ==================== Add, Sub, Load, Store
  {
    byte* pLHS = m_Memory + P__;
    byte* pFlags = m_Memory + REG_FLAGS_A_IDX + P__;
    byte* pRHS = GetAddr(GetNextByte(), __R);
    word LHS = *pLHS;
    word RHS = *pRHS;
    word Result;
    if (_Q_ == 0) // ADD
    {
      Result = LHS + RHS;
      *pLHS = (byte)Result;
      *pFlags = 0;
      if (Result & 0xFF00)
        *pFlags |= 0x02; // carry

      signed short int Temp = (signed char)LHS + (signed char)RHS;
      if (Temp < -128 || Temp > 127)
        *pFlags |= 0x01; // overflow
    }
    else if (_Q_ == 1)  // SUB
    {
      Result = LHS - RHS;
      *pLHS = (byte)Result;
      *pFlags = 0;
      if (Result & 0xFF00)
        *pFlags |= 0x02; // carry (borrow)

      signed short int Temp = (signed char)LHS - (signed char)RHS;
      if (Temp < -128 || Temp > 127)
        *pFlags |= 0x01; // overflow
    }
    else if (_Q_ == 2)  // LOAD
      *pLHS = *pRHS; 
    else if (_Q_ == 3)  // STORE
      *pRHS = *pLHS;
  }
  return true;
}
