/*
 *  Copyright (c) 2009-2011, NVIDIA Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of NVIDIA Corporation nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Adapted for Aalto CS-C3100 Computer Graphics from source code with the copyright above.

#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>             // Window manager, includes timer

class Timer
{
public:
    explicit inline     Timer           (bool started = false)  : m_startTicks((started) ? queryTicks() : -1), m_totalTicks(0) {}
    inline              Timer           (const Timer& other)    { operator=(other); }
    inline              ~Timer          (void)                  {}

    inline void         start           (void)                  { m_startTicks = queryTicks(); }
    inline void         unstart         (void)                  { m_startTicks = -1; }
    inline float        getElapsed      (void)                  { return ticksToSecs(getElapsedTicks()); }

    inline float        end             (void);                 // return elapsed, total += elapsed, restart
    inline float        getTotal        (void) const            { return ticksToSecs(m_totalTicks); }
    inline void         clearTotal      (void)                  { m_totalTicks = 0; }

    inline Timer&       operator=       (const Timer& other);

    static void         staticInit      (void);
    static inline int   queryTicks      (void);
    static inline float ticksToSecs     (int ticks);

private:
    inline int          getElapsedTicks (void);                 // return time since start, start if unstarted

private:
    static double       s_ticksToSecsCoef;
    static uint64_t     s_prevTicks;

    int                 m_startTicks;
    int                 m_totalTicks;
};

//------------------------------------------------------------------------

float Timer::end(void)
{
    int elapsed = getElapsedTicks();
    m_startTicks += elapsed;
    m_totalTicks += elapsed;
    return ticksToSecs(elapsed);
}

//------------------------------------------------------------------------

Timer& Timer::operator=(const Timer& other)
{
    m_startTicks = other.m_startTicks;
    m_totalTicks = other.m_totalTicks;
    return *this;
}

//------------------------------------------------------------------------

int Timer::queryTicks(void)
{
    return glfwGetTimerValue();
    //LARGE_INTEGER ticks;
    //QueryPerformanceCounter(&ticks);
    //ticks.QuadPart = max(s_prevTicks, ticks.QuadPart);
    //s_prevTicks = ticks.QuadPart; // increasing little endian => thread-safe
    //return ticks.QuadPart;
}

//------------------------------------------------------------------------

float Timer::ticksToSecs(int ticks)
{
    if (s_ticksToSecsCoef == -1.0)
        staticInit();
    return (float)((double)ticks * s_ticksToSecsCoef);
}

//------------------------------------------------------------------------

int Timer::getElapsedTicks(void)
{
    int curr = queryTicks();
    if (m_startTicks == -1)
        m_startTicks = curr;
    return curr - m_startTicks;
}

//------------------------------------------------------------------------
