#ifndef programs_h
#define programs_h

// sample programs
class Programs
{
public:
  static void AssembleSetRTC(byte* pMem);
  static void AssembleCountClock(byte* pMem);
  static void AssembleBCDClock(byte* pMem);
  static void AssembleBinClock(byte* pMem);
  static void AssembleDBL(byte* pMem);
  static void AssembleSieve(byte* pMem);

private:
  static byte* m_pMemory;
  static byte m_iIndex;
  static byte m_pLabels[];
  static byte m_iPass;
};

#endif
