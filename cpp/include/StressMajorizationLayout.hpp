#ifndef STRESS_MAJORIZATION_LAYOUT_HPP
#define STRESS_MAJORIZATION_LAYOUT_HPP

#pragma once
#include "GraphData.hpp"
#include <vector>
#include <algorithm>
#include <cmath>

class StressMajorizationLayout {
private:
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
        
        for (int i = 0; i < nodeSize; i++){
            d[i * nodeSize + i] = 0.0f;
        }
        for (int i=0; i<edgeSize; i++) {
            int from = (int)graph->edgeData[i * edgeStride];
            int to   = (int)graph->edgeData[i * edgeStride +1];
            if (from != to) {
                d[from * nodeSize + to] = baseDistance;
                d[to * nodeSize + from] = baseDistance;
            }
        }
        // ワ―シャルフロイド法
        for (int k=0; k<nodeSize; k++){
            for (int i=0; i<nodeSize; i++){
                for (int j=0; j<nodeSize; j++){
                    d[i * nodeSize + j] = std::min(d[i * nodeSize + j], d[i * nodeSize + k]+d[k * nodeSize + j]);
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

    void update(GraphData* graph){
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
                float w_ij = 1.0f / (d_ij * d_ij);


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

        float padding = 80.0f;
        std::vector<float> comp_widths(components.size(), 0.0f);
        std::vector<float> comp_current_cx(components.size(), 0.0f);
        std::vector<float> comp_current_cy(components.size(), 0.0f);
        float total_width = 0.0f;

        for (size_t c = 0; c < components.size(); c++){
            float min_x = inf, max_x = -inf, min_y = inf, max_y = -inf;
            for (int i: components[c]) {
                if (target_nx[i] < min_x) min_x = target_nx[i];
                if (target_nx[i] > max_x) max_x = target_nx[i];
                if (target_ny[i] < min_y) min_y = target_ny[i];
                if (target_ny[i] > max_y) max_y = target_ny[i];
            }

            if (min_x == inf) continue;

            comp_widths[c] = max_x - min_x;
            total_width += comp_widths[c];
            comp_current_cx[c] = (min_x + max_x) / 2.0f;
            comp_current_cy[c] = (min_y + max_y) / 2.0f;
        }

        total_width += padding * (components.size() - 1);

        float start_x = 300.0f - total_width / 2.0f;

        for (size_t c = 0; c < components.size(); c++){
            if (components[c].empty()) continue;

            float ideal_cx = start_x + comp_widths[c] / 2.0f;
            start_x += comp_widths[c] + padding;

            float shift_x = ideal_cx - comp_current_cx[c];
            float shift_y = 200.0f - comp_current_cy[c];

            for (int i: components[c]) {
                target_nx[i] += shift_x;
                target_ny[i] += shift_y;
            }
        }
        
        for (int i=0; i<nodeSize; i++){
            float xi = graph->nodeData[i * nodeStride];
            float yi = graph->nodeData[i * nodeStride + 1];
            graph->nodeData[i * nodeStride] = xi + (target_nx[i] - xi) * 0.1f;
            graph->nodeData[i * nodeStride + 1] = yi + (target_ny[i] - yi) * 0.1f;
        }
    }
};

#endif