import React from 'react';
import { PlaybackControls } from '../ui/PlaybackControls';
import { OpQueue } from '../ui/OpQueue';
import { NODE_STROKE, EDGE_COLOR } from '../visualizers/PixiGraphApp';
import { Section, Swatch } from '../graph/panelParts';
import type { GraphState } from '../../types/engine';

interface Props {
    state: GraphState | null;

    isPlaying: boolean;
    delay: number;
    setDelay: (v: number) => void;
    onReset: () => void;
    onPlayPause: () => void;
    onStepBack: () => void;
    onStepNext: () => void;
    onRunToEnd: () => void;

    /** 横帯として置くとき。中身を横並びにする */
    horizontal?: boolean;
    compact?: boolean;
}

const IDENTITY = { sum: '0', min: '∞', max: '-∞' };

// 値を ⊕ でつないだ式。和は a + b + c、最小 / 最大は min(a, b, c)
const expression = (monoid: 'sum' | 'min' | 'max', values: (number | null)[]) =>
    monoid === 'sum' ? values.join(' + ') : `${monoid}(${values.join(', ')})`;

const range = (l: number, r: number) => `[${l}, ${r})`;

// セグメント木の再生コントロールと実行状態。
export const SegTreePanel: React.FC<Props> = ({
    state, isPlaying, delay, setDelay,
    onReset, onPlayPause, onStepBack, onStepNext, onRunToEnd,
    horizontal, compact,
}) => {
    const dim = { color: '#90a4ae' };
    const fontSize = compact ? '12px' : '13px';

    const monoid = state?.monoid ?? 'sum';
    const phase = state?.phase ?? 'build';
    const ops = state?.ops ?? [];
    const opIndex = state?.opIndex ?? 0;
    const segL = state?.segL ?? [];
    const segR = state?.segR ?? [];
    const segValue = state?.segValue ?? [];
    const ql = state?.queryL ?? 0, qr = state?.queryR ?? 0;
    const u = state?.lastNode ?? -1;
    const nodeRange = u >= 0 ? range(segL[u], segR[u]) : '';

    const statusText = (() => {
        if (state?.finished) return 'すべての操作を終えました';
        const kind = state?.lastKind ?? '';
        const left = state?.segLeft?.[u] ?? -1, right = state?.segRight?.[u] ?? -1;
        const computed = `${expression(monoid, [segValue[left], segValue[right]])} = ${segValue[u]}`;
        switch (kind) {
            case 'build':     return `${nodeRange} を計算: ${computed}`;
            case 'recompute': return `${nodeRange} を計算し直し: ${computed}`;
            case 'leaf':      return `葉 ${segL[u]} を ${segValue[u]} に書き換えました`;
            case 'in':        return `${nodeRange} は ${range(ql, qr)} に全部入る。${segValue[u]} を答えに使います`;
            case 'out':       return ql >= qr ? `${range(ql, qr)} は空の区間です`
                                              : `${nodeRange} は ${range(ql, qr)} から外れる`;
            case 'partial':   return `${nodeRange} は ${range(ql, qr)} に一部だけかかる。子へ降ります`;
            default:          return '下の節点から順に計算して、木を組み立てます';
        }
    })();

    // 答えは query の最中と、最後の操作が query で終わったときに出す
    const lastOpIsQuery = ops.length > 0 && ops[ops.length - 1].startsWith('query');
    const showsQuery = phase === 'query' || (phase === 'done' && lastOpIsQuery);
    const taken = (state?.takenNodes ?? []).map((t) => segValue[t]);
    const answerText = taken.length === 0
        ? IDENTITY[monoid]
        : taken.length === 1 ? `${taken[0]}`
        : `${expression(monoid, taken)} = ${state?.answer}`;

    const progress = (
        <div style={{ fontSize, lineHeight: 1.7, minWidth: 0 }}>
            {/* 組み立てのあいだは、どの操作もまだ始まっていない */}
            <OpQueue ops={ops} opIndex={phase === 'build' ? -1 : opIndex} compact={compact} />
            <div>
                <b>今の操作</b>:{' '}
                {phase === 'build' ? '組み立て'
                    : phase === 'done' ? <span style={dim}>なし</span>
                    : ops[opIndex]}
            </div>
            {showsQuery && (
                <div>
                    <b>{range(ql, qr)} の答え</b>: {answerText}
                    {phase === 'query' && !state?.opComplete && <span style={dim}>{'　'}(途中)</span>}
                </div>
            )}
            <div style={{ marginTop: '6px', fontWeight: 'bold',
                          color: state?.finished ? '#27ae60' : '#78909c' }}>
                {statusText}
            </div>
        </div>
    );

    const legend = (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px' }}>
            {showsQuery ? (
                <>
                    <Swatch color={NODE_STROKE[5]} label="全部入る (答えに使う)" />
                    <Swatch color={NODE_STROKE[1]} label="一部かかる (子へ降りる)" />
                    <Swatch color={NODE_STROKE[3]} label="外れる" />
                    <Swatch color={EDGE_COLOR[2]} label="この手で降りた枝" isEdge />
                </>
            ) : (
                <>
                    <Swatch color={NODE_STROKE[5]} label="この操作で計算した節点" />
                    <Swatch color={EDGE_COLOR[2]} label="この手で読んだ子" isEdge />
                </>
            )}
        </div>
    );

    const controls = (
        <PlaybackControls
            isPlaying={isPlaying}
            ready={!!state}
            canStepBack={!!state?.canStepBack}
            delay={delay}
            loadLabel="最初から"
            onLoad={onReset}
            onPlayPause={onPlayPause}
            onStepBack={onStepBack}
            onStepNext={onStepNext}
            onRunToEnd={onRunToEnd}
            onDelayChange={setDelay}
            vertical={!horizontal}
            compact={compact}
        />
    );

    if (horizontal) {
        return (
            <div style={{
                display: 'flex', flexWrap: 'wrap', alignItems: 'flex-start',
                gap: '16px 24px', padding: '10px 15px',
                borderTop: '1px solid #ddd', background: '#f8f9fa',
            }}>
                <div style={{ flex: '1 1 260px', minWidth: 0 }}>{controls}</div>
                <div style={{ flex: '1 1 260px', minWidth: 0 }}>{progress}</div>
                <div style={{ flex: '1 1 200px', minWidth: 0 }}>{legend}</div>
            </div>
        );
    }

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>
            <Section title="実行">{controls}</Section>
            <Section title="実行状態">
                {progress}
                {legend}
            </Section>
        </div>
    );
};
