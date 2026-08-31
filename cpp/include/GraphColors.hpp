#pragma once

// GraphData の colorId が取る値。
//
// ここで決めるのは「意味」だけで、実際の色は JS 側 (PixiGraphApp) が持つ。
// 「アルゴリズムの論理は C++、見た目は JS」という分担に合わせている。
// 値を足すときは PixiGraphApp のパレットも同じ順で伸ばすこと。

enum NodeColor : int {
    NODE_DEFAULT  = 0, // 未訪問
    NODE_FRONTIER = 1, // フロンティア（キュー / スタックの中）
    NODE_VISITING = 2, // 今まさに処理している
    NODE_VISITED  = 3, // 訪問済み
    NODE_PATH     = 4, // 見つかった経路の上
    NODE_START    = 5, // 始点
    NODE_GOAL     = 6, // 終点
    NODE_COLOR_COUNT
};

enum EdgeColor : int {
    EDGE_DEFAULT = 0, // 通常
    EDGE_TREE    = 1, // 探索木の辺
    EDGE_ACTIVE  = 2, // 今たどっている
    EDGE_VISITED = 3, // 調べ終わった
    EDGE_PATH    = 4, // 見つかった経路の上
    EDGE_COLOR_COUNT
};
