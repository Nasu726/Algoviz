// C++ 側 (cpp/main.cpp の VisualizerEngine) が embind で公開している API の型。
//
// WASM との境界は実行時には型が無いので、ここで書いた形が唯一の仕様書になる。
// C++ 側を変えたらこのファイルも必ず揃えること。スモークテスト
// (test/smoke.mjs) が実際のバインディングとの食い違いを検出する。

/** テープ表示用のセル1つ (Brainfuck) */
export interface TapeCell {
    index: number;
    value: number;
    exists: boolean;
    name: string;
}

export interface BrainfuckState {
    pc: number;
    ptr: number;
    tape: TapeCell[];
    output: string;
    stepCount: bigint;
    code: string;
    isError: boolean;
    errorMessage: string;
    /** runToEnd がステップ上限で打ち切られた */
    interrupted: boolean;
    stepLimit: bigint;
}

export interface GraphState {
    /** [x, y, weight, colorId] * V。WASM のメモリを直接見ているビュー */
    nodes: Float32Array;
    /** [from, to, weight, colorId] * E */
    edges: Float32Array;
    nodeCount: number;
    edgeCount: number;
    maxNodes: number;
    maxEdges: number;
    layoutStable: boolean;
    /** グラフを作り直すたびに増える */
    generation: number;
    startNodeIndex: number;
    isDirected: boolean;
    isAutomaton: boolean;

    /** getState({ withText: true }) のときだけ */
    graphText?: string;

    /** AutomatonVisualizer のときだけ */
    acceptingStates?: number[];

    /** TraversalVisualizer のときだけ */
    algorithm?: 'bfs' | 'dfs';
    startNode?: number;
    goalNode?: number;
    current?: number;
    finished?: boolean;
    found?: boolean;
    canStepBack?: boolean;

    /** TraversalVisualizer かつ getState({ withProgress: true }) のときだけ */
    frontier?: number[];
    visitOrder?: number[];
    path?: number[];
}

/** getState に渡せるパラメータ。どれも省略可能 */
export interface StateParams {
    /** Brainfuck: テープの表示開始位置 */
    start?: number;
    /** Brainfuck: テープの表示セル数 */
    range?: number;
    /** グラフ: テキスト表現を組み立てる */
    withText?: boolean;
    /** 探索: キュー・訪問順・経路を組み立てる */
    withProgress?: boolean;
}

export interface VisualizerEngine {
    /** "brainfuck" | "graph" | "traversal" | "automaton" */
    setAlgorithm(name: string): void;
    load(source: string, input: string): void;
    /** 事前計算 (グラフならレイアウト収束) を1単位進める。準備完了なら true */
    prepare(): boolean;
    /** アルゴリズムを1手進める。継続可能なら true */
    step(): boolean;
    runToEnd(): void;
    stepBack(): void;
    /**
     * 現在の状態を受け取る。
     * 返る形はどのビジュアライザが動いているかで変わるので、
     * 呼び出し側が期待する型を指定する。
     */
    getState<T = GraphState>(params: StateParams): T;
    getOutput(): string;
    setBrainfuckModint(mod256: boolean): void;
}

/** core.js が window に生やすファクトリ */
export interface VisualizerModule {
    VisualizerEngine: new () => VisualizerEngine;
}

export type CreateVisualizerModule = () => Promise<VisualizerModule>;

declare global {
    /** index.html が読み込む /wasm/core.js が生やす */
    var createVisualizerModule: CreateVisualizerModule | undefined;
}
