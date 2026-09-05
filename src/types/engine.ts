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
    /** 節点ごとの半幅。既定は 20 (半径と同じ)。広いものは角丸の長方形で描く */
    nodeHalfWidths: Float32Array;
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
    /** 頂点に出す表示名。分類ごとに C++ 側が決める */
    labelMode: 'index' | 'state' | 'value' | 'none' | 'text';
    /** labelMode が text のときに使う、節点ごとの表示名 */
    nodeLabels?: string[];
    /** 辺の3列目が重みではなく記号 (1文字) か */
    edgeSymbols: boolean;
    /** テキスト入力で頂点の重みを受け取ったか */
    hasNodeWeights: boolean;
    /** 重み付きグラフか。重み無しならテキストにも表示にも重みは出ない */
    weighted: boolean;

    /** getState({ withText: true }) のときだけ */
    graphText?: string;

    /** AutomatonVisualizer のときだけ */
    acceptingStates?: number[];
    /** アルファベットと入力の長さの上限。C++ 側が唯一の情報源 */
    maxAlphabet?: number;
    maxInput?: number;
    /** 実際に使われている遷移記号を並べたもの */
    alphabet?: string;
    inputString?: string;
    /** 入力を何文字目まで消費したか */
    inputPos?: number;
    currentState?: number;
    accepted?: boolean;
    /** 遷移が定義されていなくて止まった */
    stuck?: boolean;
    /** 同じ状態・同じ記号の遷移が2本以上ある。DFA の前提を外れる */
    hasNondeterminism?: boolean;

    /** TraversalVisualizer のときだけ */
    algorithm?: 'bfs' | 'dfs' | 'dijkstra';
    startNode?: number;
    goalNode?: number;
    current?: number;
    finished?: boolean;
    found?: boolean;
    canStepBack?: boolean;
    /** 頂点の脇に出す数値が何を表しているか */
    nodeValueMode?: 'weight' | 'distance';
    /** ダイクストラの前提を外れる負の重みが混ざっているか */
    hasNegativeEdge?: boolean;

    /** HuffmanVisualizer のときだけ */
    counts?: { ch: string; count: number }[];
    /** まだ繋がっていない木の数 */
    rootCount?: number;
    selectedA?: number;
    selectedB?: number;
    maxLeaves?: number;

    /** TrieVisualizer のときだけ */
    words?: string[];
    /** 今いる節点までに読んだ接頭辞 */
    prefix?: string;
    maxWords?: number;

    /** AvlVisualizer のときだけ */
    checking?: number;
    /** 直前の回転。0 無し / 1 右 / 2 左 / 3 左右 / 4 右左 */
    rotation?: number;
    treeHeight?: number;

    /** BTreeVisualizer のときだけ */
    order?: number;
    /** 直前の手があふれた節点の分割だった */
    splitting?: boolean;
    /** 上へ動く値を持つ節点と、その節点の何番目の値か。無いときは -1 */
    risingNode?: number;
    risingSlot?: number;
    /** 直前の手で親へ移った値。元いた節点と、入った先の節点・セル */
    landedFrom?: number;
    landedNode?: number;
    landedSlot?: number;

    /** BstVisualizer のときだけ */
    values?: number[];
    /** これから挿入する値が values の何番目か */
    pending?: number;
    /** 今比べている節点。-1 なら次の値の挿入前 */
    cursor?: number;
    /** 直前の値が既にあったので捨てた */
    duplicate?: boolean;
    insertedCount?: number;
    maxValues?: number;

    /** TraversalVisualizer かつ getState({ withProgress: true }) のときだけ */
    frontier?: number[];
    visitOrder?: number[];
    path?: number[];
    /** ダイクストラの暫定距離。未到達は Infinity */
    distances?: number[];
    goalDistance?: number;
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
