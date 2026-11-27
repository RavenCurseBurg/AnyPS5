#pragma once
#include <mutex>

namespace Icu {

inline std::mutex NativeLifecycleMutex;
void ClearCollators();
void ClearStringSearches();

}
