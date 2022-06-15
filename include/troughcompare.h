#include "hash.h"
#include <vector>
#include <deque>

extern hashresult CalculateFFTDiff(const std::deque<float>& bufferA, const std::deque<float>& bufferB, unsigned int nSampleSize, size_t nBands, double dLimits, double dChangeDown, double dChangeUp);
