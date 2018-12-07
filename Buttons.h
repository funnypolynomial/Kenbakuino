#ifndef buttons_h
#define buttons_h

// buttons/switches
// Interacts with the 15 (8 data, 7 control) push-buttons via
// daisy-chained 74HC165's
class Buttons
{
public:
  enum tButtons
  {
    eBit0,
    eBit1,
    eBit2,
    eBit3,
    eBit4,
    eBit5,
    eBit6,
    eBit7,

    eInputClear,
    eAddressDisplay,
    eAddressSet,
    eMemoryRead,
    eMemoryStore,
    eRunStart,
    eRunStop,
    eUnused
  };

  void Init();

  bool GetButtons(word& State, word& NewPressed, bool Wait);
  bool IsPressed(word BtnState, int Btn);
  bool GetButtonDown(word BtnState, int& Btn);

private:
  word ShiftIn(int LatchPin, int DataPin, int ClockPin, int BitOrder);
  static byte m_pMap[];
  word m_wPrevState;
};

extern Buttons buttons;
#endif

