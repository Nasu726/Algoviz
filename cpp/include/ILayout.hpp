#pragma once
#include "GraphData.hpp"
#include <vector>

// 頂点の座標を決める役。GraphVisualizer はこの面だけを見る。
//
// 一般グラフは力学モデル (GeneralGraphLayout)、木は Reingold-Tilford のように
// 配置の決め方がまるで違うので、差し替えられる形にしてある。
//
// 呼ばれ方は「init して、update を収束するまで毎フレーム回す」。
// 描画ループが update を回し、時間切れになったら finish で打ち切る。
class ILayout {
public:
    virtual ~ILayout() = default;

    // adj は向きを無視し自己ループを除いた隣接リスト。
    // レイアウトは辺の向きに意味を持たないので、呼び出し側 (GraphVisualizer) が
    // 一度だけ構築したものを共有する。
    virtual void init(GraphData* graph, const std::vector<std::vector<int>>& adj) = 0;

    // 1フレーム進める。収束したら true。
    virtual bool update(GraphData* graph) = 0;

    // 時間切れ。今の座標のまま最終処理をして収束扱いにする。
    // WASM はメインスレッドで同期実行されるので、収束を待ち続けるとタブが固まる。
    virtual void finish(GraphData* graph) = 0;

    virtual bool isStable() const = 0;

    // 収束をやり直させる。グラフが差し替わったときに呼ぶ。
    virtual void invalidate() = 0;

    // 縦長 / 横長のどちらに伸ばしたいか。指向性を持たないレイアウトは無視してよい。
    virtual void setPreferHorizontal(bool) {}
};
