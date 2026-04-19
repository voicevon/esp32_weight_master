
def find_best_combination_v2(nodes, target_min, target_max):
    # Better logic: Recursive search with pruning
    nodes = sorted(nodes, reverse=True)
    n = len(nodes)
    best_res = None
    best_sum = float('inf')
    
    def backtrack(idx, current_sum, selection):
        nonlocal best_res, best_sum
        
        # If we already found a perfect match (min weight), we could stop,
        # but let's find the best one in the range.
        if target_min <= current_sum <= target_max:
            if current_sum < best_sum:
                best_sum = current_sum
                best_res = list(selection)
            return # Prune: adding more nodes will only increase weight
            
        if current_sum > target_max:
            return # Prune
            
        if idx >= n:
            return
            
        # Optimization: if currentSum + remaining sum < target_min, prune
        # (This would need precomputed suffix sums)
        
        # Option 1: Include nodes[idx]
        selection.append(nodes[idx])
        backtrack(idx + 1, current_sum + nodes[idx], selection)
        selection.pop()
        
        # Option 2: Skip nodes[idx]
        backtrack(idx + 1, current_sum, selection)

    backtrack(0, 0, [])
    return best_res, best_sum

def test_v2():
    target_min = 180
    target_max = 195
    # Scenario: 100 + 100 > 195, but 100 + 85 = 185
    nodes = [100, 100, 85, 85]
    print(f"Target: {target_min}-{target_max}")
    print(f"Nodes: {nodes}")
    res, total = find_best_combination_v2(nodes, target_min, target_max)
    print(f"V2 Result: {res}, Total: {total}")

if __name__ == "__main__":
    test_v2()
