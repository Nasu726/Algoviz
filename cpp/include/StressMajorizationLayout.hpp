#ifndef STRESS_MAJORIZATION_LAYOUT_HPP
#define STRESS_MAJORIZATION_LAYOUT_HPP

#pragma once
#include "GraphData.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <queue>

class StressMajorizationLayout {
private:
    bool is_stable = false;
    float epsilon = 0.05f;
    float inf = 999999.0f;
    float baseDistance = 80.0f;

    int nodeStride;
    int edgeStride;
    int nodeSize;
    int edgeSize;
    std::vector<float> d;

    std::vector<std::vector<int>> components;
    std::vector<float> target_nx;
    std::vector<float> target_ny;
public:
    void init(GraphData* graph) {
        nodeStride = graph->NODE_STRIDE;
        edgeStride = graph->EDGE_STRIDE;
        nodeSize = graph->nodeData.size()/nodeStride;
        edgeSize = graph->edgeData.size()/edgeStride;
        d.assign(nodeSize*nodeSize, inf);
        target_nx.assign(nodeSize, 0.0f);
        target_ny.assign(nodeSize, 0.0f);
        
        // BFSのために連結リストを作成
        std::vector<std::vector<int>> adj(nodeSize);
        for (int i=0; i<edgeSize; i++){
            int from = (int)graph->edgeData[i * edgeStride];
            int to   = (int)graph->edgeData[i * edgeStride + 1];
            if (from != to) {
                adj[from].push_back(to);
                adj[to].push_back(from);
            }
        }

        // 全頂点からのBFSで理想距離を計算
        for (int i=0; i<nodeSize; i++){
            d[i * nodeSize + i] = 0.0f;
            std::queue<int> q;
            q.push(i);

            while (!q.empty()) {
                int u = q.front(); q.pop();
                for (int v: adj[u]) {
                    if (d[i * nodeSize + v] == inf) {
                        d[i * nodeSize + v] = d[i * nodeSize + u] + baseDistance;
                        q.push(v);
                    }
                }
            }
        }
        // 非連結グラフを、連結成分のグループに分ける
        components.clear();
        std::vector<bool> visited(nodeSize, false);
        for (int i=0; i<nodeSize; i++){
            if (!visited[i]) {
                std::vector<int> comp;
                for (int j=0; j<nodeSize; j++){
                    if (d[i * nodeSize + j] != inf) {
                        visited[j] = true;
                        comp.push_back(j);
                    }
                }
                components.push_back(comp);
            }
        }
    }

