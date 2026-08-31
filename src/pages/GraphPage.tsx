import React, { useEffect, useRef, useState } from 'react';
import { useInterval } from 'react-use';
import { GraphRenderer } from '../components/visualizers/GraphRenderer';
import { NODE_STROKE, EDGE_COLOR } from '../components/visualizers/PixiGraphApp';
import { PlaybackControls } from '../components/ui/PlaybackControls';
import { speedUp, speedDown } from '../components/ui/playbackSpeed';
import { Popup } from '../components/ui/popup';
import { useKeyboardShortcuts } from '../hooks/keyboardShortcut';
import type { VisualizerEngine, GraphState } from '../types/engine';

interface GraphProps {
    engine: VisualizerEngine;
    onBack: () => void;
}

// C++ 側のビジュアライザクラスに対応する。切り替えるとクラスごと差し替わる。
type Mode = 'graph' | 'traversal' | 'automaton';
type Algorithm = 'bfs' | 'dfs' | 'dijkstra';

const MODE_LABEL: Record<Mode, string> = {
    graph: 'グラフを描くだけ',
    traversal: '探索 (BFS / DFS / ダイクストラ)',
    automaton: 'オートマトン',
};

const hex = (n: number) => '#' + n.toString(16).padStart(6, '0');

const Section: React.FC<{ title: string; children: React.ReactNode }> = ({ title, children }) => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        <h3 style={{ margin: 0, fontSize: '14px', color: '#37474f', borderBottom: '1px solid #cfd8dc', paddingBottom: '4px' }}>
            {title}
        </h3>
        {children}
    </div>
);

const Swatch: React.FC<{ color: number; label: string; isEdge?: boolean }> = ({ color, label, isEdge }) => (
    <span style={{ display: 'inline-flex', alignItems: 'center', gap: '5px', fontSize: '12px', whiteSpace: 'nowrap' }}>
        <span style={{
            width: isEdge ? '16px' : '12px',
            height: isEdge ? '3px' : '12px',
            borderRadius: isEdge ? '2px' : '50%',
            border: isEdge ? 'none' : '3px solid ' + hex(color),
            backgroundColor: isEdge ? hex(color) : '#fff',
            flexShrink: 0,
        }} />
        {label}
    </span>
);

