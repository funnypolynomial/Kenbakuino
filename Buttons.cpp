#include <Arduino.h>
#include "PINS.h"
#include "Buttons.h"

byte Buttons::m_pMap[] = // eg logical eBit2 is actually bit6 from the 165
{
  0, 1, 6, 7, 4, 5, 2, 3,
  11, 12, 13, 14, 15, 9, 8, 10
};

void Buttons::Init()
{
  // prepare the pins which talk to the 165
  pinMode(PIN_BTN_PL, OUTPUT);
  pinMode(PIN_BTN_CP, OUTPUT); 
  pinMode(PIN_BTN_Q7, INPUT);
  // set no prev state
  m_wPrevState = 0xFFFF;
}

word Buttons::ShiftIn(int LatchPin, int DataPin, int ClockPin, int BitOrder)
{
  // read 16 bits of button statuses
  digitalWrite(ClockPin, HIGH);   // "Either the CP or the !CE should be HIGH before the LOW-to-HIGH transition of PL to prevent shifting the data when PL is activated."

  digitalWrite(LatchPin, LOW);  // latch the switch states
  digitalWrite(LatchPin, HIGH);  // shift data in when this is high and on a +ve going clock, HOWEVER, shiftIn uses High->Low?
  
  byte First  = shiftIn(DataPin, ClockPin, BitOrder);
  byte Second = shiftIn(DataPin, ClockPin, BitOrder);
  return word(Second, First);
}

bool Buttons::GetButtons(word& State, word& NewPressed, bool Wait)
{
  // get the current raw state and any that have changed to down
  word ThisState = ShiftIn(PIN_BTN_PL, PIN_BTN_Q7, PIN_BTN_CP, MSBFIRST);
  
  if (Wait)
  {
    // simple de-bounce, if requested
    delay(20);
    word DebouncedState = ShiftIn(PIN_BTN_PL, PIN_BTN_Q7, PIN_BTN_CP, MSBFIRST);
    if (DebouncedState != ThisState)
      return false;
  }
  
  if (ThisState != m_wPrevState)
  {
    NewPressed = ThisState & (~m_wPrevState);  // only those that have *changed* from OFF to ON, i.e. DOWN

    m_wPrevState = ThisState;
    State = ThisState;
    return true;
  }
  return false;
}

bool Buttons::IsPressed(word BtnState, int Btn)
{
  // is the Btn down (in the given state)
  return bitRead(BtnState, m_pMap[Btn]);
}

bool Buttons::GetButtonDown(word BtnState, int& Btn)
{
  // return the first Btn down in the state
  for (Btn = eBit0; Btn <= eUnused; Btn++)
  {
    if (IsPressed(BtnState, Btn))
      return true;
  }
  return false;
}


Buttons buttons = Buttons();
