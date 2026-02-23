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

    // ==========================================
    // 内部処理用の分割関数群
    // ==========================================

    // ① SMアルゴリズムの計算
    void calculateStressMajorization(GraphData* graph) {
        for(size_t c = 0; c < components.size(); c++) {
            for(int i : components[c]) {
                float xi = graph->nodeData[i * nodeStride];
                float yi = graph->nodeData[i * nodeStride + 1];
                float sum_x = 0.0f, sum_y = 0.0f, sum_w = 0.0f;

                for(int j : components[c]) {
                    if (i == j) continue;
                    float d_ij = d[i * nodeSize + j];
                    float xj = graph->nodeData[j * nodeStride];
                    float yj = graph->nodeData[j * nodeStride + 1];
                    float dx = xi - xj;
                    float dy = yi - yj;
                    
                    if (dx == 0.0f && dy == 0.0f) {
                        dx = ((rand() % 100) - 50) / 1000.0f;
                        dy = ((rand() % 100) - 50) / 1000.0f;
                    }

                    float dist = std::max(0.01f, (float)std::sqrt(dx*dx + dy*dy));
                    float w_ij = 1.0f / d_ij; 
                    float target_x = xj + (dx/dist) * d_ij;
                    float target_y = yj + (dy/dist) * d_ij;

                    sum_w += w_ij;
                    sum_x += w_ij * target_x;
                    sum_y += w_ij * target_y;
                }
                
                if (sum_w > 0.0f) {
                    target_nx[i] = sum_x / sum_w;
                    target_ny[i] = sum_y / sum_w;
                }
            }
        }
    }

    // ② グローバル斥力（密グラフを円形に押し広げる）
    void applyNodeRepulsion() {
        float k = baseDistance; // 基準距離
        
        for(size_t c = 0; c < components.size(); c++) {
            for(size_t i = 0; i < components[c].size(); i++) {
                int u = components[c][i];
                for(size_t j = i + 1; j < components[c].size(); j++) {
                    int v = components[c][j];
                    float dx = target_nx[u] - target_nx[v];
                    float dy = target_ny[u] - target_ny[v];
                    
                    // 完全に一致した場合の微小ランダムズラし
                    if (dx == 0.0f && dy == 0.0f) {
                        dx = ((rand() % 100) - 50) / 1000.0f;
                        dy = ((rand() % 100) - 50) / 1000.0f;
                    }

                    float dist = std::max(0.01f, (float)std::sqrt(dx*dx + dy*dy));
                    
                    // FRモデルのクーロン斥力（距離が近いほど急激に強く、遠くても弱くかかり続ける）
                    float force = (k * k) / dist * 0.03f; // 0.03f は引力とのバランス係数
                    
                    float fx = (dx/dist) * force;
                    float fy = (dy/dist) * force;
                    target_nx[u] += fx; target_ny[u] += fy;
                    target_nx[v] -= fx; target_ny[v] -= fy;
                }
            }
        }
    }

    // ③ 指向性（縦長・横長）に合わせた回転
    void updateComponentOrientations(GraphData* graph) {
        for(size_t c = 0; c < components.size(); c++) {
            if (components[c].empty()) continue;
            float min_x = inf, max_x = -inf, min_y = inf, max_y = -inf;
            for(int i : components[c]) {
                if (target_nx[i] < min_x) min_x = target_nx[i];
                if (target_nx[i] > max_x) max_x = target_nx[i];
                if (target_ny[i] < min_y) min_y = target_ny[i];
                if (target_ny[i] > max_y) max_y = target_ny[i];
            }
            
            float w = max_x - min_x;
            float h = max_y - min_y;
            bool shouldRotate = false;
            
            if (preferHorizontal && h > w * 1.2f) shouldRotate = true;
            if (!preferHorizontal && w > h * 1.2f) shouldRotate = true;

            if (shouldRotate) {
                float cx = (min_x + max_x) / 2.0f;
                float cy = (min_y + max_y) / 2.0f;
                
                for(int i : components[c]) {
                    float nx = target_nx[i] - cx;
                    float ny = target_ny[i] - cy;
                    target_nx[i] = cx - ny;
                    target_ny[i] = cy + nx;
                    
                    float c_nx = graph->nodeData[i * nodeStride] - cx;
                    float c_ny = graph->nodeData[i * nodeStride + 1] - cy;
                    graph->nodeData[i * nodeStride] = cx - c_ny;
                    graph->nodeData[i * nodeStride + 1] = cy + c_nx;
                }
            }
        }
    }

    // ④ フレキシブルな力学パッキング（Bubble Packing）
    void packComponentsForceDirected() {
        if (components.empty()) return;

        std::vector<float> comp_cx(components.size(), 0.0f);
        std::vector<float> comp_cy(components.size(), 0.0f);
        std::vector<float> comp_r(components.size(), 0.0f); // コンポーネントの半径（泡の大きさ）

        // 各コンポーネントの中心と半径を計算
        for(size_t c = 0; c < components.size(); c++) {
            if (components[c].empty()) continue;
            float min_x = inf, max_x = -inf, min_y = inf, max_y = -inf;
            for(int i : components[c]) {
                if (target_nx[i] < min_x) min_x = target_nx[i];
                if (target_nx[i] > max_x) max_x = target_nx[i];
                if (target_ny[i] < min_y) min_y = target_ny[i];
                if (target_ny[i] > max_y) max_y = target_ny[i];
            }
            comp_cx[c] = (min_x + max_x) / 2.0f;
            comp_cy[c] = (min_y + max_y) / 2.0f;
            
            float w = max_x - min_x;
            float h = max_y - min_y;
            // 独立した1頂点でもしっかり領域を確保する (半径は少し大きめの25.0fに)
            comp_r[c] = std::max(25.0f, (float)std::sqrt(w*w + h*h) / 2.0f); 
        }

        std::vector<float> force_x(components.size(), 0.0f);
        std::vector<float> force_y(components.size(), 0.0f);
        float padding = 20.0f; // 泡同士の隙間

        // 泡同士の反発力（重ならないように押し返す）
        for(size_t i = 0; i < components.size(); i++) {
            for(size_t j = i + 1; j < components.size(); j++) {
                float dx = comp_cx[i] - comp_cx[j];
                float dy = comp_cy[i] - comp_cy[j];
                
                // ★修正1: 完全に重なっている場合、微小なランダム方向の力を与えて確実に分離させる
                if (dx == 0.0f && dy == 0.0f) {
                    dx = ((rand() % 100) - 50) / 1000.0f;
                    dy = ((rand() % 100) - 50) / 1000.0f;
                }

                float dist = std::max(0.01f, (float)std::sqrt(dx*dx + dy*dy));
                float minDist = comp_r[i] + comp_r[j] + padding;

                if (dist < minDist) {
                    float overlap = minDist - dist;
                    // ★修正2: 引力に負けないよう、押し出す反発力を強めに設定 (0.5f)
                    float fx = (dx/dist) * overlap * 0.5f; 
                    float fy = (dy/dist) * overlap * 0.5f;
                    force_x[i] += fx; force_y[i] += fy;
                    force_x[j] -= fx; force_y[j] -= fy;
                }
            }
        }

        // 画面中心 (300, 200) への引力
        for(size_t i = 0; i < components.size(); i++) {
            float dx = 300.0f - comp_cx[i];
            float dy = 200.0f - comp_cy[i];
            // ★修正3: 全体を綺麗な円形にするため、X軸とY軸で均等な引力をかける
            float force = 0.02f; // 反発力に負けるくらい弱めの引力にするのがコツ
            force_x[i] += dx * force;
            force_y[i] += dy * force;
        }

        // 計算した力をターゲット座標に適用
        for(size_t c = 0; c < components.size(); c++) {
            for(int i : components[c]) {
                target_nx[i] += force_x[c];
                target_ny[i] += force_y[c];
            }
        }
    }

    // ⑤ 重心のズレをキャンセルし、ドリフト（流され）を防ぐ
    void removeComponentDrift(GraphData* graph) {
        for(size_t c = 0; c < components.size(); c++) {
            if (components[c].empty()) continue;
            
            float sum_x = 0.0f, sum_y = 0.0f;
            float t_sum_x = 0.0f, t_sum_y = 0.0f;
            
            for(int i : components[c]) {
                sum_x += graph->nodeData[i * nodeStride];
                sum_y += graph->nodeData[i * nodeStride + 1];
                t_sum_x += target_nx[i];
                t_sum_y += target_ny[i];
            }
            
            int n = components[c].size();
            float cx = sum_x / n;        // 現在の重心
            float cy = sum_y / n;
            float t_cx = t_sum_x / n;    // 計算後の重心
            float t_cy = t_sum_y / n;

            // 意図しない全体のズレ（ドリフト量）
            float drift_x = t_cx - cx;
            float drift_y = t_cy - cy;
            
            // ドリフト量をすべてのターゲット座標から引き算して相殺する
            for(int i : components[c]) {
                target_nx[i] -= drift_x;
                target_ny[i] -= drift_y;
            }
        }
    }

