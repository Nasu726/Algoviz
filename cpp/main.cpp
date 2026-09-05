#include <emscripten/bind.h>
#include <memory>
#include <iostream>
#include "include/IVisualizer.hpp"
#include "include/Brainfuck.hpp"
#include "include/GraphVisualizer.hpp"
#include "include/AutomatonVisualizer.hpp"
#include "include/TraversalVisualizer.hpp"
#include "include/BstVisualizer.hpp"
#include "include/HeapVisualizer.hpp"
#include "include/TrieVisualizer.hpp"
#include "include/HuffmanVisualizer.hpp"
#include "include/AvlVisualizer.hpp"
#include "include/BTreeVisualizer.hpp"
#include "include/BubbleSortVisualizer.hpp"

using namespace emscripten;

class VisualizerEngine {
private:
    std::unique_ptr<IVisualizer> currentAlgo;
    bool mod256 = true;
public:
    VisualizerEngine() {
        // 初期状態としてBrainfuckをセット
        setAlgorithm("brainfuck");
    }

    void setAlgorithm(std::string name) {
        if (name == "brainfuck") {
            currentAlgo = std::make_unique<Brainfuck>();
            Brainfuck* bf = dynamic_cast<Brainfuck*>(currentAlgo.get());
            if (bf) bf->setBrainfuckModint(mod256);
        } else if (name == "graph") {
            currentAlgo = std::make_unique<GraphVisualizer>();
        } else if (name == "automaton") {
            currentAlgo = std::make_unique<AutomatonVisualizer>();
        } else if (name == "traversal") {
            currentAlgo = std::make_unique<TraversalVisualizer>();
        } else if (name == "bst") {
            currentAlgo = std::make_unique<BstVisualizer>();
        } else if (name == "heap") {
            currentAlgo = std::make_unique<HeapVisualizer>();
        } else if (name == "trie") {
            currentAlgo = std::make_unique<TrieVisualizer>();
        } else if (name == "huffman") {
            currentAlgo = std::make_unique<HuffmanVisualizer>();
        } else if (name == "avl") {
            currentAlgo = std::make_unique<AvlVisualizer>();
        } else if (name == "btree") {
            currentAlgo = std::make_unique<BTreeVisualizer>();
        } else if (name == "bubble") {
            currentAlgo = std::make_unique<BubbleSortVisualizer>();
        } else {
            std::cerr << "Unknown algorithm: " << name << std::endl;
        }
    }

    // 以下、現在のアルゴリズムへの委譲
    void load(std::string source, std::string input) {
        if (currentAlgo) currentAlgo->load(source, input);
    }

    // 事前計算(グラフならレイアウト収束)を1単位進める
    bool prepare() {
        if (currentAlgo) return currentAlgo->prepare();
        return true;
    }

    bool step() {
        if (currentAlgo) return currentAlgo->step();
        return false;
    }

    void runToEnd() {
        if (currentAlgo) currentAlgo->runToEnd();
    }

    void stepBack() {
        if (currentAlgo) currentAlgo->stepBack();
    }

    val getState(val params) {
        if (currentAlgo) return currentAlgo->getState(params);
        return val::null();
    }

    std::string getOutput() {
        if (currentAlgo) return currentAlgo->getOutput();
        return "";
    }

    void setBrainfuckModint(const bool mod256) {
        this->mod256 = mod256;
        if (currentAlgo) {
            Brainfuck* bf = dynamic_cast<Brainfuck*>(currentAlgo.get());
            if (bf) bf->setBrainfuckModint(mod256);
        }
    }
};

// JSへの公開定義
EMSCRIPTEN_BINDINGS(my_module) {
    class_<VisualizerEngine>("VisualizerEngine")
        .constructor<>()
        .function("setAlgorithm", &VisualizerEngine::setAlgorithm)
        .function("load", &VisualizerEngine::load)
        .function("prepare", &VisualizerEngine::prepare)
        .function("step", &VisualizerEngine::step)
        .function("runToEnd", &VisualizerEngine::runToEnd)
        .function("stepBack", &VisualizerEngine::stepBack)
        .function("getState", &VisualizerEngine::getState)
        .function("getOutput", &VisualizerEngine::getOutput)
        .function("setBrainfuckModint", &VisualizerEngine::setBrainfuckModint);
}
