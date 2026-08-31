import React from 'react';
import { Section, NumberInput } from './panelParts';
import type { GraphSettings } from './types';

interface Props {
    settings: GraphSettings;
    update: (patch: Partial<GraphSettings>) => void;
    maxNodes: number;
    maxAlphabet: number;
    onGenerateRandom: () => void;
    onGenerateFromText: () => void;
    startState: string;
    setStartState: (v: string) => void;
    acceptingStates: string;
    setAcceptingStates: (v: string) => void;
    compact?: boolean;
}

// オートマトンの設定。一般グラフとは決めることがほとんど重ならないので、
// GraphSetupPanel に条件分岐を積むのではなく別の部品にしている。
// 重み・連結・多重辺・完全グラフはオートマトンには無い概念なので出さない。
export const AutomatonSetupPanel: React.FC<Props> = ({
    settings, update, maxNodes, maxAlphabet,
    onGenerateRandom, onGenerateFromText,
    startState, setStartState, acceptingStates, setAcceptingStates,
    compact,
}) => {
    const fontSize = compact ? '12px' : '13px';
    const button: React.CSSProperties = { padding: '8px', cursor: 'pointer' };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '18px', fontSize }}>
            <Section title="オートマトン生成">
                <div>
                    状態数: <NumberInput
                        value={settings.nodeCount}
                        max={maxNodes}
                        onChange={(v) => update({ nodeCount: v })}
                    />
                </div>
                <label style={{ display: 'block' }}>
                    アルファベット:
                    <input
                        type="text"
                        value={settings.alphabet}
                        placeholder="例: ab"
                        onChange={(e) => update({
                            alphabet: e.target.value.replace(/\s/g, '').slice(0, maxAlphabet),
                        })}
                        style={{ width: '100%', marginTop: '4px' }}
                    />
                </label>
                <button onClick={onGenerateRandom} style={button}>ランダム生成</button>
            </Section>

            <Section title="遷移規則">
                <textarea
                    value={settings.inputBuffer}
                    onChange={(e) => update({ inputBuffer: e.target.value })}
                    style={{
                        width: '100%', height: compact ? '90px' : '120px',
                        fontFamily: 'monospace', whiteSpace: 'pre',
                        resize: 'vertical', boxSizing: 'border-box',
                    }}
                    placeholder={'状態数 遷移数\n元の状態 先の状態 記号\n...'}
                />
                <button onClick={onGenerateFromText} style={button}>📝 テキストから生成</button>
            </Section>

            <Section title="状態">
                <div>
                    初期状態: <NumberInput value={startState} max={maxNodes - 1} onChange={setStartState} />
                </div>
                <label style={{ display: 'block' }}>
                    受理状態 (カンマ区切り):
                    <input
                        type="text" value={acceptingStates} placeholder="例: 1, 2"
                        onChange={(e) => setAcceptingStates(e.target.value)}
                        style={{ width: '100%', marginTop: '4px' }}
                    />
                </label>
            </Section>
        </div>
    );
};
