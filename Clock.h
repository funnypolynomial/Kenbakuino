#ifndef clock_h
#define clock_h

// interacts with the DS1307 RTC
class Clock
{
public:
  //  Configure RTC support
  // RTC    | ADDR | CTRL | SRAM
  // -------+------+------+-----
  // none   | 0x00 | N/A  | N/A
  // DS1307 | 0x68 | 0x07 | 0x08
  // DS3231 | 0x68 | 0x0E | 0x00
  // DS3232 | 0x68 | 0x0E | 0x14
  static const int RTC_I2C_ADDR         = 0x68;  // ADDR: I2C address, 0x00 means no RTC (no SRAM, use top of EEPROM)
  static const int RTC_CONTROL_OFFSET   = 0x07;  // CTRL: location of RTC control register, 0x00 means ignore
  static const int RTC_USER_SRAM_OFFSET = 0x08;  // SRAM: location of first byte of user SRAM in RTC, 0x00 means RTC has no SRAM, use top of EEPROM

  Clock();
  void Init();
  byte BCD2Dec(byte BCD);
  byte Dec2BCD(byte Dec);
  byte ReadByte(byte Index);
  void WriteByte(byte Index, byte Value);
private:
  byte ReadRTCByte(byte Index);
  void WriteRTCByte(byte Index, byte Value);
};

extern Clock clock;

#endif

