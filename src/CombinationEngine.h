#ifndef COMBINATION_ENGINE_H
#define COMBINATION_ENGINE_H

#include <vector>

struct CombinationResult {
    bool success;
    float totalWeight;
    std::vector<int> selectedIndices; // 1-based IDs
};

class CombinationEngine {
public:
    CombinationEngine(float targetWeight, float tolerance);
    
    // 配置接口
    void setTargetWeight(float target) { _targetWeight = target; }
    void setTolerance(float tol) { _tolerance = tol; }

    // 在提供的重量中寻找最佳组合
    CombinationResult findBestCombination(const std::vector<float>& weights);

private:
    float _targetWeight;
    float _tolerance;
};

#endif
