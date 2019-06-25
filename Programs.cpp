#include <Arduino.h>
#include "Config.h"
#include "Cpu.h"
#include "Programs.h"

// A really simple two-pass "assembler"

#define pc(_a)      m_iIndex=_a;
#define op2(_a, _b) m_pMemory[m_iIndex++]=_a; m_pMemory[m_iIndex++]=_b;
#define op1(_a)     m_pMemory[m_iIndex++]=_a;
#define equ(_a)     m_pMemory[m_iIndex++]=_a;
#define lbl(_l)     m_iPass?m_pLabels[_l]:0
#define def(_l)     m_pLabels[_l] = m_iIndex;

#define SYSX_WRITE(_a) (0x80 | (_a))
#define SYSX_READ(_a)  (_a)
#define SYSX 0360
#define HALT 0000
  // c - const
#define LDAc 0023
#define LDBc 0123
#define LDXc 0223
#define SUAc 0013
#define SUBc 0113
#define SUXc 0213
#define ADAc 0003
#define ADBc 0103
#define ADXc 0203
#define ANDc 0323

  // a - addr
#define STAa 0034
#define STBa 0134
#define STXa 0234
#define LDAa 0024
#define LDBa 0124
#define LDXa 0224
#define ADAa 0004
#define ADBa 0104
#define ADXa 0204
#define SUAa 0014
// x - indexed
#define LDAx 0026

// b - bit ops
#define SETb(_bit) (0002 | (_bit << 3))
#define CLRb(_bit) (0102 | (_bit << 3))
#define SKCb(_bit) (0202 | (_bit << 3))  // skip if bit clear
#define SKSb(_bit) (0302 | (_bit << 3))  // skip if bit set

#define JMPu 0343
#define RETa 0353
#define JSRa 0363

byte* Programs::m_pMemory;
byte  Programs::m_iIndex;
byte  Programs::m_pLabels[32];
byte  Programs::m_iPass;

