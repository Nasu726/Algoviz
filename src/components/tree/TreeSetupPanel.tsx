import React from 'react';
import { Section, NumberInput, Check } from '../graph/panelParts';
import { usesWords, usesText, usesOps, defaultOps } from './types';
import type { TreeVariant, Monoid } from './types';

interface Props {
    variant: TreeVariant;
    /** ヒープのときだけ使う。大きい値を上にするか */
    maxHeap: boolean;
    setMaxHeap: (v: boolean) => void;
    /** B木のときだけ使う。1つの節点が持てる子の数の上限 */
    order: number;
    setOrder: (v: number) => void;
    /** セグメント木 / Fenwick 木のときだけ使う。操作の並び。集約はセグメント木だけ */
    opsText: string;
    setOpsText: (v: string) => void;
    monoid: Monoid;
    setMonoid: (v: Monoid) => void;
    valueText: string;
    setValueText: (v: string) => void;
    count: string;
    setCount: (v: string) => void;
    maxValues: number;
    onApply: () => void;
    onGenerateRandom: () => void;
    compact?: boolean;
}

// 実行前に決める設定。何を挿入するか。
export const TreeSetupPanel: React.FC<Props> = ({
    variant, maxHeap, setMaxHeap, order, setOrder,
    opsText, setOpsText, monoid, setMonoid,
    valueText, setValueText, count, setCount, maxValues,
    onApply, onGenerateRandom, compact,
}) => {
    const fontSize = compact ? '12px' : '13px';
    const button: React.CSSProperties = { padding: '8px', cursor: 'pointer' };
    const words = usesWords(variant);
    const text = usesText(variant);
    const ops = usesOps(variant);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '18px', fontSize }}>
            <Section title={text ? '数える文章' : words ? '挿入する単語'
                            : ops ? '配列'
                            : variant === 'heapsort' ? '並べる値' : '挿入する値'}>
                <textarea
                    value={valueText}
                    onChange={(e) => setValueText(e.target.value)}
                    style={{
                        width: '100%', height: compact ? '70px' : '90px',
                        fontFamily: 'monospace', resize: 'vertical', boxSizing: 'border-box',
                    }}
                    placeholder={text ? 'abracadabra' : words ? 'to tea ten ted'
                                 : ops ? '5 3 8 1 6 2 7 4' : '50 30 70 20 40'}
                />
                {ops && (
                    <>
                        <div style={{ fontWeight: 'bold', color: '#37474f' }}>操作の並び</div>
                        <textarea
                            value={opsText}
                            onChange={(e) => setOpsText(e.target.value)}
                            // 1操作を1行に書く。ランダム生成の5操作が収まる高さ
                            style={{
                                width: '100%', height: compact ? '100px' : '120px',
                                fontFamily: 'monospace', resize: 'vertical', boxSizing: 'border-box',
                            }}
                            placeholder={defaultOps[variant]}
                        />
                    </>
                )}
                <button onClick={onApply} style={button}>
                    📝 {text ? 'この文章で作り直す' : words ? 'この単語で作り直す'
                        : ops ? 'この配列と操作で動かす' : 'この値で作り直す'}
                </button>
            </Section>

            {/* どれも正しいセグメント木で、分類では決まらないので選ばせる */}
            {variant === 'segtree' && (
                <Section title="集約">
                    <select value={monoid} onChange={(e) => setMonoid(e.target.value as Monoid)}
                            style={{ padding: '4px', fontSize }}>
                        <option value="sum">和</option>
                        <option value="min">最小</option>
                        <option value="max">最大</option>
                    </select>
                </Section>
            )}

            {/* どちらも正しいヒープで、分類では決まらないので選ばせる */}
            {variant === 'heap' && (
                <Section title="ヒープの向き">
                    <Check checked={maxHeap} onChange={setMaxHeap}>
                        最大ヒープ (大きい値が上)
                    </Check>
                </Section>
            )}

            {/* どの次数も正しい B木で、分類では決まらないので選ばせる */}
            {variant === 'btree' && (
                <Section title="次数 (1つの節点が持てる子の数)">
                    <select value={order} onChange={(e) => setOrder(Number(e.target.value))}
                            style={{ padding: '4px', fontSize }}>
                        <option value={3}>3</option>
                        <option value={4}>4</option>
                        <option value={5}>5</option>
                    </select>
                </Section>
            )}

            {/* 単語のランダム生成に意味のある形が無いので、値のときだけ出す */}
            {!words && !text && (
                <Section title="ランダム生成">
                    <div>
                        個数: <NumberInput value={count} max={maxValues} onChange={setCount} />
                    </div>
                    <button onClick={onGenerateRandom} style={button}>ランダム生成</button>
                </Section>
            )}

            <p style={{ margin: 0, fontSize: '12px', color: '#78909c', lineHeight: 1.6 }}>
                {text
                    ? `文章に出てくる文字を数えます。空白は数えません。文字の種類の上限は ${maxValues} です。`
                    : variant === 'fenwick'
                    ? `配列は左から添字 1, 2, 3, … です。上限は ${maxValues} 個です。操作は1行に1つ、add に続けて添字と足す値、sum に続けて添字 i と書きます (a[1] から a[i] までの和)。`
                    : ops
                    ? `配列は左から添字 0, 1, 2, … です。上限は ${maxValues} 個です。操作は1行に1つ、update に続けて添字と値、query に続けて区間の両端 l r と書きます (半開区間 [l, r))。`
                    : words
                    ? `単語は空白で区切って、左から順に挿入します。上限は ${maxValues} 個です。`
                    : variant === 'heapsort'
                    ? `値はその並びのまま木に置きます。上限は ${maxValues} 個です。`
                    : `値は左から順に挿入します。上限は ${maxValues} 個です。`}
            </p>
        </div>
    );
};
