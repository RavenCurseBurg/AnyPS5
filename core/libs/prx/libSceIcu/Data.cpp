#include "prx/libSceIcu/Data.hpp"
#include "unicode/udata.h"

#undef udata_setCommonData
#undef udata_setFileAccess

namespace Icu {

void SetCommonData(const void* data, std::int32_t* error) {
    if (!error) {
        udata_setCommonData_78(data, nullptr);
        return;
    }
    auto status = static_cast<UErrorCode>(*error);
    udata_setCommonData_78(data, &status);
    *error = status;
}

void SetFileAccess(std::int32_t access, std::int32_t*) {
    udata_setFileAccess_78(static_cast<UDataFileAccess>(access), nullptr);
}

}
