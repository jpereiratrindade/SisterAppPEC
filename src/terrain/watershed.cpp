#include "watershed.h"
#include "terrain_map.h"
#include <queue>
#include <vector>
#include <algorithm>
#include <iostream>

namespace terrain {

// Helper to build adjacency list (In-Flow)
// Since we only store Out-Flow (flowDirMap), we need to invert it efficiently or search neighbors.
// Searching neighbors is O(1) per node traversal if we do it on the fly.
// For global segmentation, iterating all nodes once to build an adjacency list is O(N).
// Let's do on-the-fly for single delineation, and maybe adjacency for global?
// Actually, iterating all pixels and checking neighbors is fast enough.

std::vector<uint8_t> Watershed::delineate(TerrainMap& map, int startX, int startY, int basinID) {
    int w = map.getWidth();
    int h = map.getHeight();
    size_t size = static_cast<size_t>(w * h);
    
    std::vector<uint8_t> mask(size, 0);
    
    if (!map.isValid(startX, startY)) return mask;

    std::queue<int> q;
    int startIdx = startY * w + startX;
    q.push(startIdx);
    mask[startIdx] = 255;
    
    if (basinID > 0) {
        // Clear previous marks if needed? No, just overwrite.
        // Or user clears before calling? User responsibility.
        map.watershedMap()[startIdx] = basinID;
    }

    const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    while (!q.empty()) {
        int idx = q.front();
        q.pop();
        
        int cx = idx % w;
        int cy = idx / w;

        // Check neighbors that flow INTO current cell
        for (int i = 0; i < 8; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (map.isValid(nx, ny)) {
                int nIdx = ny * w + nx;
                int receiver = map.flowDirMap()[nIdx];
                
                // If neighbor flows into current, it is part of the basin
                if (receiver == idx) {
                    if (mask[nIdx] == 0) {
                        mask[nIdx] = 255;
                        if (basinID > 0) map.watershedMap()[nIdx] = basinID;
                        q.push(nIdx);
                    }
                }
            }
        }
    }
    
    return mask;
}

int Watershed::segmentGlobal(TerrainMap& map) {
    int w = map.getWidth();
    int h = map.getHeight();
    int size = w * h;
    
    // Clear existing
    std::fill(map.watershedMap().begin(), map.watershedMap().end(), 0);
    
    // 1. Find all Sinks (nodes that flow nowhere or to themselves or out of bounds)
    // In our flowDirMap, -1 means Sink (or edge).
    // Actually, we should assign a unique ID to each Sink and propagate upstream.
    
    int basinCounter = 1;
    
    // Using a queue for multi-source BFS
    std::queue<int> q;
    
    for (int i = 0; i < size; ++i) {
        int receiver = map.flowDirMap()[i];
        
        // If it's a sink (receiver is -1), it starts a basin.
        // Also if it flows to an already identified basin? No, we do upstream from sinks.
        // Wait, what if it flows off-map? Receiver is -1.
        if (receiver == -1) {
            map.watershedMap()[i] = basinCounter;
            q.push(i);
            basinCounter++;
        }
    }
    
    // 2. Propagate Upstream
    // This is like the delineate function but for all basins simultaneously.
    // Neighbors check if they flow INTO a labeled node.
    
    // BUT, standard BFS propagates form source to neighbors. 
    // Here flow is N -> C. We want to traverse C -> N (upstream).
    // So if C has label L, and N flows into C, N gets label L.
    
    // We can't use simple queue of sinks because we need to find "who flows into C".
    // We don't have an inverted list.
    // Building inverted list (adjacency) is best.
    
    std::vector<std::vector<int>> upstream(size);
    for (int i = 0; i < size; ++i) {
        int rec = map.flowDirMap()[i];
        if (rec != -1) {
            upstream[rec].push_back(i);
        }
    }
    
    // Now standard BFS using upstream links
    while (!q.empty()) {
        int curr = q.front();
        q.pop();
        
        int id = map.watershedMap()[curr];
        
        for (int up : upstream[curr]) {
            if (map.watershedMap()[up] == 0) {
                map.watershedMap()[up] = id;
                q.push(up);
            }
        }
    }
    
    std::cout << "[Watershed] Segmented " << (basinCounter - 1) << " basins." << std::endl;
    return basinCounter - 1;
}

} // namespace terrain
