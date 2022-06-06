#pragma once
#include <deque>
#include "recorder.h"


extern std::pair<int, double> CalculateMinus(const std::deque<float>& bufferA, const std::deque<float>& bufferB, const peak& thePeak, size_t nSampleSize, bool bLocked, const std::pair<int, double>& last);
