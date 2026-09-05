#pragma once
#include "EasedLayout.hpp"

// 節点を左から順に一列に並べる配置。配列を見せるためのもの。
//
// **節点の番号がそのまま添字。** 値が入れ替わっても箱は動かないので、
// 目標の座標は節点の数だけで決まる。辺は見ない (配列に辺は無い)。
//
// 入れ替えを見せるのは swapNodePositions の役目。今いる座標だけを入れ替えると、
// EasedLayout が元の位置へ戻す間に「値が移動した」ように見える。
class LineLayout : public EasedLayout {
public:
    static constexpr float CELL_GAP = 8.0f; // 隣り合う箱の縁どうしの間隔

protected:
    void computeTargets(GraphData* graph) override {
        targetX.assign(nodeSize, 0.0f);
        targetY.assign(nodeSize, 0.0f);

        float x = 0.0f;
        for (int i = 0; i < nodeSize; i++) {
            float half = graph->halfWidthOf(i);
            x += half;
            targetX[i] = x;
            x += half + CELL_GAP;
        }
    }
};
