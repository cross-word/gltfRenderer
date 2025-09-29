#pragma once
#include <windows.h>

class MiniTimer
{
public:
    MiniTimer();
    ~MiniTimer();
    LONGLONG GetTime();
    
    void Update();
    float GetFPS();
    void ResetFPS();

private:
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;
    
    LARGE_INTEGER LastFrameTime;
    int FrameCount;
    float CurrentFPS;
    static const int FPS_UPDATE_INTERVAL = 1000000;
};