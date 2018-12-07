#ifndef leds_h
#define leds_h

// control the 12 LEDs via a 74HC595 and direct control

class LEDs
{
public:
  void Init();
  void Display(byte Data, byte Control);

private:
  void ShiftOut(byte LEDs);

  static byte m_pDirectControlPins[];
  byte m_LastData;
  byte m_LastControl;
};

extern LEDs leds;
#endif
