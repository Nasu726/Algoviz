// グラフ系ページで共有する型と小さなヘルパー。

/** 0xRRGGBB を CSS の色に */
export const hex = (n: number) => '#' + n.toString(16).padStart(6, '0');

/** 1ページ1アルゴリズム。ルートと1対1に対応する */
export type GraphVariant = 'bfs' | 'dfs' | 'dijkstra' | 'automaton' | 'plain';

/** 探索を行う variant か */
export const isTraversal = (v: GraphVariant): v is 'bfs' | 'dfs' | 'dijkstra' =>
    v === 'bfs' || v === 'dfs' || v === 'dijkstra';

export const VARIANT_TITLE: Record<GraphVariant, string> = {
    bfs: '幅優先探索 (BFS)',
    dfs: '深さ優先探索 (DFS)',
    dijkstra: 'ダイクストラ法',
    automaton: '決定性有限オートマトン (DFA)',
    plain: 'グラフ描画',
};

/** C++ 側の setAlgorithm へ渡す名前 */
export const engineAlgorithm = (v: GraphVariant): string =>
    isTraversal(v) ? 'traversal' : v === 'automaton' ? 'automaton' : 'graph';

/** グラフの作り方と見た目の設定。実行前に決めるもの */
export interface GraphSettings {
    nodeCount: string;
    edgeCount: string;
    isDirected: boolean;
    allowSelfLoop: boolean;
    allowSameEdge: boolean;
    connected: boolean;
    /** 重み付きグラフか。重み無しなら重みを振らず、表示にもテキストにも出さない */
    weighted: boolean;
    useNodeWeights: boolean;
    skipExtension: boolean;
    showWeights: boolean;
    /** オートマトンの遷移記号として使う文字 */
    alphabet: string;
    inputBuffer: string;
}

export const defaultSettings = (variant: GraphVariant): GraphSettings => ({
    // オートマトンは1状態あたりアルファベットの数だけ遷移が出るので、少なめに始める
    nodeCount: variant === 'automaton' ? '4' : '8',
    edgeCount: '10',
    // オートマトンは常に有向なので、既定でチェックを入れておく
    isDirected: variant === 'automaton',
    allowSelfLoop: false,
    allowSameEdge: false,
    // 探索を見せるページでは、途中で行き止まりにならない方が既定として自然
    connected: isTraversal(variant),
    // 重みを使うのはダイクストラだけ。遊び場も既定で重み付きにしておく。
    // BFS / DFS は重みを一切見ないので、出すと使っていると誤解させる
    weighted: variant === 'dijkstra' || variant === 'plain',
    useNodeWeights: false,
    skipExtension: true,
    showWeights: true,
    alphabet: 'ab',
    inputBuffer: '',
});
