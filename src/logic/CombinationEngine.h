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
    CombinationEngine(float minWeight, float maxWeight);
    
    // 配置接口
    void setMinWeight(float min) { _minWeight = min; }
    void setMaxWeight(float max) { _maxWeight = max; }
    void setTargetRange(float min, float max) { _minWeight = min; _maxWeight = max; }

    // 在提供的重量中寻找最佳组合
    CombinationResult findBestCombination(const std::vector<float>& weights);

private:
    float _minWeight;
    float _maxWeight;
};

#endif
