#include <os_typedefs.h>
#include <os_assert.h>
#include <os_log.h>
#include <core_scoped_ptr.h>
#include <OMX_Core.h>

using namespace os;
using namespace core;
int32_t main(int argc, char *argv[]) {
  OMX_ERRORTYPE oRet = OMX_ErrorNone;
  oRet = OMX_Init();
  CHECK_EQ(oRet, OMX_ErrorNone);
  scoped_array<char> comp_name(new char [128]);
  for (int32_t i = 0; ; ++i) {
    oRet = OMX_ComponentNameEnum((OMX_STRING)comp_name.get(), 128, i);
    if (OMX_ErrorNoMore == oRet)
      break;
    CHECK_EQ(oRet, OMX_ErrorNone);

    log_verbose("tag", "Component[%d]: %s \n", i + 1, comp_name.get());
  }

  oRet = OMX_Deinit();
  CHECK_EQ(oRet, OMX_ErrorNone);
  return 0;
}
