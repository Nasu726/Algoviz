#ifndef GRAPH_VISUALIZER_HPP
#define GRAPH_VISUALIZER_HPP

#pragma once
#include "IVisualizer.hpp"
#include "GraphData.hpp"
#include "GeneralGraphLayout.hpp"
#include <emscripten/val.h>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <random>

using namespace emscripten;

class GraphVisualizer : public IVisualizer {
private:
    GraphData* graph;
    GeneralGraphLayout layout;
    bool skipExtension = true;

    // グラフを新しく作り直すヘルパー関数
    void generateRandom(int v, int e, bool allowSelfLoop, bool allowSameEdge,bool isDirected) {
        delete graph;
        graph = new GraphData(v, e);
        for (int i = 0; i < v; i++) {
            graph->setNode(i, rand() % 600 + 100, rand() % 400 + 100, 0, 0);
        }

        std::vector<std::pair<int, int>> possibleEdges;
        for (int i = 0; i < v; i++) {
            for (int j = (isDirected ? 0 : i); j < v; j++) {
                if (!allowSelfLoop && i == j) continue;
                possibleEdges.push_back({i, j});
            }
        }

        if (possibleEdges.empty()) return;

        if (allowSameEdge) {
            for (int i = 0; i < e; i++) {
                int idx = rand() % possibleEdges.size();
                float weight = rand() % 100;
                graph->addEdge(possibleEdges[idx].first, possibleEdges[idx].second, weight, 0);
            }
        } else {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(possibleEdges.begin(), possibleEdges.end(), g);

            int actualEdges = std::min((int)possibleEdges.size(), e);

            for (int i = 0; i < actualEdges; i++) {
                float weight = rand() % 100;
                graph->addEdge(possibleEdges[i].first, possibleEdges[i].second, weight, 0);
            }
        }
    }

    void generateComplete(int v, bool isDirected) {
        delete graph;
        int e = v * (v - 1) / 2;
        graph = new GraphData(v, e);
        for (int i = 0; i < v; i++) {
            graph->setNode(i, rand() % 600 + 100, rand() % 400 + 100, 0, 0);
        }
        for (int i = 0; i < v; i++) {
            for (int j = (isDirected ? 0 : i + 1); j < v; j++) {
                if (i == j) continue;
                float weight = rand() % 100;
                graph->addEdge(i, j, weight, 0);
            }
        }
    }

    void generateCustomGraph() {
        return;
    }

public:
    GraphVisualizer() {
        srand((unsigned int)time(nullptr));
        graph = nullptr;
        generateRandom(5, 7, false, false, false);
        layout.init(graph);
    }

    ~GraphVisualizer() {
        delete graph;
    }

    void load(const std::string& source, const std::string& input) override {
        // source 引数を使って向きを設定
        if (source == "horizontal")    layout.preferHorizontal = true;
        else if (source == "vertical") layout.preferHorizontal = false;
        else if (source == "setStartNode") {
            int startIdx;
            std::istringstream iss(input);
            if (iss >> startIdx && graph) {
                graph->startNodeIndex = startIdx;
            }
            return;
        }

        // Reactから送られたコマンドの解析
        if (!input.empty()) {
            std::istringstream iss(input);
            std::string cmd;
            iss >> cmd;
            if (cmd == "random") {
                int v, e, skip = 0, selfLoop = 0, sameEdge = 0, isDir = 0;
                iss >> v >> e;
                if (iss >> skip) skipExtension = (skip != 0);
                if (iss >> selfLoop >> sameEdge >> isDir) {
                    generateRandom(v, e, selfLoop != 0, sameEdge != 0, isDir != 0);
                } else {
                    generateRandom(v, e, false, false, false);
                }
                layout.init(graph);
            } else if (cmd == "complete") {
                int v, skip = 0, isDir = 0;
                iss >> v;
                if (iss >> skip) skipExtension = (skip != 0);
                if (iss >> isDir) {
                    generateComplete(v, isDir != 0);
                } else {
                    generateComplete(v, false);
                }
                layout.init(graph);
            } else if (cmd == "custom") {
                int v, e, skip = 0;
                if (iss >> v >>e) {
                    delete graph;
                    graph = new GraphData(v, e);
                    for (int i = 0; i < v; i++) {
                        graph->setNode(i, i, i, 0, 0);
                    }
                    for (int i = 0; i < e; i++) {
                        int from, to;
                        float weight = 0;
                        if (iss >> from >> to >> weight) {
                            graph->addEdge(from, to, weight, 0);
                        }
                    }
                    if (iss >> skip) skipExtension = (skip != 0);
                    layout.init(graph);
                }
            }
        }
        
        layout.is_stable = false;
    }

    bool step() override {
        if (skipExtension) {
            int frame = 0, limit = 3000;
            bool skipped = false;
            while (!layout.update(graph) && frame < limit) {
                frame++;
                skipped = true;
            }
            if (skipped) layout.forcePack(graph); // 時間切れで終わった場合にパッキングする
        } else {
            layout.update(graph);
        }
        return true;
    }

    void stepBack() override {
        // グラフの1手戻る処理（後で実装）
    }

    val getState(val params) override {
        // 状態として GraphData のインスタンスをそのままJSに渡す
        val state = val::object();
        if (graph) {
            state.set("nodes", graph->getNodeView());
            state.set("edges", graph->getEdgeView());

            std::ostringstream oss;
            int v = graph->nodeData.size() / graph->NODE_STRIDE;
            int e = graph->edgeData.size() / graph->EDGE_STRIDE;
            oss << v << " " << e << "\n";
            for (int i = 0; i < e; i++) {
                int from = graph->edgeData[i * graph->EDGE_STRIDE];
                int to   = graph->edgeData[i * graph->EDGE_STRIDE + 1];
                float weight = graph->edgeData[i * graph->EDGE_STRIDE + 2];
                oss << from << " " << to << " " << weight << "\n";
            }
            state.set("graphText", oss.str());
        }

        return state;
    }

    std::string getOutput() override {
        return "";
    }
};

#endif