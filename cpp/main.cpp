#include <emscripten/bind.h>
#include <memory>
#include <iostream>
#include "include/IVisualizer.hpp"
#include "include/Brainfuck.hpp"

using namespace emscripten;

class VisualizerEngine {
private:
    std::unique_ptr<IVisualizer> currentAlgo;

public:
    VisualizerEngine() {
        // 初期状態としてBrainfuckをセット
        setAlgorithm("brainfuck");
    }

    void setAlgorithm(std::string name) {
        if (name == "brainfuck") {
            currentAlgo = std::make_unique<Brainfuck>();
        } 
        // 将来ここに else if (name == "bubble_sort") ... を追加
        else {
            std::cerr << "Unknown algorithm: " << name << std::endl;
        }
    }

    // 以下、現在のアルゴリズムへの委譲
    void load(std::string source, std::string input) {
        if (currentAlgo) currentAlgo->load(source, input);
    }

    bool step() {
        if (currentAlgo) return currentAlgo->step();
        return false;
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
};

// JSへの公開定義
EMSCRIPTEN_BINDINGS(my_module) {
    class_<VisualizerEngine>("VisualizerEngine")
        .constructor<>()
        .function("setAlgorithm", &VisualizerEngine::setAlgorithm)
        .function("load", &VisualizerEngine::load)
        .function("step", &VisualizerEngine::step)
        .function("stepBack", &VisualizerEngine::stepBack)
        .function("getState", &VisualizerEngine::getState)
        .function("getOutput", &VisualizerEngine::getOutput);
}