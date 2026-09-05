#pragma once
#include "GraphData.hpp"
#include "ILayout.hpp"
#include <vector>
#include <algorithm>
#include <cmath>

// 木を上から下へ描く配置 (Reingold-Tilford)。
//
// 後順に部分木を組み立て、隣り合う部分木は輪郭 (contour) を突き合わせて
// ぶつからない最小の間隔まで寄せる。親は子たちの中央に置く。
//
// 各深さの「使用済み x の最大値」だけで済ませる簡易版もあるが、
// 親が子の中央から外れるのが目に見えて分かるので採らない。
// 頂点数の上限は 50 なので、輪郭の突き合わせが O(n * 深さ) でも問題にならない。
//
// 木の向きは常に上から下。preferHorizontal は木には意味が無いので上書きしない。
class TreeLayout : public ILayout {
public:
    static constexpr float LEVEL_GAP   = 90.0f;  // 深さ1つぶんの縦の間隔
    // 隣り合う節点の縁どうしの最小の間隔。節点ごとに幅が違うので、
    // 中心の間隔ではなく縁の間隔で持つ。
    static constexpr float SIBLING_GAP = 30.0f;
    static constexpr float TREE_GAP    = 120.0f; // 森にしたときの木と木の間隔

private:
    // 目標へ寄せる割合。指数的に減衰するので、所要フレームは
    // log(1/(1-EASE)) に反比例する。
    // 0.3 -> 0.277 で 1.1 倍、0.277 -> 0.221 でさらに 1.3 倍、
    // 0.221 -> 0.203 でさらに 1.1 倍かかる。
    static constexpr float EASE    = 0.203f;
    static constexpr float EPSILON = 0.5f;

    bool stable = false;
    int nodeSize = 0;

    std::vector<float> targetX, targetY;

    std::vector<std::vector<int>> children;
    // 実際に部分木として辿った子。閉路や合流があると children とは一致しないので、
    // 座標を配るときはこちらを使う。
    std::vector<std::vector<int>> laidChildren;
    std::vector<int> roots;

    std::vector<float> prelim;    // 親から見た相対 x
    std::vector<float> halfWidth; // 節点ごとの半幅
    std::vector<char> seen;       // 部分木として組み立て済み
    std::vector<char> placed;     // 絶対座標を配り済み

    // 辺の向きをそのまま親子関係として読む。木は向きに意味があるので、
    // 呼び出し側がくれる無向の隣接リストは使わない。
    //
    // 左右の並びは辺の3列目で決める。辺は木が育った順に増えるので、
    // 追加順のままだと二分探索木で小さい値が右に来ることがある。
    void buildChildren(GraphData* graph) {
        children.assign(nodeSize, {});
        std::vector<std::vector<std::pair<float, int>>> ordered(nodeSize);
        std::vector<int> indeg(nodeSize, 0);

        for (int i = 0; i < graph->edgeCount(); i++) {
            int from = graph->edgeFrom(i), to = graph->edgeTo(i);
            if (from < 0 || from >= nodeSize || to < 0 || to >= nodeSize) continue;
            if (from == to) continue;
            float order = graph->edgeData[(std::size_t)i * GraphData::EDGE_STRIDE + 2];
            ordered[from].push_back({order, to});
            indeg[to]++;
        }

        for (int i = 0; i < nodeSize; i++) {
            // 同じ並び順の値どうしは辺の追加順を保つ
            std::stable_sort(ordered[i].begin(), ordered[i].end(),
                             [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
                                 return a.first < b.first;
                             });
            for (const auto& p : ordered[i]) children[i].push_back(p.second);
        }

        roots.clear();
        for (int i = 0; i < nodeSize; i++) if (indeg[i] == 0) roots.push_back(i);
    }

