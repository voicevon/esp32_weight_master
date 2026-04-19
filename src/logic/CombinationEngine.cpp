#include "CombinationEngine.h"
#include "logic/WeightNode.h"
#include <algorithm>
#include <Arduino.h>

CombinationEngine::CombinationEngine(float minWeight, float maxWeight)
    : _minWeight(minWeight), _maxWeight(maxWeight) {}

CombinationResult CombinationEngine::findBestCombination(const std::vector<WeightNode*>& nodes_in) {
    unsigned long startTime = millis();
    CombinationResult result = {false, 0.0f, {}};
    int n = nodes_in.size();
    if (n == 0) return result;

    // 1. 结构化并降序排序 (位掩码限制 32 位)
    std::vector<InternalNode> internal_nodes;
    internal_nodes.reserve(n > 32 ? 32 : n);
    for (auto* node : nodes_in) {
        float w = node->getWeight();
        if (w > 0) {
            internal_nodes.push_back({w, node});
            if (internal_nodes.size() >= 32) break; 
        }
    }
    
    if (internal_nodes.empty()) return result;

    std::sort(internal_nodes.begin(), internal_nodes.end(), [](const InternalNode& a, const InternalNode& b) {
        return a.weight > b.weight;
    });

    // 预计算后缀和 (Suffix Sums) 用于数学剪枝
    int count = internal_nodes.size();
    float currentSuffixSum = 0;
    for (int i = count - 1; i >= 0; i--) {
        currentSuffixSum += internal_nodes[i].weight;
        _suffixSums[i] = currentSuffixSum;
    }

    // 2. 递归搜索 (带位掩码加速、后缀剪枝、时间熔断)
    std::vector<SearchMatch> matches;
    matches.reserve(8);
    
    solveDFS(0, 0.0f, 0, internal_nodes, matches, startTime, 10); // 10ms 工业级熔断保护

    // 3. 从候选掩码中提取最优解 (最小重量优先，同重最少斗数优先)
    if (matches.empty()) {
        return result;
    }

    int bestIdx = 0;
    for (int i = 1; i < (int)matches.size(); i++) {
        // 准则 A：Giveaway 更低
        if (matches[i].totalWeight < matches[bestIdx].totalWeight - 0.01f) {
            bestIdx = i;
        }
        // 准则 B：重量相等时，斗数更少 (减少物料损耗)
        else if (abs(matches[i].totalWeight - matches[bestIdx].totalWeight) < 0.01f) {
            if (__builtin_popcount(matches[i].mask) < __builtin_popcount(matches[bestIdx].mask)) {
                bestIdx = i;
            }
        }
    }

    // 还原结果
    result.success = true;
    result.totalWeight = matches[bestIdx].totalWeight;
    uint32_t bestMask = matches[bestIdx].mask;
    for (int i = 0; i < count; i++) {
        if (bestMask & (1 << i)) {
            result.selectedNodes.push_back(internal_nodes[i].node);
        }
    }

    unsigned long duration = millis() - startTime;
    if (duration > 5) {
        Serial.printf("[Combo] Optimized: %d sols, %lu ms. Best: %.1f g (Mask: 0x%08X)\n", 
                      (int)matches.size(), duration, result.totalWeight, bestMask);
    }

    return result;
}

void CombinationEngine::solveDFS(int index, float currentSum, uint32_t currentMask,
                                const std::vector<InternalNode>& candidates, 
                                std::vector<SearchMatch>& foundMatches,
                                unsigned long startTime, unsigned long timeoutMs) {
    
    // 方案 2：工业级硬熔断机制，保障实时性
    if (millis() - startTime > timeoutMs) return;

    // 熔断：已找到足够及格解则停止
    if (foundMatches.size() >= 5) return;

    // 及格判定
    if (currentSum >= _minWeight && currentSum <= _maxWeight) {
        foundMatches.push_back({currentMask, currentSum});
        return; 
    }

    // 方案 1：核心数学剪枝
    if (index >= (int)candidates.size() || currentSum > _maxWeight) return;
    
    // 后缀和剪枝：如果剩下全加起来也不够最小重量，直接回溯
    if (currentSum + _suffixSums[index] < _minWeight) return; 

    // 分支 1：包含当前节点 (递归优先尝试大重量节点快速靠拢目标)
    solveDFS(index + 1, currentSum + candidates[index].weight, currentMask | (1 << index), 
             candidates, foundMatches, startTime, timeoutMs);

    // 分支 2：不选当前节点
    solveDFS(index + 1, currentSum, currentMask, 
             candidates, foundMatches, startTime, timeoutMs);
}