void Programs::AssembleSetRTC(byte* pMem)
{
  m_pMemory = pMem;
  
  enum { SetTime, Hr, Min };
  for (m_iPass = 0; m_iPass < 2; m_iPass++)
  {
    // 0 is hr, 1 is min (BCD)
    pc(2)
      equ(0000)
      equ(0004)
    def(SetTime)
      op2(STAa, lbl(Hr))  // copy out of "registers"
      op2(STBa, lbl(Min))
      // set hr
      op2(LDAc, 128 + Config::eClockHours24)
      op2(LDBa, lbl(Hr))
      op1(SYSX)
      // set min
      op2(LDAc, 128 + Config::eClockMinutes)
      op2(LDBa, lbl(Min))
      op1(SYSX)
      // set secs
      op2(LDAc, 128 + Config::eClockSeconds)
      op2(LDBc, 0)
      op1(SYSX)
      op1(HALT) // done, but ...
      op2(JMPu, lbl(SetTime)) // ... be ready to run again
    def(Hr)
      equ(0)
    def(Min)
      equ(0)
  }
}
void Programs::AssembleCountClock(byte* pMem)
{
  enum { ShowHr, Display, Blink, HrTab, DoBlink, BlinkLoop, MinLoop, MinTab10s, MinTab5s, SaveBlink, EvenX, BCD2Dec, BCDfix, BCDloop, BCDdone };

  m_pMemory = pMem;
  
  for (m_iPass = 0; m_iPass < 2; m_iPass++)
  {
    pc(0)
      equ(0000)
      equ(0000)
      equ(0000)
      equ(0004)
      // init
      op2(LDAc, 0xFF)
      op1(SYSX)                       // SYS got extensions?
      op2(0272, REG_A_IDX)            // skip next two if A has b7 not set
      op1(0300)                       // noop
      op1(0000)     	              // halt, no extensions
      op2(LDAc, 128 + Config::eControlLEDs)
      op2(LDBc, 0000)	              // LDB #0
      op1(SYSX)	                      // SYS clear control LEDs
    def(ShowHr)
      op2(LDAc, Config::eClockHours24)
      op1(SYSX)	                      // SYS get hr-24 BCD 
      op2(0363, lbl(BCD2Dec))
      op2(STBa, lbl(Display))         // B has hour
      op2(LDAa, lbl(Display))         // A has hour
      op2(SUAc, 12)                   // A -=12
      op2(0272, REG_A_IDX)            // skip if b7 clear
      op2(LDAa, lbl(Display))         // reload A (hr)
      op2(STAa, lbl(Display))         // save A (hr)
      op2(LDXa, lbl(Display))         // X has hour
      op2(LDAx, lbl(HrTab))           // look up A
      op2(STAa, lbl(Display))         // save A (hr)
      op2(LDAc, 0000)	              // LDA #0 (no blink)
      op2(STAa, lbl(Blink))           // save A (blink)
      op2(0363, lbl(DoBlink))

      op2(LDAc, Config::eClockMinutes)
      op1(SYSX)	                      // SYS get min BCD
      op2(0363, lbl(BCD2Dec))
      op2(STBa, lbl(Display))         // B has mims
      op2(LDXc, 255)                  // X counts 5's
    def(MinLoop)
      op2(ADXc, 1)
      op2(SUBc, 5)                    // B -= 5
      op2(0372, REG_B_IDX)            // skip if b7 SET
      op2(JMPu, lbl(MinLoop))
      op2(0302, REG_X_IDX)            // skip if b0 SET (odd)
      op2(JMPu, lbl(EvenX))
      op2(LDAx, lbl(MinTab5s))        // look up A
      op2(STAa, lbl(Display))         // save A (hr)
      op2(LDAx, lbl(MinTab10s))       // look up A
      op2(ADAc, 0200)	              // LDA #128 (blink)
      op2(JMPu, lbl(SaveBlink))
    def(EvenX)
      op2(LDAx, lbl(MinTab10s))       // look up A
      op2(STAa, lbl(Display))         // save A (hr)
      op2(LDAc, 0200)	              // LDA #128 (blink)
    def(SaveBlink)
      op2(STAa, lbl(Blink))           // save A (blink)
      op2(0363, lbl(DoBlink))
      op2(JMPu, lbl(ShowHr))

    def(DoBlink)
      op1(0300)                       // NOOP: space for the return addr
      op2(LDXc, 5)                    // LDX with counter
      op2(LDAa, lbl(Display))         // load (display)
    def(BlinkLoop)
      op2(STAa, REG_OUTPUT_IDX)       // Show it
      op2(LDAc, 128 + Config::eControlDelayMilliSec)
      op2(LDBc, 250)	              // LDB #250
      op1(SYSX)	                      // SYS sleep 250ms
      op1(SYSX)	                      // SYS sleep 250ms
      op2(LDAa, REG_OUTPUT_IDX)       // reload A
      op2(ADAa, lbl(Blink))           // add blink mask
      op2(STAa, REG_OUTPUT_IDX)       // Show it
      op2(LDAc, 128 + Config::eControlDelayMilliSec)
      op2(LDBc, 250)	              // LDB #250
      op1(SYSX)	                      // SYS sleep 250ms
      op1(SYSX)	                      // SYS sleep 250ms
      op2(LDAa, REG_OUTPUT_IDX)       // reload A
      op2(SUAa, lbl(Blink))           // subtract blink mask
      op2(SUXc, 1)                    // dec blink ctr
      op2(0254, lbl(DoBlink))         // if 0 return 
      op2(JMPu, lbl(BlinkLoop))
      
    pc(0204)  // skip over the "flag" bytes
    
    def(BCD2Dec) // B has BCD, B returned in Dec
      op1(0300)                       // NOOP: space for the return addr
      op2(LDAa, 1)
      op2(LDBc, 0)
      op2(0045, lbl(BCDfix))   // A<0
    def(BCDloop)
      op2(SUAc, 0020)
      op2(0045, lbl(BCDdone))   // A<0
      op2(ADBc, 0012)
      op2(JMPu, lbl(BCDloop))
    def(BCDfix)
      op2(SUAc, 0200)
      op2(ADBc, 0120)
      op2(JMPu, lbl(BCDloop))
    def(BCDdone)
      op2(ADAc, 0020)
      op2(ADBa, 0000)
      op2(0353, lbl(BCD2Dec)) // return
      
    def(Display)
      equ(0000)
    def(Blink)
      equ(0000)
    def(HrTab)
      equ(0277) // 12

      equ(0001) // 1
      equ(0003)  
      equ(0007)
      equ(0017)
      equ(0037)
      equ(0077)

      equ(0201) // 7
      equ(0203)
      equ(0207)
      equ(0217)
      equ(0237) // 11

    def(MinTab5s)  // offset to 5min entries, LED to blink
      equ(0000)
    def(MinTab10s) // offset to 10min entries, solid LEDs
      equ(0000) // 0
      equ(0001) // 5
      equ(0001) // 10
      equ(0002) // 15
      equ(0003) // 20
      equ(0004) // 25
      equ(0007) // 30
      equ(0010) // 35
      equ(0017) // 40
      equ(0020) // 45
      equ(0037) // 50
      equ(0040) // 55
 }
}

