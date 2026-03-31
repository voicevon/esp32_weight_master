#include "CombinationEngine.h"
#include <algorithm>
#include <Arduino.h>

struct NodeRef {
    float weight;
    int index; // Original index in the input vector (0-indexed)
};

CombinationEngine::CombinationEngine(float minWeight, float maxWeight)
    : _minWeight(minWeight), _maxWeight(maxWeight) {}

CombinationResult CombinationEngine::findBestCombination(const std::vector<float>& weights) {
    CombinationResult result = {false, 0.0f, {}};
    int n = weights.size();
    if (n == 0) return result;

    // 1. 结构化并降序排序 (优先处理大物料)
    std::vector<NodeRef> nodes;
    nodes.reserve(n);
    for (int i = 0; i < n; i++) {
        if (weights[i] > 0) {
            nodes.push_back({weights[i], i});
        }
    }
    
    if (nodes.empty()) return result;

    std::sort(nodes.begin(), nodes.end(), [](const NodeRef& a, const NodeRef& b) {
        return a.weight > b.weight;
    });

    // 2. 贪婪初选：从重到轻累加，直到达到下限
    std::vector<int> selections; // 存储在 nodes 中的索引
    float currentSum = 0;
    
    for (int i = 0; i < (int)nodes.size(); i++) {
        currentSum += nodes[i].weight;
        selections.push_back(i);
        if (currentSum >= _minWeight) break;
    }

    // 3. 初选校验：如果初选就超上限，说明无法通过大斗组合直接满足
    if (currentSum < _minWeight || currentSum > _maxWeight) {
        // 尝试从头开始找一个单斗满足的 (兜底)
        for(int i = 0; i < (int)nodes.size(); i++) {
            if (nodes[i].weight >= _minWeight && nodes[i].weight <= _maxWeight) {
                result.success = true;
                result.totalWeight = nodes[i].weight;
                result.selectedIndices = {nodes[i].index + 1};
                return result;
            }
        }
        return result; 
    }

    // 4. 置换优化：尝试用剩余的“较小”斗替换掉已选中的“最小”斗，看能否更逼近下限
    bool optimized = true;
    while (optimized) {
        optimized = false;
        
        // 找到当前选中组合中权重最小的那个
        int smallest_in_sel_idx = -1;
        float min_weight_in = 1e9;
        int list_pos_in_selections = -1;

        for (int i = 0; i < (int)selections.size(); i++) {
            int node_idx = selections[i];
            if (nodes[node_idx].weight < min_weight_in) {
                min_weight_in = nodes[node_idx].weight;
                smallest_in_sel_idx = node_idx;
                list_pos_in_selections = i;
            }
        }

        // 在未选中的斗中寻找一个替代者
        for (int i = 0; i < (int)nodes.size(); i++) {
            // 检查是否已选中
            bool already_selected = false;
            for (int s : selections) if (s == i) { already_selected = true; break; }
            if (already_selected) continue;

            float newWeight = currentSum - min_weight_in + nodes[i].weight;
            
            // 如果新重量仍在范围内，且比当前重量更接近下限，则替换
            if (newWeight >= _minWeight && newWeight < currentSum) {
                currentSum = newWeight;
                selections[list_pos_in_selections] = i;
                optimized = true;
                break; // 继续下一轮全局置换
            }
        }
    }

    // 5. 封装结果
    result.success = true;
    result.totalWeight = currentSum;
    for (int node_idx : selections) {
        result.selectedIndices.push_back(nodes[node_idx].index + 1);
    }

    return result;
}
