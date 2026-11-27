#pragma once
#include <mutex>

namespace Icu {

inline std::mutex NativeLifecycleMutex;
void ClearStringSearches();

}
