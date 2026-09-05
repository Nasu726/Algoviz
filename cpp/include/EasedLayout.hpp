#pragma once
#include "GraphData.hpp"
#include "ILayout.hpp"
#include <vector>
#include <algorithm>
#include <cmath>

// 目標の座標へ少しずつ寄せる配置の共通部分。
//
// 木も配列も「置く場所は計算で一意に決まる。あとはそこへ動かすだけ」なので、
// 違うのは目標をどう決めるか (computeTargets) だけになる。
// 力学モデル (GeneralGraphLayout) は目標を持たずに毎フレーム力を解くので、
// こちらには乗らない。
//
// 一気に目標へ飛ばさないのは、値が入れ替わったり木の形が変わったりしたときに
// 節点が瞬間移動して、何が起きたか分からなくなるため。
class EasedLayout : public ILayout {
protected:
    // 目標へ寄せる割合。指数的に減衰するので、所要フレームは
    // log(1/(1-EASE)) に反比例する。
    // 0.3 -> 0.277 で 1.1 倍、0.277 -> 0.221 でさらに 1.3 倍、
    // 0.221 -> 0.203 でさらに 1.1 倍かかる。
    static constexpr float EASE    = 0.203f;
    static constexpr float EPSILON = 0.5f;

    bool stable = false;
    int nodeSize = 0;
    std::vector<float> targetX, targetY;

    // 節点ごとの目標の座標を決める。派生クラスが実装する唯一の部分。
    virtual void computeTargets(GraphData* graph) = 0;

public:
    void init(GraphData* graph, const std::vector<std::vector<int>>& adj) override {
        (void)adj; // 目標を計算する側が必要な情報を graph から直接読む
        nodeSize = graph->nodeCount();
        stable = false;
        if (nodeSize == 0) {
            stable = true;
            return;
        }
        computeTargets(graph);
    }

    bool update(GraphData* graph) override {
        if (stable) return true;

        float maxMove = 0.0f;
        for (int i = 0; i < nodeSize; i++) {
            std::size_t o = (std::size_t)i * GraphData::NODE_STRIDE;
            float x = graph->nodeData[o], y = graph->nodeData[o + 1];
            float nx = x + (targetX[i] - x) * EASE;
            float ny = y + (targetY[i] - y) * EASE;
            maxMove = std::max(maxMove, std::hypot(nx - x, ny - y));
            graph->nodeData[o] = nx;
            graph->nodeData[o + 1] = ny;
        }

        // 目標に十分近づいたら、ぴったり合わせて終わりにする
        if (maxMove < EPSILON) finish(graph);
        return stable;
    }

    void finish(GraphData* graph) override {
        for (int i = 0; i < nodeSize; i++) {
            std::size_t o = (std::size_t)i * GraphData::NODE_STRIDE;
            graph->nodeData[o] = targetX[i];
            graph->nodeData[o + 1] = targetY[i];
        }
        stable = true;
    }

    bool isStable() const override { return stable; }
    void invalidate() override { stable = false; }
};
