#include "BasicGamer2DApp.h"

int main(void) {
  // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try {
    BasicGamer2DApp{}.Run();
  } catch (DxException &e) {
    setlocale(LC_ALL, "");
    fwprintf(stderr, L"HR Failed: %ls", e.ToString().c_str());
    return 0;
  }
}