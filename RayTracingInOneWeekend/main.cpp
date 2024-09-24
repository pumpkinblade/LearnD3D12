#include "InOneWeekendApp.h"
#include <stb_image.h>

int main(void) {
  // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try {
    InOneWeekendApp{}.Run();
  } catch (DxException &e) {
    setlocale(LC_ALL, "");
    fwprintf(stderr, L"HR Failed: %ls", e.ToString().c_str());
    return 0;
  } catch (std::exception &e) {
    fprintf(stderr, "%d", e.what());
    return 0;
  }
}