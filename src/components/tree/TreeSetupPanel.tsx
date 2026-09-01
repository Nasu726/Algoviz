import React from 'react';
import { Section, NumberInput, Check } from '../graph/panelParts';
import { usesWords, usesText } from './types';
import type { TreeVariant } from './types';

interface Props {
    variant: TreeVariant;
    /** ヒープのときだけ使う。大きい値を上にするか */
    maxHeap: boolean;
    setMaxHeap: (v: boolean) => void;
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
    variant, maxHeap, setMaxHeap,
    valueText, setValueText, count, setCount, maxValues,
    onApply, onGenerateRandom, compact,
}) => {
    const fontSize = compact ? '12px' : '13px';
    const button: React.CSSProperties = { padding: '8px', cursor: 'pointer' };
    const words = usesWords(variant);
    const text = usesText(variant);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '18px', fontSize }}>
            <Section title={text ? '数える文章' : words ? '挿入する単語' : '挿入する値'}>
                <textarea
                    value={valueText}
                    onChange={(e) => setValueText(e.target.value)}
                    style={{
                        width: '100%', height: compact ? '70px' : '90px',
                        fontFamily: 'monospace', resize: 'vertical', boxSizing: 'border-box',
                    }}
                    placeholder={text ? 'abracadabra' : words ? 'to tea ten ted' : '50 30 70 20 40'}
                />
                <button onClick={onApply} style={button}>
                    📝 {text ? 'この文章で作り直す' : words ? 'この単語で作り直す' : 'この値で作り直す'}
                </button>
            </Section>

            {/* どちらも正しいヒープで、分類では決まらないので選ばせる */}
            {variant === 'heap' && (
                <Section title="ヒープの向き">
                    <Check checked={maxHeap} onChange={setMaxHeap}>
                        最大ヒープ (大きい値が上)
                    </Check>
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
                    : words
                    ? `単語は空白で区切って、左から順に挿入します。上限は ${maxValues} 個です。`
                    : `値は左から順に挿入します。上限は ${maxValues} 個です。`}
            </p>
        </div>
    );
};