void Programs::AssembleBCDClock(byte* pMem)
{
  enum { ShowHr, ScrollLeft, Left, Right, Display, Mins, Hours, Blink, HrBCDTab, DoBlink, BlinkLoop, BCD2Dec, BCDfix, BCDloop, BCDdone };

  m_pMemory = pMem;
  
  for (m_iPass = 0; m_iPass < 2; m_iPass++)
  {
    pc(0)
      equ(0000)
      equ(0000)
      equ(0000)
      equ(0004)
      // init
      op2(LDAc, 0xFF)
      op1(SYSX)                       // SYS got extensions?
      op2(0272, REG_A_IDX)            // skip next two if A has b7 not set
      op1(0300)                       // noop
      op1(0000)     	              // halt, no extensions
      op2(LDAc, 128 + Config::eControlLEDs)
      op2(LDBc, 0000)	              // LDB #0
      op1(SYSX)	                      // SYS clear control LEDs
    def(ShowHr)
      op2(LDAc, Config::eClockHours24)
      op1(SYSX)	                      // SYS get hr-24 BCD 
      op2(0363, lbl(BCD2Dec))
      op2(STBa, lbl(Display))         // B has hour
      op2(LDAa, lbl(Display))         // A has hour
      op2(SUAc, 12)                   // A -=12
      op2(0272, REG_A_IDX)            // skip if b7 clear
      op2(LDAa, lbl(Display))         // reload A (hr)
      op2(STAa, lbl(Display))         // save A (hr)
      op2(LDXa, lbl(Display))         // X has hour
      op2(LDAx, lbl(HrBCDTab))        // look up A as BCD
      op2(STAa, lbl(Display))         // save A (hr)
      op2(STAa, lbl(Hours))
      op2(LDAc, 0000)	                // LDA #0 (no blink)
      op2(STAa, lbl(Blink))           // save A (blink)
      op2(0363, lbl(DoBlink))

      op2(LDAc, Config::eClockMinutes)
      op1(SYSX)	                      // SYS get min BCD

      op2(0363, lbl(ScrollLeft))

      op2(LDAc, 0200)	                // LDA #128 (blink)
      op2(STAa, lbl(Blink))           // save A (blink)
      op2(0363, lbl(DoBlink))

      op2(LDXc, 8)
    def(Right)
      // scroll right
      op2(LDAa, lbl(Display))
      op1(0011) //right shift A 1
      op2(STAa, lbl(Display))
      op2(LDAa, lbl(Hours))
      op1(0111) //right roll A 1
      op2(STAa, lbl(Hours))
      op2(ANDc, 0100)
      op2(ADAa, lbl(Display))
      op2(STAa, lbl(Display))
      op2(STAa, REG_OUTPUT_IDX)
      op2(LDAc, 128 + Config::eControlDelayMilliSec)
      op2(LDBc, 100)
      op1(SYSX)	   // SYS sleep
      op2(SUXc, 1)
      op2(0243, lbl(Right))

      op2(JMPu, lbl(ShowHr))

    def(DoBlink)
      op1(0300)                       // NOOP: space for the return addr
      op2(LDXc, 5)                    // LDX with counter
      op2(LDAa, lbl(Display))         // load (display)
    def(BlinkLoop)
      op2(STAa, REG_OUTPUT_IDX)       // Show it
      op2(LDAc, 128 + Config::eControlDelayMilliSec)
      op2(LDBc, 250)	              // LDB #250
      op1(SYSX)	                      // SYS sleep 250ms
      op1(SYSX)	                      // SYS sleep 250ms
      op2(LDAa, REG_OUTPUT_IDX)       // reload A
      op2(ADAa, lbl(Blink))           // add blink mask
      op2(STAa, REG_OUTPUT_IDX)       // Show it
      op2(LDAc, 128 + Config::eControlDelayMilliSec)
      op2(LDBc, 250)	              // LDB #250
      op1(SYSX)	                      // SYS sleep 250ms
      op1(SYSX)	                      // SYS sleep 250ms
      op2(LDAa, REG_OUTPUT_IDX)       // reload A
      op2(SUAa, lbl(Blink))           // subtract blink mask
      op2(SUXc, 1)                    // dec blink ctr
      op2(0254, lbl(DoBlink))         // if 0 return 
      op2(JMPu, lbl(BlinkLoop))

    pc(0204)  // skip over the "flag" bytes

    def(ScrollLeft)
      op1(0300)                       // NOOP: space for the return addr
      op2(STBa, lbl(Mins))         // B has mims
      op2(LDXc, 8)
    def(Left)
      // scroll left
      op2(LDAa, lbl(Display))
      op1(0211) //left shift A 1
      op2(STAa, lbl(Display))
      op2(LDAa, lbl(Mins))
      op1(0311) //left roll A 1
      op2(STAa, lbl(Mins))
      op2(ANDc, 0001)
      op2(ADAa, lbl(Display))
      op2(STAa, lbl(Display))
      op2(STAa, REG_OUTPUT_IDX)
      op2(LDAc, 128 + Config::eControlDelayMilliSec)
      op2(LDBc, 100)
      op1(SYSX)	   // SYS sleep
      op2(SUXc, 1)
      op2(0243, lbl(Left))
      op2(0254, lbl(ScrollLeft)) // return to caller

    def(BCD2Dec) // B has BCD, B returned in Dec
      op1(0300)                       // NOOP: space for the return addr
      op2(LDAa, 1)
      op2(LDBc, 0)
      op2(0045, lbl(BCDfix))   // A<0
    def(BCDloop)
      op2(SUAc, 0020)
      op2(0045, lbl(BCDdone))   // A<0
      op2(ADBc, 0012)
      op2(JMPu, lbl(BCDloop))
    def(BCDfix)
      op2(SUAc, 0200)
      op2(ADBc, 0120)
      op2(JMPu, lbl(BCDloop))
    def(BCDdone)
      op2(ADAc, 0020)
      op2(ADBa, 0000)
      op2(0353, lbl(BCD2Dec)) // return
      
    def(Display)
      equ(0000)
    def(Mins)
      equ(0000)
    def(Hours)
      equ(0000)
    def(Blink)
      equ(0000)
    def(HrBCDTab)
      equ(0x12) // 12

      equ(0x01) // 1
      equ(0x02)  
      equ(0x03)
      equ(0x04)
      equ(0x05)
      equ(0x06)

      equ(0x07) // 7
      equ(0x08)
      equ(0x09)
      equ(0x10)
      equ(0x11) // 11
  }  
}

