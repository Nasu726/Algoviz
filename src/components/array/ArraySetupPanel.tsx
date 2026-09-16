import React from 'react';
import { Section, NumberInput } from '../graph/panelParts';
import { isSearch, usesOps } from './types';
import type { ArrayVariant } from './types';

interface Props {
    variant: ArrayVariant;
    /** 探すものだけ使う。探す値 */
    targetText: string;
    setTargetText: (v: string) => void;
    onApplyTarget: () => void;
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
    variant, targetText, setTargetText, onApplyTarget,
    valueText, setValueText, count, setCount, maxValues,
    onApply, onGenerateRandom, compact,
}) => {
    const fontSize = compact ? '12px' : '13px';
    const button: React.CSSProperties = { padding: '8px', cursor: 'pointer' };
    const search = isSearch(variant);
    const ops = usesOps(variant);
    const deque = variant === 'deque';

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '18px', fontSize }}>
            {search && (
                <Section title="探す値">
                    <input
                        type="text"
                        inputMode="numeric"
                        value={targetText}
                        onChange={(e) => setTargetText(e.target.value)}
                        onBlur={onApplyTarget}
                        onKeyDown={(e) => { if (e.key === 'Enter') onApplyTarget(); }}
                        style={{ width: '80px', fontFamily: 'monospace', padding: '4px' }}
                    />
                    <button onClick={onApplyTarget} style={button}>🔍 この値を探す</button>
                </Section>
            )}

            <Section title={ops ? '操作の並び' : search ? '探される値' : '並べる値'}>
                <textarea
                    value={valueText}
                    onChange={(e) => setValueText(e.target.value)}
                    style={{
                        width: '100%', height: compact ? '70px' : '90px',
                        fontFamily: 'monospace', resize: 'vertical', boxSizing: 'border-box',
                    }}
                    placeholder={ops
                        ? (deque ? 'pushR 5 pushL 2 popL popR' : 'push 5 push 2 pop pop')
                        : '5 2 9 1 7 3 8 4'}
                />
                <button onClick={onApply} style={button}>
                    📝 {ops ? 'この並びで動かす'
                        : search ? 'この値で作り直す' : 'この値で並べ直す'}
                </button>
            </Section>

            <Section title="ランダム生成">
                <div>
                    {ops ? '操作の数' : '個数'}:{' '}
                    <NumberInput value={count} max={maxValues} onChange={setCount} />
                </div>
                <button onClick={onGenerateRandom} style={button}>ランダム生成</button>
            </Section>

            <p style={{ margin: 0, fontSize: '12px', color: '#78909c', lineHeight: 1.6 }}>
                {ops
                    ? (deque
                        ? `pushL / pushR に続けて値、popL / popR と書きます。上限は ${maxValues} 個です。`
                        : `push に続けて値、pop と書きます。上限は ${maxValues} 個です。`)
                    : `値は左から順に並びます。上限は ${maxValues} 個です。${variant === 'binary' ? ' ランダム生成は昇順で作ります。' : ''}`}
            </p>
        </div>
    );
};
