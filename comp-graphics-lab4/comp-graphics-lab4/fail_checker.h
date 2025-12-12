#ifndef FAIL_CHECKER_H
#define FAIL_CHECKER_H

#include "d3d12.h"
#include <exception>

inline void failCheck(HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception("DirectX error. Lol");
    }
}

#endif // FAIL_CHECKER_H