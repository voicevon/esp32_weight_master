#include "CombinationEngine.h"
#include "logic/WeightNode.h"
#include <algorithm>
#include <Arduino.h>

struct InternalNode {
    float weight;
    WeightNode* node;
};

CombinationEngine::CombinationEngine(float minWeight, float maxWeight)
    : _minWeight(minWeight), _maxWeight(maxWeight) {}

CombinationResult CombinationEngine::findBestCombination(const std::vector<WeightNode*>& nodes_in) {
    CombinationResult result = {false, 0.0f, {}};
    int n = nodes_in.size();
    if (n == 0) return result;

    // 1. 结构化并降序排序
    std::vector<InternalNode> internal_nodes;
    internal_nodes.reserve(n);
    for (auto* node : nodes_in) {
        float w = node->getWeight();
        if (w > 0) {
            internal_nodes.push_back({w, node});
        }
    }
    
    if (internal_nodes.empty()) return result;

    std::sort(internal_nodes.begin(), internal_nodes.end(), [](const InternalNode& a, const InternalNode& b) {
        return a.weight > b.weight;
    });

    // 2. 贪婪初选
    std::vector<int> selections; // indices into internal_nodes
    float currentSum = 0;
    
    for (int i = 0; i < (int)internal_nodes.size(); i++) {
        currentSum += internal_nodes[i].weight;
        selections.push_back(i);
        if (currentSum >= _minWeight) break;
    }

    // 3. 初选校验
    if (currentSum < _minWeight || currentSum > _maxWeight) {
        for(int i = 0; i < (int)internal_nodes.size(); i++) {
            if (internal_nodes[i].weight >= _minWeight && internal_nodes[i].weight <= _maxWeight) {
                result.success = true;
                result.totalWeight = internal_nodes[i].weight;
                result.selectedNodes = {internal_nodes[i].node};
                return result;
            }
        }
        return result; 
    }

    // 4. 置换优化
    bool optimized = true;
    while (optimized) {
        optimized = false;
        int smallest_in_sel_idx = -1;
        float min_weight_in = 1e9;
        int list_pos_in_selections = -1;

        for (int i = 0; i < (int)selections.size(); i++) {
            int node_idx = selections[i];
            if (internal_nodes[node_idx].weight < min_weight_in) {
                min_weight_in = internal_nodes[node_idx].weight;
                smallest_in_sel_idx = node_idx;
                list_pos_in_selections = i;
            }
        }

        for (int i = 0; i < (int)internal_nodes.size(); i++) {
            bool already_selected = false;
            for (int s : selections) if (s == i) { already_selected = true; break; }
            if (already_selected) continue;

            float newWeight = currentSum - min_weight_in + internal_nodes[i].weight;
            if (newWeight >= _minWeight && newWeight < currentSum) {
                currentSum = newWeight;
                selections[list_pos_in_selections] = i;
                optimized = true;
                break;
            }
        }
    }

    // 5. 封装结果
    result.success = true;
    result.totalWeight = currentSum;
    for (int node_idx : selections) {
        result.selectedNodes.push_back(internal_nodes[node_idx].node);
    }

    return result;
}
