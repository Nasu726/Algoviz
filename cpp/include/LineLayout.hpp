#pragma once
#include "EasedLayout.hpp"

// 節点を左から順に一列に並べる配置。配列を見せるためのもの。
//
// **節点の番号がそのまま添字。** 値が入れ替わっても箱は動かないので、
// 目標の座標は節点の数だけで決まる。辺は見ない (配列に辺は無い)。
//
// 入れ替えを見せるのは swapNodePositions の役目。今いる座標だけを入れ替えると、
// EasedLayout が元の位置へ戻す間に「値が移動した」ように見える。
//
// perRow を決めると段に分ける。マージソートのように作業用の場所が要るものが、
// 元の配列を上の段、作業用を下の段として使う。
class LineLayout : public EasedLayout {
public:
    static constexpr float CELL_GAP = 8.0f;  // 隣り合う箱の縁どうしの間隔
    static constexpr float ROW_GAP  = 70.0f; // 段と段の間隔

    // 1段に並べる数。0 なら段に分けず全部を1列にする
    void setPerRow(int count) { perRow = count; }

private:
    int perRow = 0;

protected:
    void computeTargets(GraphData* graph) override {
        targetX.assign(nodeSize, 0.0f);
        targetY.assign(nodeSize, 0.0f);

        int width = perRow > 0 ? perRow : nodeSize;
        if (width <= 0) return;

        // 段ごとに幅が違うと縦に揃わないので、x は段の中の位置だけで決める
        float x = 0.0f;
        for (int i = 0; i < nodeSize; i++) {
            int column = i % width;
            if (column == 0) x = 0.0f;
            float half = graph->halfWidthOf(i);
            x += half;
            targetX[i] = x;
            targetY[i] = (float)(i / width) * ROW_GAP;
            x += half + CELL_GAP;
        }
    }
};
