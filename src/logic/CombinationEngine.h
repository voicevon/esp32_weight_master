#ifndef COMBINATION_ENGINE_H
#define COMBINATION_ENGINE_H

#include <vector>
#include <Arduino.h>


class WeightNode;

struct CombinationResult {
    bool success;
    float totalWeight;
    std::vector<WeightNode*> selectedNodes; 
};

struct InternalNode {
    float weight;
    WeightNode* node;
};

// 搜索过程中的轻量化结果
struct SearchMatch {
    uint32_t mask;
    float totalWeight;
};

class CombinationEngine {
public:
    CombinationEngine(float minWeight, float maxWeight);
    
    // 配置接口
    void setMinWeight(float min) { _minWeight = min; }
    void setMaxWeight(float max) { _maxWeight = max; }
    void setTargetRange(float min, float max) { _minWeight = min; _maxWeight = max; }

    // 在提供的重量中寻找最佳组合
    CombinationResult findBestCombination(const std::vector<WeightNode*>& nodes);

private:
    float _minWeight;
    float _maxWeight;
    float _suffixSums[32]; // 存储后缀和以便剪枝加速

    void solveDFS(int index, float currentSum, uint32_t currentMask,
                  const std::vector<InternalNode>& candidates, 
                  std::vector<SearchMatch>& foundMatches,
                  unsigned long startTime, unsigned long timeoutMs);
};


#endif
