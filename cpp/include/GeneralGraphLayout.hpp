#ifndef GENERAL_GRAPH_LAYOUT_HPP
#define GENERAL_GRAPH_LAYOUT_HPP

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
    std::vector<bool> is_circular_layout;
    std::vector<float> target_nx;
    std::vector<float> target_ny;

    // ==========================================
    // 輪郭（凸包）の抽出
    // ==========================================

    struct Point2D {
        int id;
        float x, y;
    };

    // 3点 O, A, B が作るベクトルの外積（Z成分）
    // 正なら反時計回り(左折)、負なら時計回り(右折)、0なら同一直線上
    float crossProduct(const Point2D& O, const Point2D& A, const Point2D& B) {
        return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
    }

    // コンポーネントの頂点群から凸包（輪郭の多角形）を抽出する (Andrew's Monotone Chain)
    std::vector<Point2D> getConvexHull(const std::vector<int>& component, GraphData* graph) {
        int n = component.size();
        if (n <= 2) {
            // 頂点が2個以下の場合は、そのまま返す
            std::vector<Point2D> hull;
            for (int id : component) {
                hull.push_back({id, graph->nodeData[id * nodeStride], graph->nodeData[id * nodeStride + 1]});
            }
            return hull;
        }

        std::vector<Point2D> P(n);
        for (int i = 0; i < n; i++) {
            int id = component[i];
            P[i] = {id, graph->nodeData[id * nodeStride], graph->nodeData[id * nodeStride + 1]};
        }

        // X座標でソート（Xが同じならYでソート）
        std::sort(P.begin(), P.end(), [](const Point2D& a, const Point2D& b) {
            return a.x < b.x || (a.x == b.x && a.y < b.y);
        });

        std::vector<Point2D> hull(2 * n);
        int k = 0;

        // 下側凸包の構築
        for (int i = 0; i < n; i++) {
            while (k >= 2 && crossProduct(hull[k - 2], hull[k - 1], P[i]) <= 0) k--;
            hull[k++] = P[i];
        }

        // 上側凸包の構築
        for (int i = n - 2, t = k + 1; i >= 0; i--) {
            while (k >= t && crossProduct(hull[k - 2], hull[k - 1], P[i]) <= 0) k--;
            hull[k++] = P[i];
        }

        // 最後の頂点は最初の頂点と重複するので削る
        hull.resize(k - 1);
        return hull;
    }

    // ==========================================
    // パッキング準備 (バウンディング計算とソート)
    // ==========================================

    struct ComponentShape {
        int id; // components配列のインデックス
        std::vector<Point2D> hull;
        float cx, cy;
        float radius;
    };

    std::vector<ComponentShape> comp_shapes; // パッキング用の形状リスト

    void preparePacking(GraphData* graph) {
        comp_shapes.clear();

        for (size_t c = 0; c < components.size(); c++) {
            if (components[c].empty()) continue;

            ComponentShape shape;
            shape.id = c;
            shape.hull = getConvexHull(components[c], graph);

            // 1. 重心（バウンディングボックスの中心）を計算
            float min_x = inf, max_x = -inf;
            float min_y = inf, max_y = -inf;
            for (int u : components[c]) {
                float x = graph->nodeData[u * nodeStride];
                float y = graph->nodeData[u * nodeStride + 1];
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
            }
            shape.cx = (min_x + max_x) / 2.0f;
            shape.cy = (min_y + max_y) / 2.0f;

            // ノードの半径分、輪郭を中心から外側に押し出す
            float margin = 50.0f; // ノード半径(20) + 隙間(70)
            for (auto& pt : shape.hull) {
                float dx = pt.x - shape.cx;
                float dy = pt.y - shape.cy;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > 0.001f) {
                    pt.x += (dx / dist) * margin;
                    pt.y += (dy / dist) * margin;
                }
            }

            // 2. 半径（中心から最も遠い頂点までの距離）を計算
            float max_dist_sq = 0.0f;
            for (int u : components[c]) {
                float x = graph->nodeData[u * nodeStride];
                float y = graph->nodeData[u * nodeStride + 1];
                float dx = x - shape.cx;
                float dy = y - shape.cy;
                max_dist_sq = std::max(max_dist_sq, dx * dx + dy * dy);
            }
            
            // margin を余白として足す
            shape.radius = std::sqrt(max_dist_sq) + margin; 

            comp_shapes.push_back(shape);
        }

        // 3. 半径が大きい順（降順）にソート！
        std::sort(comp_shapes.begin(), comp_shapes.end(), [](const ComponentShape& a, const ComponentShape& b) {
            return a.radius > b.radius;
        });
    }

    // ==========================================
    // 厳密パッキング (多角形交差判定 + 螺旋探索)
    // ==========================================

    // 線分上に点があるか
    bool onSegment(Point2D p, Point2D q, Point2D r) {
        return q.x <= std::max(p.x, r.x) && q.x >= std::min(p.x, r.x) &&
               q.y <= std::max(p.y, r.y) && q.y >= std::min(p.y, r.y);
    }

    // 線分 p1-q1 と p2-q2 が交差しているか判定
    bool doIntersect(Point2D p1, Point2D q1, Point2D p2, Point2D q2) {
        float o1 = crossProduct(p1, q1, p2);
        float o2 = crossProduct(p1, q1, q2);
        float o3 = crossProduct(p2, q2, p1);
        float o4 = crossProduct(p2, q2, q1);

        if (((o1 > 0 && o2 < 0) || (o1 < 0 && o2 > 0)) &&
            ((o3 > 0 && o4 < 0) || (o3 < 0 && o4 > 0))) return true;

        if (o1 == 0 && onSegment(p1, p2, q1)) return true;
        if (o2 == 0 && onSegment(p1, q2, q1)) return true;
        if (o3 == 0 && onSegment(p2, p1, q2)) return true;
        if (o4 == 0 && onSegment(p2, q1, q2)) return true;
        return false;
    }

    // 点が多角形の中にあるか（Ray Casting法）
    bool isPointInPolygon(Point2D pt, const std::vector<Point2D>& poly, float offsetX, float offsetY) {
        int n = poly.size();
        if (n < 3) return false;
        bool inside = false;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            float xi = poly[i].x + offsetX, yi = poly[i].y + offsetY;
            float xj = poly[j].x + offsetX, yj = poly[j].y + offsetY;
            if (((yi > pt.y) != (yj > pt.y)) &&
                (pt.x < (xj - xi) * (pt.y - yi) / (yj - yi) + xi)) {
                inside = !inside;
            }
        }
        return inside;
    }

    // 2つのコンポーネントが衝突（重なり）しているかを判定
    bool checkOverlap(const ComponentShape& s1, float dx1, float dy1,
                      const ComponentShape& s2, float dx2, float dy2) {
        // 【枝刈り1: 外接円による超高速判定】
        float distSq = ((s1.cx + dx1) - (s2.cx + dx2)) * ((s1.cx + dx1) - (s2.cx + dx2)) +
                       ((s1.cy + dy1) - (s2.cy + dy2)) * ((s1.cy + dy1) - (s2.cy + dy2));
        float rSum = s1.radius + s2.radius;
        if (distSq >= rSum * rSum) return false; // 円が重なっていないなら絶対に安全！

        // 【枝刈り2: 頂点が少なすぎる場合は円判定のみで妥協（1~2頂点のグラフ用）】
        int n1 = s1.hull.size();
        int n2 = s2.hull.size();
        if (n1 < 3 || n2 < 3) return true; // 円が重なっていて、かつ多角形じゃないなら衝突とする

        // 【詳細判定: 凸包の線分交差テスト】
        for (int i = 0; i < n1; i++) {
            Point2D p1 = {0, s1.hull[i].x + dx1, s1.hull[i].y + dy1};
            Point2D q1 = {0, s1.hull[(i+1)%n1].x + dx1, s1.hull[(i+1)%n1].y + dy1};
            for (int j = 0; j < n2; j++) {
                Point2D p2 = {0, s2.hull[j].x + dx2, s2.hull[j].y + dy2};
                Point2D q2 = {0, s2.hull[(j+1)%n2].x + dx2, s2.hull[(j+1)%n2].y + dy2};
                if (doIntersect(p1, q1, p2, q2)) return true; // 境界線が交差している
            }
        }

        // 【詳細判定: 完全に内包されているかのテスト】
        if (isPointInPolygon({0, s1.hull[0].x + dx1, s1.hull[0].y + dy1}, s2.hull, dx2, dy2)) return true;
        if (isPointInPolygon({0, s2.hull[0].x + dx2, s2.hull[0].y + dy2}, s1.hull, dx1, dy1)) return true;

        return false;
    }

    // 螺旋探索による厳密パッキングの実行
    void packComponentsStrict(GraphData* graph) {
        if (comp_shapes.empty()) return;

        float center_x = 400.0f; // 画面中心
        float center_y = 300.0f;

        std::vector<float> final_dx(components.size(), 0.0f);
        std::vector<float> final_dy(components.size(), 0.0f);

        // 1番巨大なコンポーネントは無条件で画面ド真ん中に配置
        ComponentShape& first = comp_shapes[0];
        final_dx[first.id] = center_x - first.cx;
        final_dy[first.id] = center_y - first.cy;

        // 残りのコンポーネントを配置していく
        for (size_t i = 1; i < comp_shapes.size(); i++) {
            ComponentShape& current = comp_shapes[i];
            bool placed = false;
            float step = 4.0f; // 探索の粗さ（小さいほど高密度になるが少し重い）

            // 画面中心から螺旋状（スパイラル）に空き地を探索
            for (float r = step; r < 5000.0f && !placed; r += step) {
                float d_theta = std::max(0.1f, step / r); // 外側に行くほど角度の刻みを細かくする
                for (float theta = 0; theta < 2.0f * (float)M_PI; theta += d_theta) {
                    float test_dx = (center_x + r * std::cos(theta)) - current.cx;
                    float test_dy = (center_y + r * std::sin(theta)) - current.cy;

                    bool overlap = false;
                    // すでに配置済みの「自分より大きいパーツ群」すべてと衝突判定
                    for (size_t j = 0; j < i; j++) {
                        ComponentShape& placed_comp = comp_shapes[j];
                        if (checkOverlap(current, test_dx, test_dy, placed_comp, final_dx[placed_comp.id], final_dy[placed_comp.id])) {
                            overlap = true;
                            break;
                        }
                    }

                    // どこにもぶつからなかったら、そこを安住の地とする！
                    if (!overlap) {
                        final_dx[current.id] = test_dx;
                        final_dy[current.id] = test_dy;
                        placed = true;
                        break;
                    }
                }
            }
        }

        // 確定したオフセットを実際のノード座標に適用してピタッと止める
        for (size_t i = 0; i < comp_shapes.size(); i++) {
            int c_id = comp_shapes[i].id;
            float dx = final_dx[c_id];
            float dy = final_dy[c_id];
            
            for (int u : components[c_id]) {
                target_nx[u] += dx;
                target_ny[u] += dy;
                // 一瞬で所定の位置にスナップさせる
                graph->nodeData[u * nodeStride] = target_nx[u];
                graph->nodeData[u * nodeStride + 1] = target_ny[u];
            }
        }
    }

    // ==========================================
    // フェーズ2: 古典的MDSによる初期配置
    // ==========================================

    // 実対称行列の固有値分解を行うヤコビ法 (Jacobi Method)
    void jacobiMethod(const std::vector<std::vector<float>>& A, std::vector<float>& eigenvalues, std::vector<std::vector<float>>& eigenvectors) {
        int n = A.size();
        std::vector<std::vector<float>> mat = A;
        eigenvectors.assign(n, std::vector<float>(n, 0.0f));
        for (int i = 0; i < n; i++) eigenvectors[i][i] = 1.0f;

        int max_iter = 100 * n * n; 
        float epsilon_jacobi = 1e-5f;

        for (int iter = 0; iter < max_iter; iter++) {
            float max_val = 0.0f;
            int p = 0, q = 1;
            // 非対角要素の最大値を探す
            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    if (std::abs(mat[i][j]) > max_val) {
                        max_val = std::abs(mat[i][j]);
                        p = i;
                        q = j;
                    }
                }
            }

            if (max_val < epsilon_jacobi) break; // 収束

            // ギブンス回転の角度を計算
            float theta;
            if (std::abs(mat[p][p] - mat[q][q]) < 1e-9f) {
                theta = (mat[p][q] > 0) ? M_PI / 4.0f : -M_PI / 4.0f;
            } else {
                theta = 0.5f * std::atan2(2.0f * mat[p][q], mat[p][p] - mat[q][q]);
            }

            float c = std::cos(theta);
            float s = std::sin(theta);

            // 行列の更新
            float app = mat[p][p], aqq = mat[q][q], apq = mat[p][q];
            mat[p][p] = c * c * app - 2.0f * s * c * apq + s * s * aqq;
            mat[q][q] = s * s * app + 2.0f * s * c * apq + c * c * aqq;
            mat[p][q] = mat[q][p] = 0.0f;

            for (int i = 0; i < n; i++) {
                if (i != p && i != q) {
                    float aip = mat[i][p], aiq = mat[i][q];
                    mat[i][p] = mat[p][i] = c * aip - s * aiq;
                    mat[i][q] = mat[q][i] = s * aip + c * aiq;
                }
            }

            // 固有ベクトルの更新
            for (int i = 0; i < n; i++) {
                float eip = eigenvectors[i][p], eiq = eigenvectors[i][q];
                eigenvectors[i][p] = c * eip - s * eiq;
                eigenvectors[i][q] = s * eip + c * eiq;
            }
        }

        eigenvalues.resize(n);
        for (int i = 0; i < n; i++) eigenvalues[i] = mat[i][i];
    }

    // ==========================================
    // フェーズ2: 密度に基づくハイブリッド初期配置
    // ==========================================
    float getDensityThreshold(float V, int maxK) {
        if (V <= 1) return 1.0;
        const float C = 3; // 許容平均次数
        float density_threshold = C / (V - 1) * std::min((float)1.0, C / maxK);
        return std::min((float)1.0, density_threshold);
    }

    void applySmartInitialLayout(GraphData* graph, const std::vector<std::vector<int>>& adj) {
        float center_x = 400.0f; // キャンバスの中心X
        float center_y = 300.0f; // キャンバスの中心Y

        for (size_t c = 0; c < components.size(); c++) {
            int n = components[c].size();
            
            // 頂点が2個以下の場合は直線配置
            if (n <= 2) {
                for (int i = 0; i < n; i++) {
                    int u = components[c][i];
                    target_nx[u] = center_x + i * baseDistance - (n - 1) * baseDistance / 2.0f;
                    target_ny[u] = center_y;
                    graph->nodeData[u * nodeStride] = target_nx[u];
                    graph->nodeData[u * nodeStride + 1] = target_ny[u];
                }
                continue;
            }

            // 1. コンポーネント内の辺の数 (E) をカウント
            int comp_edges = 0;
            int maxK = 0;
            for (int u : components[c]) {
                int k_count = 0;
                for (int v : adj[u]) {
                    // 両方の頂点がこのコンポーネントに属しているか（通常は属している）
                    // 無向グラフの重複カウントを防ぐため u < v のみカウント
                    if (u < v) {
                        comp_edges++; 
                        k_count++;
                    }
                }
                maxK = std::max(maxK, k_count);
            }

            // 2. 密度の計算: D = 2E / V(V-1)
            float density = (2.0f * comp_edges) / (n * (n - 1.0f));

            // ==================================================
            // 高密度グラフ -> 円状配置 (Circular Layout)
            // ==================================================
            if (density >= getDensityThreshold(n, maxK)) {
                is_circular_layout[c] = true;
                // ★ PixiJS側の nodeRadius (20.0) を基準に、重ならない半径を計算
                float nodeRadius = 20.0f;
                float padding = 10.0f; // ノード同士の隙間（お好みで調整）
                
                // ノードが重ならないために必要な円周 = 頂点数 × (直径 + 隙間)
                float requiredCircumference = n * (nodeRadius * 2.0f + padding);
                
                // 円周 = 2πr より、必要な半径 r を逆算
                float minRadius = requiredCircumference / (2.0f * (float)M_PI);

                // 理想距離 (baseDistance) から求めた半径と比べて、大きい方を採用する
                float defaultRadius = (n * baseDistance) / (2.0f * (float)M_PI);
                float radius = std::max({baseDistance, defaultRadius, minRadius});

                for (int i = 0; i < n; i++) {
                    int u = components[c][i];
                    float angle = 2.0f * M_PI * i / n;
                    float x = center_x + radius * std::cos(angle);
                    float y = center_y + radius * std::sin(angle);

                    target_nx[u] = x;
                    target_ny[u] = y;
                    graph->nodeData[u * nodeStride] = x;
                    graph->nodeData[u * nodeStride + 1] = y;
                }
                continue; // MDSの計算をスキップして次へ
            }

            // ==================================================
            // 通常グラフ -> 古典的MDS
            // ==================================================
            std::vector<std::vector<float>> D2(n, std::vector<float>(n, 0.0f));
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    int u = components[c][i];
                    int v = components[c][j];
                    float dist = d[u * nodeSize + v];
                    D2[i][j] = dist * dist;
                }
            }

            std::vector<float> row_mean(n, 0.0f);
            float total_mean = 0.0f;
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) row_mean[i] += D2[i][j];
                total_mean += row_mean[i];
                row_mean[i] /= n;
            }
            total_mean /= (n * n);

            std::vector<std::vector<float>> B(n, std::vector<float>(n, 0.0f));
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    B[i][j] = -0.5f * (D2[i][j] - row_mean[i] - row_mean[j] + total_mean);
                }
            }

            std::vector<float> eigenvalues;
            std::vector<std::vector<float>> eigenvectors;
            jacobiMethod(B, eigenvalues, eigenvectors);

            std::vector<int> indices(n);
            for(int i = 0; i < n; i++) indices[i] = i;
            std::sort(indices.begin(), indices.end(), [&](int a, int b){
                return eigenvalues[a] > eigenvalues[b];
            });

            float lambda1 = std::max(0.0f, eigenvalues[indices[0]]);
            float lambda2 = std::max(0.0f, eigenvalues[indices[1]]);
            float sqrt_l1 = std::sqrt(lambda1);
            float sqrt_l2 = std::sqrt(lambda2);

            for (int i = 0; i < n; i++) {
                int u = components[c][i];
                float x = eigenvectors[i][indices[0]] * sqrt_l1 + center_x;
                float y = eigenvectors[i][indices[1]] * sqrt_l2 + center_y;
                
                if (x != x || y != y) {
                    x = center_x + ((rand() % 100) - 50) / 10.0f;
                    y = center_y + ((rand() % 100) - 50) / 10.0f;
                }

                target_nx[u] = x;
                target_ny[u] = y;
                graph->nodeData[u * nodeStride] = x;
                graph->nodeData[u * nodeStride + 1] = y;
            }
        }
    }

    // ==========================================
    // 内部処理用の分割関数群
    // ==========================================

    // SMアルゴリズムの計算
    void calculateStressMajorization(GraphData* graph) {
        for(size_t c = 0; c < components.size(); c++) {
            if (is_circular_layout[c]) continue;
            
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

    // グローバル斥力（密グラフを円形に押し広げる）
    void applyNodeRepulsion() {
        float k = baseDistance; // 基準距離
        
        for(size_t c = 0; c < components.size(); c++) {
            if (is_circular_layout[c]) continue;

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
                    float force = (k * k) / dist * 0.1f; // 0.03f は引力とのバランス係数
                    
                    float fx = (dx/dist) * force;
                    float fy = (dy/dist) * force;
                    target_nx[u] += fx; target_ny[u] += fy;
                    target_nx[v] -= fx; target_ny[v] -= fy;
                }
            }
        }
    }

    // 指向性（縦長・横長）に合わせた回転
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

        is_circular_layout.assign(components.size(), false);
        applySmartInitialLayout(graph, adj);
    }

    // update は各関数を呼ぶだけ
    bool update(GraphData* graph) {
        if (is_stable) return true;

        // 1. 各コンポーネント内の形を整える
        calculateStressMajorization(graph);

        // 2. ノードの絡まりや縮こまりを解消するために斥力を与える。
        applyNodeRepulsion();
        
        // 3. 縦横の指向性に合わせて回転する
        updateComponentOrientations(graph);

        // 4. イージングをかけて実際の座標を動かし、収束を判定する
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

        // 5. 収束判定のアップデート（忍耐カウンター）
        if (max_movement < epsilon) {
            stable_count++;
            if (stable_count >= required_stable_frames) {
                is_stable = true;

                // 完全に静止した瞬間に、1回だけ厳密パッキングを実行
                preparePacking(graph);       // バウンディング計算とソート
                packComponentsStrict(graph); // 螺旋探索による多角形パッキング
            }
        } else {
            stable_count = 0; // 少しでも大きく動いたら最初から数え直し！
        }

        return is_stable;
    }
};

#endif