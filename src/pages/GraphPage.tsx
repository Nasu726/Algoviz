import React, { useEffect, useState } from 'react';
import { GraphRenderer } from '../components/visualizers/GraphRenderer'; 

interface GraphProps {
  engine: any;
  onBack: () => void;
}

export const GraphPage: React.FC<GraphProps> = ({ engine, onBack }) => {
    const [isLoaded, setIsLoaded] = useState(false);
    const [isHorizontal, setIsHorizontal] = useState(true);
    const [inputBuffer, setInputBuffer] = useState("");

    // テスト環境用のState
    const [nodeCount, setNodeCount] = useState("5");
    const [edgeCount, setEdgeCount] = useState("7");
    const [isDirected, setIsDirected] = useState(true);
    const [showWeights, setShowWeights] = useState(true);
    const [labelType, setLabelType] = useState<'index' | 'name'>('index');

    const [skipExtension, setSkipExtension] = useState(true);
    const [allowSelfLoop, setAllowSelfLoop] = useState(true);
    const [allowSameEdge, setAllowSameEdge] = useState(true);
    
    // オートマトン用のState
    const [isAutomaton, setIsAutomaton] = useState(false);
    const [startNode, setStartNode] = useState("0");
    const [acceptingNodes, setAcceptingNodes] = useState("1, 2");

    useEffect(() => {
        if (!engine) return;
        engine.setAlgorithm("graph");
        engine.load(isHorizontal ? "horizontal" : "vertical", "random 5 7");
        setIsLoaded(true);
        const state = engine.getState({});
        if (state && state.graphText) setInputBuffer(state.graphText);
    }, [engine]);

    useEffect(() => {
        if (!engine) return;
        engine.load("setStartNode", isAutomaton ? startNode : "-1");
    }, [isAutomaton, startNode, engine]);

    const handleGenerateRandom = () => {
        const skip = skipExtension ? 1 : 0;
        const selfLoop = allowSelfLoop ? 1 : 0;
        const sameEdge = allowSameEdge ? 1 : 0;
        const isDir = isDirected ? 1 : 0;
        engine.load(
            isHorizontal ? "horizontal" : "vertical",
            `random ${nodeCount} ${edgeCount} ${skip} ${selfLoop} ${sameEdge} ${isDir}`
        );
        const state = engine.getState({});
        if (state && state.graphText) setInputBuffer(state.graphText);
    };

    const handleGenerateComplete = () => {
        const skip = skipExtension ? 1 : 0;
        const isDir = isDirected ? 1 : 0;
        engine.load(
            isHorizontal ? "horizontal" : "vertical", 
            `complete ${nodeCount} ${skip} ${isDir}`
        );
        const state = engine.getState({});
        if (state && state.graphText) setInputBuffer(state.graphText);
    };

    const handleGenerateFromText = () => {
        const skip = skipExtension ? 1 : 0;
        engine.load(
            isHorizontal ? "horizontal" : "vertical",
            `custom ${inputBuffer} ${skip}`
        );
        const state = engine.getState({});
        if (state && state.graphText) setInputBuffer(state.graphText);
    };

    // 空欄でフォーカスが外れたら 0 を補完する 
    const handleBlur = (setter: React.Dispatch<React.SetStateAction<string>>, value: string) => {
        if (value.trim() === "") setter("0");
        if (Number(value.trim()) > 100) setter("100");
    };

    // 数字以外の入力を弾く 
    const handleNumberChange = (setter: React.Dispatch<React.SetStateAction<string>>) => (e: React.ChangeEvent<HTMLInputElement>) => {
        setter(e.target.value.replace(/[^0-9]/g, ''));
    };

    return (
        <div style={{ padding: "20px", fontFamily: 'sans-serif', display: "flex", flexDirection: "row", gap: "20px" }}>
            {/* 左側：コントロールパネル */}
            <div style={{ 
                display: 'flex', flexDirection: "column", gap: '15px', overflowY: "auto",
                minWidth: '280px', padding: '15px', background: '#f8f9fa', 
                borderRadius: '8px', border: '1px solid #ddd' 
            }}>
                <button onClick={onBack} style={{ padding: '8px 16px', alignSelf: 'flex-start' }}>◀ 戻る</button>
                <h3 style={{ margin: 0 }}>グラフ設定</h3>

                <div>
                    <label>頂点数 (V): </label>
                    <input type="text" value={nodeCount} onChange={handleNumberChange(setNodeCount)} onBlur={() => handleBlur(setNodeCount, nodeCount)} style={{ width: '50px' }} />
                </div>
                <div>
                    <label>辺の数 (E): </label>
                    <input type="text" value={edgeCount} onChange={handleNumberChange(setEdgeCount)} onBlur={() => handleBlur(setEdgeCount, edgeCount)} style={{ width: '50px' }} />
                </div>
                
                <button onClick={handleGenerateRandom} style={{ padding: '8px', cursor: 'pointer' }}> ランダム生成 </button>
                <button onClick={handleGenerateComplete} style={{ padding: '8px', cursor: 'pointer' }}> 完全グラフ生成 </button>

                {/* テキスト入出力エリア */}
                <hr style={{ width: '100%', borderTop: '1px solid #ccc' }} />
                <h3 style={{ margin: 0 }}>グラフ入力</h3>
                <textarea 
                    value={inputBuffer} 
                    onChange={e => setInputBuffer(e.target.value)}
                    style={{ width: '100%', height: '120px', fontFamily: 'monospace', whiteSpace: 'pre', resize: 'vertical' }}
                    placeholder="頂点数 辺数&#10;始点 終点 (重み)&#10;始点 終点 (重み)&#10;始点 終点 (重み)&#10;..."
                />
                <button onClick={handleGenerateFromText} style={{ padding: '8px', cursor: 'pointer' }}>
                    📝 テキストから生成
                </button>

                <hr style={{ width: '100%', borderTop: '1px solid #ccc' }} />

                <h3 style={{ margin: 0 }}>表示オプション</h3>
                <label>
                    <input type="checkbox" checked={isHorizontal} onChange={e => {
                        setIsHorizontal(e.target.checked);
                        engine.load(e.target.checked ? "horizontal" : "vertical", "");
                    }} /> 横長レイアウト
                </label>
                <label>
                    <input type="checkbox" checked={isDirected} onChange={e => setIsDirected(e.target.checked)} /> 有向辺 (Directed)
                </label>
                <label>
                    <input type="checkbox" checked={showWeights} onChange={e => setShowWeights(e.target.checked)} /> 重みを表示
                </label>
                <label>
                    <input type="checkbox" checked={skipExtension} onChange={e => setSkipExtension(e.target.checked)} /> 展開アニメーションを飛ばす
                </label>
                <label>
                    <input type="checkbox" checked={allowSelfLoop} onChange={e => setAllowSelfLoop(e.target.checked)} /> 自己ループを許す
                </label>
                <label>
                    <input type="checkbox" checked={allowSameEdge} onChange={e => setAllowSameEdge(e.target.checked)} /> 多重辺を許す
                </label>
                <hr style={{ width: '100%', borderTop: '1px solid #ccc' }} />

                <label style={{ fontWeight: 'bold' }}>
                    <input type="checkbox" checked={isAutomaton} onChange={e => setIsAutomaton(e.target.checked)} /> オートマトンモード
                </label>
                <div style={{ marginBottom: '5px' }}>
                <label>頂点の表示名: </label>
                    <select value={labelType} onChange={e => setLabelType(e.target.value as 'index' | 'name')} style={{ width: '100%' }}>
                        <option value="index">インデックス (0, 1...)</option>
                        <option value="name">状態名 (q_0, q_1...)</option>
                    </select>
                </div>
                
                <div style={{ opacity: isAutomaton ? 1 : 0.5, pointerEvents: isAutomaton ? 'auto' : 'none', display: 'flex', flexDirection: 'column', gap: '10px' }}>
                    <div>
                        <label>初期状態 (Start): </label>
                        <input type="text" value={startNode} onChange={e => setStartNode(e.target.value)} style={{ width: '50px' }} />
                    </div>
                    <div>
                        <label>受理状態 (Accept) カンマ区切り: </label>
                        <input type="text" value={acceptingNodes} onChange={e => setAcceptingNodes(e.target.value)} style={{ width: '100%' }} placeholder="例: 1, 2" />
                    </div>
                </div>
            </div>

            {/* 右側：キャンバス */}
            {isLoaded && (
                <GraphRenderer 
                    engine={engine} 
                    isDirected={isDirected} 
                    showWeights={showWeights}
                    isAutomaton={isAutomaton}
                    startNode={startNode}
                    acceptingNodes={acceptingNodes}
                    labelType={labelType}
                />
            )}
        </div>
    );
};