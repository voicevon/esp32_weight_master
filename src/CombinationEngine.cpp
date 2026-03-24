#include "CombinationEngine.h"
#include <algorithm>
#include <Arduino.h>

struct SubsetResult {
    float weight;
    uint32_t mask;
};

CombinationEngine::CombinationEngine(float targetWeight, float tolerance)
    : _targetWeight(targetWeight), _tolerance(tolerance) {}

CombinationResult CombinationEngine::findBestCombination(const std::vector<float>& weights) {
    CombinationResult bestResult = {false, 0.0f, {}};
    int n = weights.size();
    if (n == 0) return bestResult;

    int n1 = n / 2;
    int n2 = n - n1;

    // 1. 实现前半部分的生成
    std::vector<SubsetResult> left;
    left.reserve(1 << n1);
    for (int i = 0; i < (1 << n1); i++) {
        float sum = 0;
        for (int j = 0; j < n1; j++) if ((i >> j) & 1) sum += weights[j];
        left.push_back({sum, (uint32_t)i});
    }

    // 2. 预排序
    std::sort(left.begin(), left.end(), [](const SubsetResult& a, const SubsetResult& b) {
        return a.weight < b.weight;
    });

    float minError = _tolerance;
    
    // 3. 后半部分匹配
    for (int i = 0; i < (1 << n2); i++) {
        float sum2 = 0;
        for (int j = 0; j < n2; j++) if ((i >> j) & 1) sum2 += weights[n1 + j];

        float needed = _targetWeight - sum2;
        auto it = std::lower_bound(left.begin(), left.end(), needed, 
            [](const SubsetResult& sr, float val) { return sr.weight < val; });

        if (it != left.end()) {
            float total = it->weight + sum2;
            float error = total - _targetWeight;

            if (error >= 0 && error < minError) {
                minError = error;
                bestResult.success = true;
                bestResult.totalWeight = total;
                
                bestResult.selectedIndices.clear();
                for (int j = 0; j < n1; j++) if ((it->mask >> j) & 1) bestResult.selectedIndices.push_back(j + 1);
                for (int j = 0; j < n2; j++) if ((i >> j) & 1) bestResult.selectedIndices.push_back(n1 + j + 1);
                
                if (error == 0) break;
            }
        }
    }

    return bestResult;
}