export const GraphPage: React.FC<GraphProps> = ({ engine, onBack }) => {
    const [mode, setMode] = useState<Mode>('traversal');

    // --- グラフ生成 ---
    const [nodeCount, setNodeCount] = useState("8");
    const [edgeCount, setEdgeCount] = useState("10");
    const [isDirected, setIsDirected] = useState(false);
    const [allowSelfLoop, setAllowSelfLoop] = useState(false);
    const [allowSameEdge, setAllowSameEdge] = useState(false);
    const [skipExtension, setSkipExtension] = useState(true);
    const [useNodeWeights, setUseNodeWeights] = useState(false);
    const [isHorizontal, setIsHorizontal] = useState(true);
    const [inputBuffer, setInputBuffer] = useState("");

    // --- 探索 ---
    const [algorithm, setAlgorithm] = useState<Algorithm>('bfs');
    const [startNode, setStartNode] = useState("0");
    const [goalNode, setGoalNode] = useState("");

    // --- オートマトン ---
    const [automatonStart, setAutomatonStart] = useState("0");
    const [acceptingNodes, setAcceptingNodes] = useState("1, 2");

    // --- 表示 ---
    const [showWeights, setShowWeights] = useState(true);
    const [labelType, setLabelType] = useState<'index' | 'name'>('index');

    // --- 再生 ---
    const [isPlaying, setIsPlaying] = useState(false);
    const [delay, setDelay] = useState(300);
    const [state, setState] = useState<GraphState | null>(null);
    const [isLoaded, setIsLoaded] = useState(false);
    const [isHelpOpen, setIsHelpOpen] = useState(false);

    // 生成コマンドを組み立てるときに常に最新の設定を読めるようにしておく。
    // useEffect の依存配列に全部並べると、設定を変えるたびにグラフが作り直されてしまう。
    const opts = useRef({
        isHorizontal, skipExtension, isDirected, useNodeWeights, mode, algorithm,
        startNode, goalNode, automatonStart, acceptingNodes,
    });
    opts.current = {
        isHorizontal, skipExtension, isDirected, useNodeWeights, mode, algorithm,
        startNode, goalNode, automatonStart, acceptingNodes,
    };

    const orientation = () => opts.current.isHorizontal ? "horizontal" : "vertical";
    const skipFlag = () => opts.current.skipExtension ? 1 : 0;
    const dirFlag = () => opts.current.isDirected ? 1 : 0;
    const nodeWFlag = () => opts.current.useNodeWeights ? 1 : 0;

    // モード固有の設定を C++ 側へ渡す
    const applyModeSettings = () => {
        const o = opts.current;
        if (o.mode === 'traversal') {
            const goal = o.goalNode.trim() === "" ? -1 : Number(o.goalNode);
            engine.load("setTraversal", o.algorithm + " " + (Number(o.startNode) || 0) + " " + goal);
        } else if (o.mode === 'automaton') {
            engine.load("setStartNode", o.automatonStart);
            engine.load("setAccepting", o.acceptingNodes);
        }
    };

    // 進行状況（キュー・訪問順・経路）は要求したときだけ組み立てられる
    const readState = () => setState(engine.getState<GraphState>({ withProgress: true }));

    // グラフを作り直したあとは、テキスト欄も C++ が持っている内容に合わせる
    const readStateAndText = () => {
        const s = engine.getState<GraphState>({ withText: true, withProgress: true });
        setState(s);
        if (s && s.graphText) setInputBuffer(s.graphText);
    };

    const generate = (command: string) => {
        setIsPlaying(false);
        engine.load(orientation(), command);
        applyModeSettings();
        readStateAndText();
    };

    // モードを切り替えると C++ 側のクラスごと差し替わるので、
    // 今のグラフをテキスト経由で作り直す
    useEffect(() => {
        if (!engine) return;
        setIsPlaying(false);
        engine.setAlgorithm(mode);
        const body = inputBuffer.trim();
        engine.load(orientation(), body
            ? "custom " + skipFlag() + " " + dirFlag() + " " + nodeWFlag() + "\n" + body
            : "random " + nodeCount + " " + edgeCount + " " + skipFlag() + " 0 0 " + dirFlag());
        applyModeSettings();
        readStateAndText();
        setIsLoaded(true);
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [engine, mode]);

    // 探索・オートマトンの設定変更はグラフを作り直さずに反映する
    useEffect(() => {
        if (!engine || !isLoaded) return;
        setIsPlaying(false);
        applyModeSettings();
        readState();
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [algorithm, startNode, goalNode, automatonStart, acceptingNodes]);

    // レイアウトの向き
    useEffect(() => {
        if (!engine || !isLoaded) return;
        engine.load(orientation(), "");
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [isHorizontal]);

    // === 再生ループ ===
    useInterval(() => {
        if (!isPlaying || !engine) return;
        if (!engine.step()) setIsPlaying(false);
        readState();
    }, isPlaying ? delay : null);

    const isTraversal = mode === 'traversal';

    const handleReset = () => {
        setIsPlaying(false);
        engine.load("resetTraversal", "");
        readState();
    };
    const handleStep = () => {
        setIsPlaying(false);
        engine.step();
        readState();
    };
    const handleStepBack = () => {
        setIsPlaying(false);
        engine.stepBack();
        readState();
    };
    const handleRunToEnd = () => {
        setIsPlaying(false);
        engine.runToEnd();
        readState();
    };

    const handleGenerateRandom = () => generate(
        "random " + nodeCount + " " + edgeCount + " " + skipFlag() + " " +
        (allowSelfLoop ? 1 : 0) + " " + (allowSameEdge ? 1 : 0) + " " + dirFlag()
    );
    const handleGenerateComplete = () =>
        generate("complete " + nodeCount + " " + skipFlag() + " " + dirFlag());
    const handleGenerateFromText = () =>
        generate("custom " + skipFlag() + " " + dirFlag() + " " + nodeWFlag() + "\n" + inputBuffer);

    const backToMenu = () => {
        if (window.confirm("ビジュアライザ一覧へ戻りますか？")) onBack();
    };

    useKeyboardShortcuts({
        onEsc: !isHelpOpen ? backToMenu : undefined,
        onHelp: () => setIsHelpOpen(!isHelpOpen),
        onPlayPause: !isHelpOpen && isTraversal ? () => setIsPlaying(!isPlaying) : undefined,
        onStepNext: !isHelpOpen && isTraversal ? handleStep : undefined,
        onStepBack: !isHelpOpen && isTraversal ? handleStepBack : undefined,
        onSave: !isHelpOpen ? handleGenerateFromText : undefined,
        onSpeedUp: () => { if (!isHelpOpen) setDelay(speedUp(delay)); },
        onSpeedDown: () => { if (!isHelpOpen) setDelay(speedDown(delay)); },
    });

    // 数字だけを受け付ける入力欄
    const numberInput = (value: string, setter: (v: string) => void, width = '54px') => (
        <input
            type="text" value={value}
            onChange={(e) => setter(e.target.value.replace(/[^0-9]/g, ''))}
            onBlur={() => {
                const max = state?.maxNodes ?? 100;
                if (value.trim() === "") setter("0");
                else if (Number(value) > max) setter(String(max));
            }}
            style={{ width }}
        />
    );

    const frontierLabel = algorithm === 'bfs' ? 'キュー'
        : algorithm === 'dfs' ? 'スタック'
        : '優先度付きキュー';
    const orderLabel = algorithm === 'dijkstra' ? '確定順' : '訪問順';
    const isDijkstra = algorithm === 'dijkstra';
    const distances = state?.distances ?? [];
    const fmt = (v: number) => (Number.isFinite(v) ? String(v) : '\u221e');
    // 進行状況は探索モードのときだけ返ってくる
    const frontier = state?.frontier ?? [];
    const visitOrder = state?.visitOrder ?? [];
    const path = state?.path ?? [];
    const current = state?.current ?? -1;
    const total = state?.nodeCount ?? 0;

    return (
        <div style={{
            display: 'flex', flexDirection: 'column',
            height: '100vh', width: '100vw',
            margin: 0, overflow: 'hidden', fontFamily: 'sans-serif',
            backgroundColor: '#fff', color: '#000',
        }}>
            {/* === ヘッダー === */}
            <div style={{
                display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                padding: '10px 20px', backgroundColor: '#263238', color: 'white', flexShrink: 0,
            }}>
                <button onClick={backToMenu} style={{ cursor: 'pointer' }}>◀ 戻る</button>
                <h2 style={{ margin: 0, fontSize: '18px' }}>グラフビジュアライザ</h2>
                <button onClick={() => setIsHelpOpen(true)} style={{ cursor: 'pointer', fontWeight: 'bold' }}>
                    ヘルプ ❓
                </button>
            </div>

            <div style={{ display: 'flex', flexDirection: 'row', flex: 1, minHeight: 0 }}>
                {/* === 左: 操作パネル === */}
                <div style={{
                    width: '300px', flexShrink: 0,
                    display: 'flex', flexDirection: 'column', gap: '18px',
                    padding: '15px', overflowY: 'auto',
                    background: '#f8f9fa', borderRight: '1px solid #ddd',
                }}>
                    <Section title="モード">
                        <select value={mode} onChange={(e) => setMode(e.target.value as Mode)} style={{ width: '100%', padding: '4px' }}>
                            {(Object.keys(MODE_LABEL) as Mode[]).map(m => (
                                <option key={m} value={m}>{MODE_LABEL[m]}</option>
                            ))}
                        </select>
                    </Section>

                    {isTraversal && (
                        <Section title="探索">
                            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '10px' }}>
                                <label style={{ fontSize: '13px' }}>
                                    <input type="radio" checked={algorithm === 'bfs'} onChange={() => setAlgorithm('bfs')} /> BFS
                                </label>
                                <label style={{ fontSize: '13px' }}>
                                    <input type="radio" checked={algorithm === 'dfs'} onChange={() => setAlgorithm('dfs')} /> DFS
                                </label>
                                <label style={{ fontSize: '13px' }}>
                                    <input type="radio" checked={algorithm === 'dijkstra'} onChange={() => setAlgorithm('dijkstra')} /> ダイクストラ
                                </label>
                            </div>
                            <div style={{ fontSize: '13px' }}>
                                始点 s: {numberInput(startNode, setStartNode)}
                                <span style={{ marginLeft: '10px' }}>
                                    終点 t: <input
                                        type="text" value={goalNode} placeholder="なし"
                                        onChange={(e) => setGoalNode(e.target.value.replace(/[^0-9]/g, ''))}
                                        style={{ width: '54px' }}
                                    />
                                </span>
                            </div>
                            <PlaybackControls
                                isPlaying={isPlaying}
                                ready={!!state}
                                canStepBack={!!state?.canStepBack}
                                delay={delay}
                                loadLabel="最初から"
                                onLoad={handleReset}
                                onPlayPause={() => setIsPlaying(!isPlaying)}
                                onStepBack={handleStepBack}
                                onStepNext={handleStep}
                                onRunToEnd={handleRunToEnd}
                                onDelayChange={setDelay}
                                vertical
                            />
                        </Section>
                    )}

                    <Section title="グラフ生成">
                        <div style={{ fontSize: '13px' }}>
                            頂点数 V: {numberInput(nodeCount, setNodeCount)}
                            <span style={{ marginLeft: '10px' }}>辺の数 E: {numberInput(edgeCount, setEdgeCount)}</span>
                        </div>
                        <label style={{ fontSize: '13px' }}>
                            <input type="checkbox" checked={isDirected} onChange={(e) => setIsDirected(e.target.checked)} /> 有向グラフ
                        </label>
                        <label style={{ fontSize: '13px' }}>
                            <input type="checkbox" checked={allowSelfLoop} onChange={(e) => setAllowSelfLoop(e.target.checked)} /> 自己ループを許す
                        </label>
                        <label style={{ fontSize: '13px' }}>
                            <input type="checkbox" checked={allowSameEdge} onChange={(e) => setAllowSameEdge(e.target.checked)} /> 多重辺を許す
                        </label>
                        <label style={{ fontSize: '13px' }}>
                            <input type="checkbox" checked={useNodeWeights} onChange={(e) => setUseNodeWeights(e.target.checked)} /> 頂点の重みを入力する
                        </label>
                        <button onClick={handleGenerateRandom} style={{ padding: '8px', cursor: 'pointer' }}>ランダム生成</button>
                        <button onClick={handleGenerateComplete} style={{ padding: '8px', cursor: 'pointer' }}>完全グラフ生成</button>
                    </Section>

                    <Section title="グラフ入力">
                        <textarea
                            value={inputBuffer}
                            onChange={(e) => setInputBuffer(e.target.value)}
                            style={{ width: '100%', height: '120px', fontFamily: 'monospace', whiteSpace: 'pre', resize: 'vertical', boxSizing: 'border-box' }}
                            placeholder={useNodeWeights
                                ? "頂点数 辺数\n頂点0の重み 頂点1の重み ...\n始点 終点 (重み)\n..."
                                : "頂点数 辺数\n始点 終点 (重み)\n始点 終点 (重み)\n..."}
                        />
                        <button onClick={handleGenerateFromText} style={{ padding: '8px', cursor: 'pointer' }}>
                            📝 テキストから生成
                        </button>
                    </Section>

                    <Section title="表示">
                        <label style={{ fontSize: '13px' }}>
                            <input type="checkbox" checked={isHorizontal} onChange={(e) => setIsHorizontal(e.target.checked)} /> 横長レイアウト
                        </label>
                        <label style={{ fontSize: '13px' }}>
                            <input type="checkbox" checked={showWeights} onChange={(e) => setShowWeights(e.target.checked)} /> 重みを表示
                        </label>
                        <label style={{ fontSize: '13px' }}>
                            <input type="checkbox" checked={skipExtension} onChange={(e) => setSkipExtension(e.target.checked)} /> 展開アニメーションを飛ばす
                        </label>
                        <div style={{ fontSize: '13px' }}>
                            頂点の表示名:
                            <select value={labelType} onChange={(e) => setLabelType(e.target.value as 'index' | 'name')} style={{ width: '100%', marginTop: '4px' }}>
                                <option value="index">インデックス (0, 1...)</option>
                                <option value="name">状態名 (q₀, q₁...)</option>
                            </select>
                        </div>
                    </Section>

                    {mode === 'automaton' && (
                        <Section title="オートマトン">
                            <div style={{ fontSize: '13px' }}>初期状態: {numberInput(automatonStart, setAutomatonStart)}</div>
                            <div style={{ fontSize: '13px' }}>
                                受理状態 (カンマ区切り):
                                <input
                                    type="text" value={acceptingNodes} placeholder="例: 1, 2"
                                    onChange={(e) => setAcceptingNodes(e.target.value)}
                                    style={{ width: '100%', marginTop: '4px' }}
                                />
                            </div>
                        </Section>
                    )}

                    {isTraversal && (
                        <Section title="実行状態">
                            <div style={{ fontSize: '13px', lineHeight: 1.7 }}>
                                <div>
                                    <b>{frontierLabel}</b>: {frontier.length
                                        ? frontier.join(' → ')
                                        : <span style={{ color: '#90a4ae' }}>空</span>}
                                </div>
                                <div>
                                    <b>処理中</b>: {current >= 0
                                        ? current
                                        : <span style={{ color: '#90a4ae' }}>なし</span>}
                                </div>
                                <div>
                                    <b>{orderLabel}</b>: {visitOrder.length
                                        ? visitOrder.join(', ')
                                        : <span style={{ color: '#90a4ae' }}>なし</span>}
                                    <span style={{ color: '#90a4ae' }}> ({visitOrder.length} / {total})</span>
                                </div>
                                {isDijkstra && (
                                    <div>
                                        <b>距離</b>: {distances.length
                                            ? distances.map((d, i) => `${i}:${fmt(d)}`).join('  ')
                                            : <span style={{ color: '#90a4ae' }}>なし</span>}
                                    </div>
                                )}
                                <div>
                                    <b>経路</b>: {path.length
                                        ? path.join(' → ')
                                        : <span style={{ color: '#90a4ae' }}>未発見</span>}
                                    {isDijkstra && path.length > 0 && state?.goalDistance !== undefined && (
                                        <span style={{ color: '#90a4ae' }}> (長さ {fmt(state.goalDistance)})</span>
                                    )}
                                </div>
                                <div style={{ marginTop: '6px', fontWeight: 'bold', color: state?.found ? '#27ae60' : '#78909c' }}>
                                    {!state?.finished ? '探索中…'
                                        : state?.found ? '終点に到達しました'
                                        : goalNode.trim() === "" ? '到達できる範囲を調べ終えました'
                                        : '終点には到達できませんでした'}
                                </div>
                            </div>

                            {isDijkstra && state?.hasNegativeEdge && (
                                <div style={{
                                    padding: '6px 8px', borderRadius: '4px', fontSize: '12px',
                                    backgroundColor: '#fff8e1', border: '1px solid #ffb300', color: '#5d4037',
                                }}>
                                    負の重みの辺があります。ダイクストラ法は非負の重みを前提にしているので、
                                    求まる距離が最短とは限りません。
                                </div>
                            )}

                            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px', marginTop: '4px' }}>
                                <Swatch color={NODE_STROKE[0]} label="未訪問" />
                                <Swatch color={NODE_STROKE[1]} label={frontierLabel + "の中"} />
                                <Swatch color={NODE_STROKE[2]} label="処理中" />
                                <Swatch color={NODE_STROKE[3]} label={isDijkstra ? "確定済み" : "訪問済み"} />
                                <Swatch color={NODE_STROKE[4]} label="経路上" />
                                <Swatch color={EDGE_COLOR[1]} label="探索木の辺" isEdge />
                                <Swatch color={EDGE_COLOR[3]} label="調べ済みの辺" isEdge />
                                <Swatch color={EDGE_COLOR[4]} label="経路の辺" isEdge />
                            </div>
                        </Section>
                    )}
                </div>

                {/* === 右: キャンバス === */}
                <div style={{ flex: 1, display: 'flex', minWidth: 0, padding: '15px' }}>
                    {isLoaded && (
                        <GraphRenderer engine={engine} showWeights={showWeights} labelType={labelType} />
                    )}
                </div>
            </div>

            <Popup title="ヘルプ" isOpen={isHelpOpen} onClose={() => setIsHelpOpen(false)}>
                <h3>1. モード</h3>
                <ul>
                    <li><b>グラフを描くだけ</b>：生成したグラフを表示する</li>
                    <li><b>探索 (BFS / DFS / ダイクストラ)</b>：始点 s から探索する様子を1手ずつ見る。終点 t を指定すると s から t への経路を探す</li>
                    <li><b>オートマトン</b>：常に有向グラフとして扱い、初期状態への矢印と受理状態の二重丸を描く</li>
                </ul>

                <h3>2. 探索の見方</h3>
                <p>1ステップは「辺を1本調べる」か「その頂点の隣接をすべて調べ終える」のどちらかです。</p>
                <ul>
                    <li><b>BFS</b>：見つけた頂点をキューの末尾に積み、先頭から取り出す。始点から近い順に広がる</li>
                    <li><b>DFS</b>：見つけた頂点をスタックに積み、すぐそこへ潜る。行き止まりまで進んでから戻る（バックトラック）</li>
                    <li><b>ダイクストラ法</b>：まだ確定していない頂点のうち、暫定距離が最小のものを取り出して確定させる。取り出した頂点から伸びる辺を見て、その辺を通った方が近ければ距離と親を更新する（緩和）</li>
                    <li>BFS が見つける経路は<b>辺の本数</b>で最短、ダイクストラが見つける経路は<b>重みの合計</b>で最短になる。この2つは一致しないことがある</li>
                </ul>
                <p>3つの違いは「次にどの頂点を取り出すか」だけです。BFS は先頭（キュー）、DFS は末尾（スタック）、ダイクストラは暫定距離が最小のもの（優先度付きキュー）を取り出します。</p>
                <p>ダイクストラのときは、各頂点の脇に暫定距離が出ます。まだ届いていない頂点は ∞ です。負の重みがある場合は正しい答えを出せないので、警告を表示します。</p>
                <p>色の意味はサイドバーの凡例のとおりです。「探索木の辺」はその頂点を最初に見つけたときに通った辺、「調べ済みの辺」は見に行ったが既に発見済みの頂点へ向かっていた辺です。</p>

                <h3>3. グラフの入力</h3>
                <p>1行目に「頂点数 辺数」、続けて辺を1行ずつ「始点 終点 重み」の形で書きます。重みは省略できます。</p>
                <pre style={{ background: '#eceff1', padding: '8px', borderRadius: '4px' }}>
                    4 4{'\n'}0 1 5{'\n'}1 2 3{'\n'}2 3 7{'\n'}0 3
                </pre>
                <p>「頂点の重みを入力する」にチェックを入れると、「頂点数 辺数」の次の行で頂点の重みを V 個並べて指定できます。</p>
                <pre style={{ background: '#eceff1', padding: '8px', borderRadius: '4px' }}>
                    3 2{'\n'}10 20 30{'\n'}0 1 5{'\n'}1 2 5
                </pre>
                <ul>
                    <li>辺の重みを書かないと 1 として扱います。重み無しグラフのダイクストラは BFS と同じ結果になります</li>
                    <li>範囲外の頂点番号を含む行は読み飛ばされます</li>
                    <li>頂点数には上限があります（現在 {state?.maxNodes ?? 100}）</li>
                    <li>「有向グラフ」のチェックは生成時に反映されます。探索が辺の向きを守るかどうかもこれで決まります</li>
                </ul>

                <h3>4. 画面の操作</h3>
                <ul>
                    <li><b>ドラッグ</b>：表示位置を動かす</li>
                    <li><b>ホイール</b>：拡大・縮小。縮小しすぎると文字は表示されなくなります</li>
                    <li>グラフを作り直すと、自動で全体が画面に収まる位置に戻ります</li>
                </ul>

                <h3>5. ショートカットキー</h3>
                <ul>
                    <li><b>Esc</b>：ビジュアライザ選択画面へ戻る</li>
                    <li><b>Ctrl + Enter</b>：実行 / 一時停止</li>
                    <li><b>Ctrl + H</b>：ヘルプを開く</li>
                    <li><b>Ctrl + S</b>：テキストからグラフを生成</li>
                    <li><b>Ctrl + ←</b> / <b>→</b>：戻る / 進む</li>
                    <li><b>Ctrl + ↑</b> / <b>↓</b>：実行速度の増減</li>
                </ul>
            </Popup>
        </div>
    );
};
