import React from 'react';
import { Section, Check, NumberInput } from './panelParts';
import type { GraphSettings, GraphVariant } from './types';
import { isTraversal } from './types';

interface Props {
    variant: GraphVariant;
    settings: GraphSettings;
    update: (patch: Partial<GraphSettings>) => void;
    maxNodes: number;
    onGenerateRandom: () => void;
    onGenerateComplete: () => void;
    onGenerateFromText: () => void;
    compact?: boolean;
}

// 実行前に決める設定。グラフの作り方と見た目。
export const GraphSetupPanel: React.FC<Props> = ({
    variant, settings, update, maxNodes,
    onGenerateRandom, onGenerateComplete, onGenerateFromText,
    compact,
}) => {
    const s = settings;
    const fontSize = compact ? '12px' : '13px';
    const button: React.CSSProperties = { padding: '8px', cursor: 'pointer' };
    // 横に並べて、サイドバーをスクロールせずに使える高さに収める
    const genButton: React.CSSProperties = {
        ...button, flex: 1, minWidth: 0, fontSize: '12px', padding: '8px 4px',
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '18px', fontSize }}>
            <Section title="グラフ生成">
                <div>
                    頂点数 V: <NumberInput value={s.nodeCount} max={maxNodes} onChange={(v) => update({ nodeCount: v })} />
                    <span style={{ marginLeft: '10px' }}>
                        辺の数 E: <NumberInput value={s.edgeCount} onChange={(v) => update({ edgeCount: v })} />
                    </span>
                </div>

                <Check checked={s.isDirected} onChange={(v) => update({ isDirected: v })}>
                    有向グラフ
                </Check>
                <Check checked={s.connected} onChange={(v) => update({ connected: v })}>
                    連結なグラフにする
                </Check>
                <Check checked={s.weighted} onChange={(v) => update({ weighted: v })}>
                    重み付きグラフ
                </Check>
                <Check checked={s.allowSelfLoop} onChange={(v) => update({ allowSelfLoop: v })}>
                    自己ループを許す
                </Check>
                <Check checked={s.allowSameEdge} onChange={(v) => update({ allowSameEdge: v })}>
                    多重辺を許す
                </Check>
                <Check checked={s.useNodeWeights} onChange={(v) => update({ useNodeWeights: v })}>
                    頂点の重みを入力する
                </Check>

                <div style={{ display: 'flex', gap: '8px' }}>
                    <button onClick={onGenerateRandom} style={genButton}>ランダム生成</button>
                    <button onClick={onGenerateComplete} style={genButton}>完全グラフ生成</button>
                </div>
            </Section>

            <Section title="グラフ入力">
                <textarea
                    value={s.inputBuffer}
                    onChange={(e) => update({ inputBuffer: e.target.value })}
                    style={{
                        width: '100%', height: compact ? '90px' : '120px',
                        fontFamily: 'monospace', whiteSpace: 'pre',
                        resize: 'vertical', boxSizing: 'border-box',
                    }}
                    placeholder={s.useNodeWeights
                        ? '頂点数 辺数\n頂点0の重み 頂点1の重み ...\n始点 終点 (重み)\n...'
                        : '頂点数 辺数\n始点 終点 (重み)\n始点 終点 (重み)\n...'}
                />
                <button onClick={onGenerateFromText} style={button}>📝 テキストから生成</button>
            </Section>

            {(s.weighted || variant === 'plain') && (
                <Section title="表示">
                    {s.weighted && (
                        <Check checked={s.showWeights} onChange={(v) => update({ showWeights: v })}>
                            {variant === 'dijkstra' ? '重みと距離を表示' : '重みを表示'}
                        </Check>
                    )}
                    {/* レイアウトが収束する過程はアルゴリズムと無関係なので、遊び場だけに置く */}
                    {variant === 'plain' && (
                        <Check checked={s.skipExtension} onChange={(v) => update({ skipExtension: v })}>
                            展開アニメーションを飛ばす
                        </Check>
                    )}
                </Section>
            )}

            {isTraversal(variant) && (
                <p style={{ margin: 0, fontSize: '12px', color: '#78909c', lineHeight: 1.6 }}>
                    頂点数の上限は {maxNodes} です。連結を指定したとき、辺の数が
                    V-1 に満たなければ V-1 まで増やします。
                </p>
            )}
        </div>
    );
};