void Programs::AssembleBinClock(byte* pMem)
{
  enum { ShowHr, Display, Blink, HrBinTab, DoBlink, BlinkLoop, BCD2Dec, BCDfix, BCDloop, BCDdone };

  m_pMemory = pMem;
  
  for (m_iPass = 0; m_iPass < 2; m_iPass++)
  {
    pc(0)
      equ(0000)
      equ(0000)
      equ(0000)
      equ(0004)
      // init
      op2(LDAc, 0xFF)
      op1(SYSX)                       // SYS got extensions?
      op2(0272, REG_A_IDX)            // skip next two if A has b7 not set
      op1(0300)                       // noop
      op1(0000)     	              // halt, no extensions
      op2(LDAc, 128 + Config::eControlLEDs)
      op2(LDBc, 0000)	              // LDB #0
      op1(SYSX)	                      // SYS clear control LEDs
    def(ShowHr)
      op2(LDAc, Config::eClockHours24)
      op1(SYSX)	                      // SYS get hr-24 BCD 
      op2(0363, lbl(BCD2Dec))
      op2(STBa, lbl(Display))         // B has hour
      op2(LDAa, lbl(Display))         // A has hour
      op2(SUAc, 12)                   // A -=12
      op2(0272, REG_A_IDX)            // skip if b7 clear
      op2(LDAa, lbl(Display))         // reload A (hr)
      op2(STAa, lbl(Display))         // save A (hr)
      op2(LDXa, lbl(Display))         // X has hour
      op2(LDAx, lbl(HrBinTab))        // look up A as binary
      op2(STAa, REG_B_IDX)
      op2(LDAc, 128 + Config::eControlLEDs)
      op1(SYSX)                       // write the hour to the 4 ctrl LEDs

      op2(LDAc, Config::eClockMinutes)
      op1(SYSX)	                      // SYS get min BCD
      op2(0363, lbl(BCD2Dec))
      op2(STBa, lbl(Display))         // B has minutes

      op2(LDAc, 0200)	                // LDA #128 (blink)
      op2(STAa, lbl(Blink))           // save A (blink)
      op2(0363, lbl(DoBlink))

      op2(JMPu, lbl(ShowHr))

    def(DoBlink)
      op1(0300)                       // NOOP: space for the return addr
      op2(LDXc, 8)                    // LDX with counter
      op2(LDAa, lbl(Display))         // load (display)
    def(BlinkLoop)
      op2(STAa, REG_OUTPUT_IDX)       // Show it
      op2(LDAc, 128 + Config::eControlDelayMilliSec)
      op2(LDBc, 250)	              // LDB #250
      op1(SYSX)	                      // SYS sleep 250ms
      op1(SYSX)	                      // SYS sleep 250ms
      op2(LDAa, REG_OUTPUT_IDX)       // reload A
      op2(ADAa, lbl(Blink))           // add blink mask
      op2(STAa, REG_OUTPUT_IDX)       // Show it
      op2(LDAc, 128 + Config::eControlDelayMilliSec)
      op2(LDBc, 250)	              // LDB #250
      op1(SYSX)	                      // SYS sleep 250ms
      op1(SYSX)	                      // SYS sleep 250ms
      op2(LDAa, REG_OUTPUT_IDX)       // reload A
      op2(SUAa, lbl(Blink))           // subtract blink mask
      op2(SUXc, 1)                    // dec blink ctr
      op2(0254, lbl(DoBlink))         // if 0 return 
      op2(JMPu, lbl(BlinkLoop))

    pc(0204)  // skip over the "flag" bytes

    def(BCD2Dec) // B has BCD, B returned in Dec
      op1(0300)                       // NOOP: space for the return addr
      op2(LDAa, 1)
      op2(LDBc, 0)
      op2(0045, lbl(BCDfix))   // A<0
    def(BCDloop)
      op2(SUAc, 0020)
      op2(0045, lbl(BCDdone))   // A<0
      op2(ADBc, 0012)
      op2(JMPu, lbl(BCDloop))
    def(BCDfix)
      op2(SUAc, 0200)
      op2(ADBc, 0120)
      op2(JMPu, lbl(BCDloop))
    def(BCDdone)
      op2(ADAc, 0020)
      op2(ADBa, 0000)
      op2(0353, lbl(BCD2Dec)) // return
      
    def(Display)
      equ(0000)
    def(Blink)
      equ(0000)
    def(HrBinTab) 
      // bits are reverse order, b0 is the left-most ctrl LED, value of 8
      equ(3) // 12

      equ(8) // 1
      equ(4)  
      equ(12)
      equ(2)
      equ(10)
      equ(6)

      equ(14) // 7
      equ(1)
      equ(9)
      equ(5)
      equ(13) // 11
  }
}


