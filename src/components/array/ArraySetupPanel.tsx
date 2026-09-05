import React from 'react';
import { Section, NumberInput } from '../graph/panelParts';

interface Props {
    valueText: string;
    setValueText: (v: string) => void;
    count: string;
    setCount: (v: string) => void;
    maxValues: number;
    onApply: () => void;
    onGenerateRandom: () => void;
    compact?: boolean;
}

// 実行前に決める設定。何を並べるか。
export const ArraySetupPanel: React.FC<Props> = ({
    valueText, setValueText, count, setCount, maxValues,
    onApply, onGenerateRandom, compact,
}) => {
    const fontSize = compact ? '12px' : '13px';
    const button: React.CSSProperties = { padding: '8px', cursor: 'pointer' };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '18px', fontSize }}>
            <Section title="並べる値">
                <textarea
                    value={valueText}
                    onChange={(e) => setValueText(e.target.value)}
                    style={{
                        width: '100%', height: compact ? '70px' : '90px',
                        fontFamily: 'monospace', resize: 'vertical', boxSizing: 'border-box',
                    }}
                    placeholder="5 2 9 1 7 3 8 4"
                />
                <button onClick={onApply} style={button}>📝 この値で並べ直す</button>
            </Section>

            <Section title="ランダム生成">
                <div>
                    個数: <NumberInput value={count} max={maxValues} onChange={setCount} />
                </div>
                <button onClick={onGenerateRandom} style={button}>ランダム生成</button>
            </Section>

            <p style={{ margin: 0, fontSize: '12px', color: '#78909c', lineHeight: 1.6 }}>
                値は左から順に並びます。上限は {maxValues} 個です。
            </p>
        </div>
    );
};
