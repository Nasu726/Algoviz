import React, { useEffect, useState } from 'react';
import { GraphRenderer } from '../components/visualizers/GraphRenderer'; 

interface GraphProps {
  engine: any;
  onBack: () => void;
}

export const GraphPage: React.FC<GraphProps> = ({ engine, onBack }) => {
    const [isLoaded, setIsLoaded] = useState(false);
    const [isHorizontal, setIsHorizontal] = useState(true);

    // テスト環境用のState
    const [nodeCount, setNodeCount] = useState(5);
    const [edgeCount, setEdgeCount] = useState(7);
    const [isDirected, setIsDirected] = useState(true);
    const [showWeights, setShowWeights] = useState(true);

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
        engine.load(isHorizontal ? "horizontal" : "vertical", "");
        setIsLoaded(true);
    }, [engine]);

    const handleGenerateRandom = () => {
        const skip = skipExtension ? 1 : 0;
        const selfLoop = allowSelfLoop ? 1 : 0;
        const sameEdge = allowSameEdge ? 1 : 0;
        const isDir = isDirected ? 1 : 0;
        engine.load(
            isHorizontal ? "horizontal" : "vertical",
            `random ${nodeCount} ${edgeCount} ${skip} ${selfLoop} ${sameEdge} ${isDir}`
        );
    };

    const handleGenerateComplete = () => {
        const skip = skipExtension ? 1 : 0;
        const isDir = isDirected ? 1 : 0;
        engine.load(
            isHorizontal ? "horizontal" : "vertical", 
            `complete ${nodeCount} ${skip} ${isDir}`
        );
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
                    <input type="number" value={nodeCount} onChange={e => setNodeCount(Number(e.target.value))} style={{ width: '50px' }} />
                </div>
                <div>
                    <label>辺の数 (E): </label>
                    <input type="number" value={edgeCount} onChange={e => setEdgeCount(Number(e.target.value))} style={{ width: '50px' }} />
                </div>
                
                <button onClick={handleGenerateRandom} style={{ padding: '8px', cursor: 'pointer' }}>🎲 ランダム生成</button>
                <button onClick={handleGenerateComplete} style={{ padding: '8px', cursor: 'pointer' }}>🕸️ 完全グラフ生成 (Vのみ使用)</button>

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
                />
            )}
        </div>
    );
};