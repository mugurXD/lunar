#include <version>

#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
#  include <functional>
namespace lunar { using std::move_only_function; }
#else
#  include <std23/move_only_function.h>
namespace lunar { using std23::move_only_function; }
#endif