void Programs::AssembleDBL(byte* pMem)
{
  // Das Blinken Lights
  enum { Loop, Temp };

  m_pMemory = pMem; 
  
  byte Delay = 20;
  for (m_iPass = 0; m_iPass < 2; m_iPass++)
  {
    pc(0)
      equ(0000)
      equ(0000)
      equ(0000)
      equ(0004)
      op2(LDAc, 128 + Config::eControlLEDs)
      op2(LDBc, 0000)	              // LDB #0
      op1(SYSX)	                      // SYS clear control LEDs
      op2(LDAc, 128 + Config::eControlRandom)
      op2(LDBc, 0)
      op1(SYSX)  // randomize
    def(Loop)
      op2(LDAc, Config::eControlRandom)
      op1(SYSX)  // random
      op2(STBa, REG_X_IDX)
      op2(LDAc, Config::eControlRandom)
      op1(SYSX)  // random
      
      // mask off the PWM
      op2(STBa, Temp)
      op2(STBa, REG_A_IDX)
      op2(0323, 0x0F)  // AND
      op2(STAa, REG_B_IDX)
      op2(LDAc, 128 + Config::eControlLEDs)
      op1(SYSX)  // ctrl LEDs
      op2(LDAc, 128 + Config::eControlDelayMilliSec)
      op2(LDBc, Delay)
      op1(SYSX)	                      // SYS sleep
      op2(LDAa, Temp)
      op1(0001)  // shift A right 4
      op2(STAa, REG_B_IDX)
      op2(LDAc, 128 + Config::eControlLEDs)
      op1(SYSX)  // ctrl LEDs
      
      op2(STXa, REG_OUTPUT_IDX)
      op2(LDAc, 128 + Config::eControlDelayMilliSec)
      op2(LDBc, Delay*2)
      op1(SYSX)	                      // SYS sleep
      op2(JMPu, lbl(Loop))
    def(Temp)
      equ(0)
  }
}

