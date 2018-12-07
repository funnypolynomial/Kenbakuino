#ifndef clock_h
#define clock_h

// interacts with the DS1307 RTC
class Clock
{
public:
  Clock();
  void Init();
  byte BCD2Dec(byte BCD);
  byte Dec2BCD(byte Dec);
  byte ReadByte(byte Index);
  void WriteByte(byte Index, byte Value);
private:
  int m_iAddress;
};

extern Clock clock;

#endif
