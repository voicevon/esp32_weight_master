
def find_best_combination_v1(nodes, target_min, target_max):
    # Current C++ logic simulation
    internal_nodes = sorted(nodes, reverse=True)
    n = len(internal_nodes)
    if n == 0: return None
    
    # 2. Greedy selection
    selections = []
    current_sum = 0
    for i in range(n):
        current_sum += internal_nodes[i]
        selections.append(i)
        if current_sum >= target_min:
            break
            
    # 3. Validation
    if current_sum < target_min or current_sum > target_max:
        # Check single nodes
        for i in range(n):
            if target_min <= internal_nodes[i] <= target_max:
                return [internal_nodes[i]], internal_nodes[i]
        return None, current_sum # Fail
        
    return [internal_nodes[i] for i in selections], current_sum

def test_v1():
    target_min = 180
    target_max = 195
    # Scenario: 100 + 100 > 195, but 100 + 85 = 185
    nodes = [100, 100, 85, 85]
    print(f"Target: {target_min}-{target_max}")
    print(f"Nodes: {nodes}")
    res, total = find_best_combination_v1(nodes, target_min, target_max)
    print(f"V1 Result: {res}, Total: {total}")

if __name__ == "__main__":
    test_v1()
