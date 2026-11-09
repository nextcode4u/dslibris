// SPDX-License-Identifier: MIT
#pragma once
#include <nds.h>

static inline bool reader_main_loop()
{
#ifdef __BLOCKSDS__
    // BlocksDS handles power/sleep services on its ARM7 core.
    return true;
#else
    return pmMainLoop();
#endif
}
