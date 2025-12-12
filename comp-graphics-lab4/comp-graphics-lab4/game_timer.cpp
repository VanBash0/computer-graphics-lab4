#include "game_timer.h"

#include <windows.h>

GameTimer::GameTimer() : mSecondsPerCount(0.0), mDeltaTime(0.0), mBaseTime(0), mStopTime(0.0),
mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false) {
    __int64 countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
    mSecondsPerCount = 1.0 / static_cast<double>(countsPerSec);
}

float GameTimer::getDeltaTime() const { return mDeltaTime; }

float GameTimer::getTotalTime() const {
    if (mStopped) {
        return static_cast<float>(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
    }
    else {
        return static_cast<float>(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
    }
}

void GameTimer::tick() {
    if (mStopped) {
        mDeltaTime = 0.0f;
        return;
    }

    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    mCurrTime = currTime;

    mDeltaTime = static_cast<float>((mCurrTime - mPrevTime) * mSecondsPerCount);
    mPrevTime = mCurrTime;

    if (mDeltaTime < 0.0f) {
        mDeltaTime = 0.0f;
    }
}

void GameTimer::reset() {
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    mBaseTime = currTime;
    mPrevTime = currTime;
    mStopTime = 0;
    mStopped = false;
}

void GameTimer::stop() {
    if (!mStopped) {
        __int64 currTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
        mStopTime = currTime;
        mStopped = true;
    }
}

void GameTimer::start() {
    if (mStopped) {
        __int64 startTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
        mPausedTime += (startTime - mStopTime);
        mPrevTime = startTime;
        mStopTime = 0;
        mStopped = false;
    }
}

float GameTimer::getTotalTime() const {
    if (mStopped) {
        return static_cast<float>(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
    }
    else {
        return static_cast<float>(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
    }
}