public:
    bool is_stable = false;
    float epsilon = 0.5f;
    bool preferHorizontal = true;
    int stable_count = 0;
    int required_stable_frames = 20;

    void init(GraphData* graph) {
        nodeStride = graph->NODE_STRIDE;
        edgeStride = graph->EDGE_STRIDE;
        nodeSize = graph->nodeData.size()/nodeStride;
        edgeSize = graph->edgeData.size()/edgeStride;
        d.assign(nodeSize*nodeSize, inf);
        target_nx.assign(nodeSize, 0.0f);
        target_ny.assign(nodeSize, 0.0f);
        
        std::vector<std::vector<int>> adj(nodeSize);
        for (int i=0; i<edgeSize; i++){
            int from = (int)graph->edgeData[i * edgeStride];
            int to   = (int)graph->edgeData[i * edgeStride +1];
            if (from != to) {
                adj[from].push_back(to);
                adj[to].push_back(from);
            }
        }

        for (int i=0; i<nodeSize; i++){
            d[i * nodeSize + i] = 0.0f;
            std::queue<int> q;
            q.push(i);
            while(!q.empty()){
                int u = q.front(); q.pop();
                for(int v: adj[u]){
                    if (d[i*nodeSize + v] == inf) {
                        d[i*nodeSize + v] = d[i*nodeSize + u] + baseDistance;
                        q.push(v);
                    }
                }
            }
        }

        components.clear();
        std::vector<bool> visited(nodeSize, false);
        for (int i=0; i<nodeSize; i++){
            if (!visited[i]) {
                std::vector<int> comp;
                for (int j=0; j<nodeSize; j++){
                    if (d[i*nodeSize + j] != inf) {
                        visited[j] = true;
                        comp.push_back(j);
                    }
                }
                components.push_back(comp);
            }
        }
    }

    // ★ update は各関数を呼ぶだけでスッキリ！
    bool update(GraphData* graph) {
        if (is_stable) return true;

        // 1. 各コンポーネント内の形を整える
        calculateStressMajorization(graph);

        // 2. 遊泳して画面外に流れてしまうのを防ぐ
        removeComponentDrift(graph);
        
        // 3. 密集を防ぎ、円形に膨らませる
        applyNodeRepulsion();

        // 4. 縦横の指向性に合わせて回転する
        updateComponentOrientations(graph);

        // 5. コンポーネント同士を泡のようにパッキングする
        packComponentsForceDirected();

        // 6. イージングをかけて実際の座標を動かし、収束を判定する
        float max_movement = 0.0f;
        for (int i=0; i<nodeSize; i++){
            float xi = graph->nodeData[i * nodeStride];
            float yi = graph->nodeData[i * nodeStride + 1];
            float new_x = xi + (target_nx[i] - xi) * 0.3f;
            float new_y = yi + (target_ny[i] - yi) * 0.3f;

            float dx = new_x - xi;
            float dy = new_y - yi;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist > max_movement) max_movement = dist;

            graph->nodeData[i * nodeStride] = new_x;
            graph->nodeData[i * nodeStride + 1] = new_y;
        }

        //  収束判定のアップデート（忍耐カウンター）
        if (max_movement < epsilon) {
            stable_count++;
            if (stable_count >= required_stable_frames) {
                is_stable = true;
            }
        } else {
            stable_count = 0; // 少しでも大きく動いたら最初から数え直し！
        }

        return is_stable;
    }
};

#endif