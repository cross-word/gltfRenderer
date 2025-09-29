#include "stdafx.h"
#include "MiniTimer.h"
#include <windows.h>

MiniTimer::MiniTimer()
{
    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&StartingTime);
    
    LastFrameTime = StartingTime;
    FrameCount = 0;
    CurrentFPS = 0.0f;
}

MiniTimer::~MiniTimer()
{

}

// return time in microseconds
LONGLONG MiniTimer::GetTime()
{
    QueryPerformanceCounter(&EndingTime);
    return (EndingTime.QuadPart - StartingTime.QuadPart) * 1000000 / Frequency.QuadPart;
}

void MiniTimer::Update()
{
    LARGE_INTEGER CurrentTime;
    QueryPerformanceCounter(&CurrentTime);
    
    FrameCount++;
    
    LONGLONG elapsedMicroseconds = (CurrentTime.QuadPart - LastFrameTime.QuadPart) * 1000000 / Frequency.QuadPart;
    
    if (elapsedMicroseconds >= FPS_UPDATE_INTERVAL)
    {
        CurrentFPS = (float)FrameCount * 1000000.0f / (float)elapsedMicroseconds;
        FrameCount = 0;
        LastFrameTime = CurrentTime;
    }
}

float MiniTimer::GetFPS()
{
    return CurrentFPS;
}

void MiniTimer::ResetFPS()
{
    QueryPerformanceCounter(&LastFrameTime);
    FrameCount = 0;
    CurrentFPS = 0.0f;
}