#if 1
// doing this frees up about 4k
const byte programSieve[] PROGMEM = 
{
  0000,0000,0000,0004,0034,0200,0223,0200,0023,0002,0363,0111,0203,0001,0023,0003,
  0363,0111,0003,0002,0203,0001,0243,0020,0223,0200,0203,0001,0363,0140,0044,0066,
  0234,0171,0363,0140,0034,0167,0204,0000,0212,0203,0343,0064,0023,0000,0363,0111,
  0204,0167,0343,0050,0224,0171,0203,0001,0243,0034,0223,0200,0363,0140,0044,0103,
  0034,0200,0000,0203,0001,0243,0074,0343,0072,0300,0234,0170,0234,0001,0113,0204,
  0146,0132,0213,0004,0036,0000,0224,0170,0353,0111,0036,0000,0224,0170,0353,0111,
  0300,0234,0170,0234,0001,0113,0204,0146,0161,0213,0004,0026,0000,0224,0170,0353,
  0140,0026,0000,0224,0170,0353,0140,0000
};

void Programs::AssembleSieve(byte* pMem)
{
    memcpy_P(pMem, programSieve, 120);
}
#else
void Programs::AssembleSieve(byte* pMem)
{
  // Sieve of Eratosthenes, primes up to 255
  
  enum { Init, Sieve, Sieve1, Sieve2, Dump, List, List1, Zero, TempA, TempX, PrevX, vWrite, vWriteGE, vRead, vReadGE, 
    Table = 0200 // virtual start address, chosen so we seed the list up to 255 (and don't wrap)
  };
  m_pMemory = pMem; 
  for (m_iPass = 0; m_iPass < 2; m_iPass++)
  {
    pc(0)
      equ(0000)
      equ(0000)
      equ(0000)
      equ(0004)
      op2(STAa, REG_OUTPUT_IDX)
      // build a list of 2, followed by ODD numbers to 255 and apply the sieve
      // the list overlays the 0200 (OUTPUT) and flags bytes (we don't have enough contiguous memory)
      // so use "virtual" read and write which adjust the X appropriately
      op2(LDXc, Table)
      op2(LDAc, 2)
      op2(JSRa, lbl(vWrite))
      
      op2(ADXc, 1)
      op2(LDAc, 3)
    def(Init)
      op2(JSRa, lbl(vWrite))
      op2(ADAc, 2)
      op2(ADXc, 1)
      op2(0243, lbl(Init))

      op2(LDXc, Table)
      op2(ADXc, 1)
    def(Sieve)
      op2(JSRa, lbl(vRead))
      op2(0044, lbl(Zero))  // slot is zero
      // do a sweep
      op2(STXa, lbl(PrevX))
      op2(JSRa, lbl(vRead))
      op2(STAa, lbl(TempA))
      op2(ADXa, REG_A_IDX)
    def(Sieve1)
      op2(0212, REG_FLAGS_X_IDX)  // skip if (Xcarry clear)
      op2(JMPu, lbl(Sieve2)) // if carry
      op2(LDAc, 0)
      op2(JSRa, lbl(vWrite))
      op2(ADXa, lbl(TempA))

      op2(JMPu, lbl(Sieve1))
    def(Sieve2)
      op2(LDXa, lbl(PrevX))
    def(Zero)
      op2(ADXc, 1)
      op2(0243, lbl(Sieve))

    def(Dump)
      op2(LDXc, Table)
    def(List)
      op2(JSRa, lbl(vRead))
      op2(0044, lbl(List1)) //if (A==0) List1
      op2(STAa, REG_OUTPUT_IDX)
      op1(HALT) // WAIT
    def(List1)
      op2(ADXc, 1)
      op2(0243, lbl(List))
      op2(JMPu, lbl(Dump))

    def(vWrite)   // virtual write Table[X] = A
      op1(0300)   // NOOP: space for the return addr
      op2(STXa, lbl(TempX))
      op2(STXa, REG_B_IDX)
      op2(SUBc, 0204) // B = B - 0204
      op2(0146, lbl(vWriteGE)) // B >=0
      op2(SUXc, 4)
      op2(0036, 0)  // STA, X
      op2(LDXa, lbl(TempX))
      op2(RETa, lbl(vWrite))   
    def(vWriteGE)
      op2(0036, 0)
      op2(LDXa, lbl(TempX))
      op2(RETa, lbl(vWrite))   

    def(vRead)   // virtual read A = Table[X]
      op1(0300)   // NOOP: space for the return addr
      op2(STXa, lbl(TempX))
      op2(STXa, REG_B_IDX)
      op2(SUBc, 0204)
      op2(0146, lbl(vReadGE))
      op2(SUXc, 4)
      op2(0026, 0)
      op2(LDXa, lbl(TempX))
      op2(RETa, lbl(vRead))   
    def(vReadGE)
      op2(0026, 0)
      op2(LDXa, lbl(TempX))
      op2(RETa, lbl(vRead))   

    def(TempA)
      equ(0000)
    def(TempX)
      equ(0000)
    def(PrevX)
      equ(0000)
  }
}
#endif

