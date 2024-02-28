#include <iostream>
#include "TestUnit.h"

int main(void)
{
  // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try
  {
    TestUnit unit;
    if (!unit.Initialize())
      return 0;
    return unit.Run();
  }
  catch (DxException& e)
  {
    MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
    return 0;
  }
  return 0;
}