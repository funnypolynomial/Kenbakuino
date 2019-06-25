#ifndef memory_h
#define memory_h

// handle EEPROM and PROGMEM
class Memory
{
public:
  void Init();
  void BuildSlots(byte Map);
  bool LoadStandardProgram(byte Index);
  bool ReadMemoryFromEEPROMSlot(byte Slot);
  bool WriteMemoryToEEPROMSlot(byte Slot);
  int GetEEPROMTopIdx();
  int SlotStartAddr(byte Slot);
  int SlotSize(byte Slot);
  
private:
  int m_pSlotStartAddr[8];
  int m_pSlotSize[8];
};

extern Memory memory;
#endif

