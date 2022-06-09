#pragma once
#include <map>
#include <vector>
#include <deque>

using hashresult = std::pair<int, double>;

extern hashresult CalculateHash(const std::deque<float>& vBufferA, const std::deque<float>& vBufferB, size_t nSampleSize, bool bLocked);

extern int CalculateOffset(std::vector<float> vBufferA, std::vector<float> vBufferB);

extern bool CheckForTone(std::vector<float> vBufferA, std::vector<float> vBufferB);


extern float HannWindow(float dIn, size_t nSample, size_t nSize);
