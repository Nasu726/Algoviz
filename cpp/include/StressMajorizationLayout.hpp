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
public:
    void init(GraphData* graph) {
        nodeStride = graph->NODE_STRIDE;
        edgeStride = graph->EDGE_STRIDE;
        nodeSize = graph->nodeData.size()/nodeStride;
        edgeSize = graph->edgeData.size()/edgeStride;
        d.assign(nodeSize*nodeSize, inf);
        
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
        float max_d = baseDistance;
        for (int i = 0; i < nodeSize * nodeSize; i++) {
            if (d[i] != inf && d[i] > max_d) {
                max_d = d[i];
            }
        }
        float disconnected_d = max_d + baseDistance;
        for (int i = 0; i < nodeSize * nodeSize; i++) {
            if (d[i] == inf) {
                d[i] = disconnected_d;
            }
        }
    }

    void update(GraphData* graph){
        int nodeStride = graph->NODE_STRIDE;

        std::vector<float> next_x(nodeSize, 0.0f);
        std::vector<float> next_y(nodeSize, 0.0f);

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
                // ノード j の現在の座標 xj, yj を取得する
                float xj = graph->nodeData[j * nodeStride];
                float yj = graph->nodeData[j * nodeStride + 1];

                // ノード i とノード j の座標の差を求め、距離を求める
                // distは0だとその後の処理で0除算してしまうので、0.01fを最小値とする
                float dx = xi - xj;
                float dy = yi - yj;
                float dist = std::max(0.01f, (float)std::sqrt(dx*dx + dy*dy));

                // ノード i, j の理想の距離を取得し、重みを計算する。距離がinfなら連結でないのでスキップ
                float d_ij = d[i * nodeSize + j];
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
                next_x[i] = sum_x / sum_w;
                next_y[i] = sum_y / sum_w;

                graph->nodeData[i * nodeStride]     = xi + (next_x[i] - xi) * 0.3f;
                graph->nodeData[i * nodeStride + 1] = yi + (next_y[i] - yi) * 0.3f;

            } else {
                next_x[i] = xi;
                next_y[i] = yi;
            }
        }
        // for (int i=0; i<nodeSize; i++){
        //     graph->nodeData[i * nodeStride]     = next_x[i];
        //     graph->nodeData[i * nodeStride + 1] = next_y[i];
        // }
    }
};

#endif