    bool update(GraphData* graph){
        if (is_stable) return true;
        float max_movement = 0.0f;
        // すべてのノード i について、新しい座標を計算する
        for (int i = 0; i < nodeSize; i++){
            float sum_w = 0.0f;
            float sum_x = 0.0f;
            float sum_y = 0.0f;

            // ノード i の現在の座標
            float xi = graph->nodeData[i * nodeStride];
            float yi = graph->nodeData[i * nodeStride + 1];

            // 他の全てのノード j との調整をする
            for (int j = 0; j < nodeSize; j++) {
                if (i == j) continue;
                float d_ij = d[i * nodeSize + j];
                if (d_ij == inf) continue; // 非連結なら無視

                // ノード j の現在の座標 xj, yj を取得する
                float xj = graph->nodeData[j * nodeStride];
                float yj = graph->nodeData[j * nodeStride + 1];

                // ノード i とノード j の座標の差を求め、距離を求める
                // distは0だとその後の処理で0除算してしまうので、0.01fを最小値とする
                float dx = xi - xj;
                float dy = yi - yj;

                float dist = std::max(0.01f, (float)std::sqrt(dx*dx + dy*dy));
                float w_ij = 1.0f / (d_ij );


                // ノード j から見た i の理想の座標 (target_x, target_y)
                float target_x = xj + (dx/dist) * d_ij;
                float target_y = yj + (dy/dist) * d_ij;

                // 重み付けをして座標を足す
                sum_w += w_ij;
                sum_x += w_ij * target_x;
                sum_y += w_ij * target_y;

            }
            if (sum_w > 0.0f){
                // ノード i の新しい座標を決定
                target_nx[i] = sum_x / sum_w;
                target_ny[i] = sum_y / sum_w;
            } else {
                target_nx[i] = xi;
                target_ny[i] = yi;
            }
        }

        if (components.size() > 0) {
            float padding = 60.0f;

            int cols = std::max(1, (int)std::ceil(std::sqrt(components.size())));

            std::vector<float> comp_widths(components.size(), 0.0f);
            std::vector<float> comp_heights(components.size(), 0.0f);
            std::vector<float> comp_cx(components.size(), 0.0f);
            std::vector<float> comp_cy(components.size(), 0.0f);

            for (size_t c = 0; c < components.size(); c++){
                if (components[c].empty()) continue;
                float min_x = inf, max_x = -inf, min_y = inf, max_y = -inf;
                for (int i: components[c]) {
                    if (target_nx[i] < min_x) min_x = target_nx[i];
                    if (target_nx[i] > max_x) max_x = target_nx[i];
                    if (target_ny[i] < min_y) min_y = target_ny[i];
                    if (target_ny[i] > max_y) max_y = target_ny[i];
                }
                comp_widths[c] = max_x - min_x;
                comp_heights[c] = max_y - min_y;
                comp_cx[c] = (min_x + max_x) / 2.0f;
                comp_cy[c] = (min_y + max_y) / 2.0f;
            }

            std::vector<float> grid_cx(components.size(), 0.0f);
            std::vector<float> grid_cy(components.size(), 0.0f);
            
            float current_x = 0.0f;
            float current_y = 0.0f;
            float row_max_height = 0.0f;

            float total_min_x = inf, total_max_x = -inf, total_min_y = inf, total_max_y = -inf;

            for (size_t c = 0; c < components.size(); c++) {
                if (components[c].empty()) continue;

                if (c > 0 && c % cols == 0) {
                    current_x = 0.0f;
                    current_y += row_max_height + padding;
                    row_max_height = 0.0f;
                }

                float w = comp_widths[c];
                float h = comp_heights[c];
                if ( h > row_max_height) row_max_height = h;

                grid_cx[c] = current_x + w / 2.0f;
                grid_cy[c] = current_y + h / 2.0f;

                if (current_x < total_min_x)     total_min_x = current_x;
                if (current_x + w > total_max_x) total_max_x = current_x + w;
                if (current_y < total_min_y)     total_min_y = current_y;
                if (current_y + h > total_max_y) total_max_y = current_y + h;

                current_x += w + padding;
            }

            float total_cx = (total_min_x + total_max_x) / 2.0f;
            float total_cy = (total_min_y + total_max_y) / 2.0f;
            float offset_x = 300.0f - total_cx;
            float offset_y = 200.0f - total_cy;

            for (size_t c = 0; c < components.size(); c++) {
                if (components[c].empty()) continue;

                float shift_x = (grid_cx[c] + offset_x) - comp_cx[c];
                float shift_y = (grid_cy[c] + offset_y) - comp_cy[c];

                for (int i: components[c]) {
                    target_nx[i] += shift_x;
                    target_ny[i] += shift_y;
                }
            }
        }
        
        for (int i=0; i<nodeSize; i++){
            float xi = graph->nodeData[i * nodeStride];
            float yi = graph->nodeData[i * nodeStride + 1];

            float new_x = xi + (target_nx[i] - xi) * 0.3f;
            float new_y = yi + (target_ny[i] - yi) * 0.3f;

            // ★移動量をチェック
            float dx = new_x - xi;
            float dy = new_y - yi;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist > max_movement) {
                max_movement = dist;
            }

            graph->nodeData[i * nodeStride]     = new_x;
            graph->nodeData[i * nodeStride + 1] = new_y;
        }

        if (max_movement < epsilon){
            is_stable = true;
        }
        return is_stable;
    }
};

#endif