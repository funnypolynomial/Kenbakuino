#ifndef config_h
#define config_h

// Configuration settings and also "SysInfo"

// auto-run flag bits:
#define AUTO_RUN_BUILTIN 0b00010000
#define AUTO_RUN_EEPROM  0b00100000
// copy EEPROM flags
#define COPY_EEPROM_PRESERVE  0b00010000  // preserve special locations
#define COPY_EEPROM_PAGE      0b00001000  // a page is 256 bytes not slots

class Config
{
public:
  enum tItems
  {
    eClockSeconds,
    eClockMinutes,
    eClockHours24,
    eClockDay,
    eClockDate,
    eClockMonth,
    eClockYear,
    eClockControl,
    
    // ------------
    eControlFlags,
    eControlEEPROMMap,
    eControlUser1,
    eControlUser2,
    eControlUser3,
    eControlUser4,
    eControlUser5,
    eControlAutoRun,  // was User6
    
    // ------------
    eControlLEDs,
    eControlRandom,
    eControlDelayMilliSec,
    eControlSerial,

    // -----------
    eEEPROMOffset,  // 024
    eRAMOffset,
    eEEPROMSize,
    eEEPROMOverlay,
    eEEPROMPage
  };
  
  Config();
  void Init();
  
  byte Read(byte Item, byte Value=0);
  bool Write(byte Item, byte Value);
  void SetCPUSpeed(byte Bit);
  
  // configuration settings
  bool m_bToggleBits;  // if true pressing a Bit button toggles the value, otherwise it only sets it
  byte m_iCycleDelayMilliseconds;      // delay each cpu "cycle"
  byte m_iEEPROMSlotMap;  // indicates halving of program slots in EEPROM, see Memory::BuildSlots()
  byte m_iAutoRunProgram;
  
private:
  void UpdateFlags(byte Value);
  void CheckStartupConfig();
  byte ReadFromEEPROM(bool Read, byte EEPROMPage);
  byte m_EEPROMOffset;
  byte m_RAMOffset;
  int m_EEPROMSize;
};

extern Config config;

#endif

