#pragma once

#include "LoopScheduler.dec.h"

namespace LoopScheduler
{
    class Loop final
    {
    public:
        Loop(Group Architecture);
        ~Loop();

        void Start();
        void Stop();
    };
}
