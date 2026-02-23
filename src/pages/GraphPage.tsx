import React, { useEffect, useState } from 'react';
import { GraphRenderer } from '../components/visualizers/GraphRenderer'; 

interface GraphProps {
  engine: any;
  onBack: () => void;
}

export const GraphPage: React.FC<GraphProps> = ({ engine, onBack }) => {
    const [isLoaded, setIsLoaded] = useState(false);
    const [isHorizontal, setIsHorizontal] = useState(true); // ★ 向きのState

    useEffect(() => {
        if (!engine) return;
        engine.setAlgorithm("graph");
        // 初期状態をロード
        engine.load(isHorizontal ? "horizontal" : "vertical", "");
        setIsLoaded(true);
    }, [engine]); // ※ 初回のみ

    // ★ ボタンが押された時に向きを切り替えてエンジンに伝える関数
    const toggleOrientation = () => {
        const newOrientation = !isHorizontal;
        setIsHorizontal(newOrientation);
        engine.load(newOrientation ? "horizontal" : "vertical", "");
    };

    return (
        <div style={{ padding: "20px 60px", fontFamily: 'sans-serif', display: "flex", flexDirection: "row" }}>
            <div style={{ display: 'flex', gap: '10px', marginBottom: '20px', flexDirection: "column" }}>
                <h2>Graph Visualizer 統合版</h2>
            
                <button onClick={onBack} style={{ padding: '8px 16px' }}>
                    ◀ 戻る
                </button>
                {/* ★ 向き切り替えボタンを追加 */}
                <button 
                    onClick={toggleOrientation} 
                    style={{ padding: '8px 16px', cursor: 'pointer', backgroundColor: '#3498db', color: '#fff', border: 'none', borderRadius: '4px' }}
                >
                    向きを変更: {isHorizontal ? "横長 (Horizontal)" : "縦長 (Vertical)"}
                </button>
            </div>

            {isLoaded ? (
                <div style={{ display: "flex"}}>
                    <GraphRenderer engine={engine} isDirected={true} />
                </div>
            ) : (
                <p>エンジンをロード中...</p>
            )}
        </div>
    )
}