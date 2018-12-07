#ifndef mcp_h
#define mcp_h

#include "CPU.h"

// derived from the default KENBAK-1 cpu to over-ride NOOP
class ExtendedCPU:public CPU
{
public:
  virtual void Init();
  virtual bool OnNOOPExtension(byte Op);
};

// "master control program"
// handles the interaction between the front panel and the CPU
class MCP
{
public:
  enum tMode
  {
    eInput,
    eAddress,
    eMemory,
    eRun,
    
    eNone
  };

  void Init();
  void Splash();
  void Loop();
  
  void SetControlLEDs(byte LEDs);
  static bool NOOPExtensionCallback(void* This, byte Op);

private:
  void SetMode(byte Mode);
  void HandleButtonHalted(word State, word Pressed);
  void HandleButtonRunning(word State, word Pressed);
  void OnInputButton(int Btn, byte Chord);
  void OnInputClear(byte Chord);
  void OnAddressDisplay(byte Chord);
  void OnAddressSet(byte Chord);
  void OnMemoryRead(byte Chord);
  void OnMemoryStore(byte Chord);
  void OnRunStart(byte Chord);
  void OnRunStop(byte Chord);
  void Blink(byte LED);
  bool SystemCall(byte& A, byte& B);
  bool OnNOOPExtension(byte Op);
  
  bool m_bRunning;
  byte m_Data;
  byte m_Control;
  byte m_Mode;
  byte m_Address;
  
  friend class ExtendedCPU;
};

extern MCP mcp;
#endif