    // 部分木を組み立て、その輪郭 (深さごとの左端 / 右端) を返す。
    // 座標はこの部分木の根を 0 とした相対値。
    void layoutSubtree(int u, std::vector<float>& lc, std::vector<float>& rc) {
        seen[u] = 1;
        prelim[u] = 0.0f;

        std::vector<int>& kids = laidChildren[u];
        kids.clear();
        for (int c : children[u]) if (!seen[c]) kids.push_back(c);

        // 輪郭は節点の縁。幅を入れておくと、寄せる量の計算がそのまま
        // 「縁どうしが SIBLING_GAP 以上あく」を意味するようになる。
        lc.assign(1, -halfWidth[u]);
        rc.assign(1,  halfWidth[u]);
        if (kids.empty()) return;

        std::vector<float> accL, accR; // 置き終えた兄弟たちの輪郭
        std::vector<float> offset(kids.size(), 0.0f);

        for (std::size_t i = 0; i < kids.size(); i++) {
            std::vector<float> cl, cr;
            layoutSubtree(kids[i], cl, cr);

            // 既に置いた兄弟の右端と、この部分木の左端が SIBLING_GAP 以上あく
            // ところまで寄せる。重なるすべての深さで見るのが輪郭の突き合わせ。
            float shift = 0.0f;
            if (i > 0) {
                std::size_t overlap = std::min(accR.size(), cl.size());
                for (std::size_t d = 0; d < overlap; d++) {
                    shift = std::max(shift, accR[d] + SIBLING_GAP - cl[d]);
                }
            }
            offset[i] = shift;

            for (std::size_t d = 0; d < cl.size(); d++) {
                if (d < accL.size()) {
                    accL[d] = std::min(accL[d], shift + cl[d]);
                    accR[d] = std::max(accR[d], shift + cr[d]);
                } else {
                    accL.push_back(shift + cl[d]);
                    accR.push_back(shift + cr[d]);
                }
            }
        }

        // 親は端の子2つの中央。子の相対位置をその分だけずらす
        float mid = (offset.front() + offset.back()) / 2.0f;
        for (std::size_t i = 0; i < kids.size(); i++) prelim[kids[i]] = offset[i] - mid;

        for (std::size_t d = 0; d < accL.size(); d++) {
            lc.push_back(accL[d] - mid);
            rc.push_back(accR[d] - mid);
        }
        // 親自身の幅も輪郭に入れる。子より広いことがある (B木)
        lc[0] = std::min(lc[0], -halfWidth[u]);
        rc[0] = std::max(rc[0],  halfWidth[u]);
    }

    void assignAbsolute(int u, float x, int depth) {
        placed[u] = 1;
        targetX[u] = x;
        targetY[u] = (float)depth * LEVEL_GAP;
        for (int c : laidChildren[u]) assignAbsolute(c, x + prelim[c], depth + 1);
    }

    // どの根からも辿れなかった節点 (閉路の中など) は右に一列で置く
    void placeLeftovers(float startX) {
        float x = startX;
        for (int i = 0; i < nodeSize; i++) {
            if (placed[i]) continue;
            targetX[i] = x;
            targetY[i] = 0.0f;
            x += halfWidth[i] * 2.0f + SIBLING_GAP;
        }
    }

    void computeTargets(GraphData* graph) {
        buildChildren(graph);
        halfWidth.assign(nodeSize, GraphData::DEFAULT_HALF_WIDTH);
        for (int i = 0; i < nodeSize; i++) halfWidth[i] = graph->halfWidthOf(i);
        laidChildren.assign(nodeSize, {});
        prelim.assign(nodeSize, 0.0f);
        targetX.assign(nodeSize, 0.0f);
        targetY.assign(nodeSize, 0.0f);
        seen.assign(nodeSize, 0);
        placed.assign(nodeSize, 0);

        float cursor = 0.0f;
        for (int r : roots) {
            if (seen[r]) continue;
            std::vector<float> lc, rc;
            layoutSubtree(r, lc, rc);

            // 木の外形は長方形なので、横にずらして並べれば重ならない。
            // 一般グラフのような凸包パッキングは要らない。
            float minL = *std::min_element(lc.begin(), lc.end());
            float maxR = *std::max_element(rc.begin(), rc.end());

            assignAbsolute(r, cursor - minL, 0);
            cursor += (maxR - minL) + TREE_GAP;
        }

        placeLeftovers(cursor);
    }

public:
    void init(GraphData* graph, const std::vector<std::vector<int>>& adj) override {
        (void)adj; // 木は辺の向きを見るので、無向の隣接リストは使わない
        nodeSize = graph->nodeCount();
        stable = false;
        if (nodeSize == 0) {
            stable = true;
            return;
        }
        computeTargets(graph);
    }

    // 目標の座標へ少しずつ寄せる。一気に飛ばすと挿入のたびに節点が瞬間移動する。
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
