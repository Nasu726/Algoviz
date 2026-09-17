import React from 'react';
import { Section, Check, NumberInput } from '../graph/panelParts';
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
    /** Union-Find だけ使う。工夫を使うか */
    options: { compress: boolean; bySize: boolean };
    setOptions: (v: { compress: boolean; bySize: boolean }) => void;
    compact?: boolean;
}

// 実行前に決める設定。何を並べるか。
export const ArraySetupPanel: React.FC<Props> = ({
    variant, targetText, setTargetText, onApplyTarget,
    valueText, setValueText, count, setCount, maxValues,
    onApply, onGenerateRandom, options, setOptions, compact,
}) => {
    const fontSize = compact ? '12px' : '13px';
    const button: React.CSSProperties = { padding: '8px', cursor: 'pointer' };
    const search = isSearch(variant);
    const ops = usesOps(variant);
    const deque = variant === 'deque';
    const unionFind = variant === 'unionfind';

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
                        ? (unionFind ? 'union 0 1 union 2 3 find 3'
                            : deque ? 'pushR 5 pushL 2 popL popR' : 'push 5 push 2 pop pop')
                        : '5 2 9 1 7 3 8 4'}
                />
                <button onClick={onApply} style={button}>
                    📝 {ops ? 'この並びで動かす'
                        : search ? 'この値で作り直す' : 'この値で並べ直す'}
                </button>
            </Section>

            {unionFind && (
                <Section title="工夫">
                    <Check checked={options.compress} onChange={(v) => setOptions({ ...options, compress: v })}>
                        経路圧縮
                    </Check>
                    <Check checked={options.bySize} onChange={(v) => setOptions({ ...options, bySize: v })}>
                        union by size
                    </Check>
                </Section>
            )}

            <Section title="ランダム生成">
                <div>
                    {ops ? '操作の数' : '個数'}:{' '}
                    <NumberInput value={count} max={maxValues} onChange={setCount} />
                </div>
                <button onClick={onGenerateRandom} style={button}>ランダム生成</button>
            </Section>

            <p style={{ margin: 0, fontSize: '12px', color: '#78909c', lineHeight: 1.6 }}>
                {ops
                    ? (unionFind
                        ? `union に続けて2つの番号、find に続けて1つの番号と書きます (番号は 0 始まり、19 まで)。上限は ${maxValues} 個です。`
                        : deque
                        ? `pushL / pushR に続けて値、popL / popR と書きます。上限は ${maxValues} 個です。`
                        : `push に続けて値、pop と書きます。上限は ${maxValues} 個です。`)
                    : `値は左から順に並びます。上限は ${maxValues} 個です。${variant === 'binary' ? ' ランダム生成は昇順で作ります。' : ''}${variant === 'bucket' ? ' 値は 0〜9 です (外れた値は端に寄せます)。' : ''}${variant === 'radix' ? ' 値は 0〜15 です (外れた値は端に寄せます)。' : ''}`}
            </p>
        </div>
    );
};
