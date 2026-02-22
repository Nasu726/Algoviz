import React, { useEffect, useState } from 'react';
import { GraphRenderer } from '../components/visualizers/GraphRenderer'; 

interface GraphProps {
  engine: any;
  onBack: () => void;
}

export const GraphPage: React.FC<GraphProps> = ({ engine, onBack }) => {
    const [isLoaded, setIsLoaded] = useState(false);

    // ページが開かれた時の初期化処理
    useEffect(() => {
        if (!engine) return;
        // 1. C++エンジンを「グラフモード」に切り替え
        engine.setAlgorithm("graph");        
        setIsLoaded(true);
    }, [engine]);

    return (
        <div style={{ padding: "50px 60px", fontFamily: 'sans-serif' }}>
            <h2>Graph Visualizer 統合版</h2>
            <button onClick={onBack} style={{ padding: '8px 16px', marginBottom: '20px' }}>
                ◀ 戻る
            </button>

            {/* エンジンとデータが揃ったら描画コンポーネントを表示 */}
            {isLoaded ? (
                <div>
                    {/* ★ GraphRenderer に engine を渡します */}
                    <GraphRenderer engine={engine} />
                </div>
            ) : (
                <p>エンジンをロード中...</p>
            )}
        </div>
    )
}