// グラフ系ページで共有する型と小さなヘルパー。

/** 0xRRGGBB を CSS の色に */
export const hex = (n: number) => '#' + n.toString(16).padStart(6, '0');

/** 1ページ1アルゴリズム。ルートと1対1に対応する */
export type GraphVariant =
    'bfs' | 'dfs' | 'dijkstra' | 'prim' | 'kruskal' | 'bipartite' | 'automaton' | 'plain';

/** 探索を行う variant か (プリム法も「取り出して確定」の同じ形) */
export const isTraversal = (v: GraphVariant): v is 'bfs' | 'dfs' | 'dijkstra' | 'prim' =>
    v === 'bfs' || v === 'dfs' || v === 'dijkstra' || v === 'prim';

/**
 * 向きと重みの有無を利用者に選ばせない variant の形。選ばせるなら null。
 * 固定するときは常に無向
 */
export const fixedShape = (v: GraphVariant): { weighted: boolean } | null =>
    v === 'kruskal' || v === 'prim' ? { weighted: true }  // 最小全域木
    : v === 'bipartite' ? { weighted: false }             // 重みは判定に関係ない
    : null;

export const VARIANT_TITLE: Record<GraphVariant, string> = {
    bfs: '幅優先探索 (BFS)',
    dfs: '深さ優先探索 (DFS)',
    dijkstra: 'ダイクストラ法',
    prim: 'プリム法',
    kruskal: 'クラスカル法',
    bipartite: '二部グラフ判定',
    automaton: '決定性有限オートマトン (DFA)',
    plain: 'グラフ描画',
};

/** C++ 側の setAlgorithm へ渡す名前 */
export const engineAlgorithm = (v: GraphVariant): string =>
    isTraversal(v) ? 'traversal' : v === 'automaton' ? 'automaton'
    : v === 'kruskal' ? 'kruskal' : v === 'bipartite' ? 'bipartite' : 'graph';

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
    /** 二部グラフになるように作るか (二部グラフ判定のページだけ) */
    bipartite: boolean;
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
    connected: isTraversal(variant) || variant === 'kruskal' || variant === 'bipartite',
    // 重みを使うのはダイクストラと最小全域木だけ。遊び場も既定で重み付きにしておく。
    // BFS / DFS は重みを一切見ないので、出すと使っていると誤解させる
    weighted: fixedShape(variant)?.weighted ?? (variant === 'dijkstra' || variant === 'plain'),
    useNodeWeights: false,
    skipExtension: true,
    showWeights: true,
    alphabet: 'ab',
    inputBuffer: '',
    // ランダムなグラフはまず二部グラフにならないので、既定では二部グラフに作る
    bipartite: true,
});
