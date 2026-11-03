import React from 'react';
import { PlaybackControls } from '../ui/PlaybackControls';
import { OpQueue } from '../ui/OpQueue';
import { NODE_STROKE } from '../visualizers/PixiGraphApp';
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

const lowbit = (i: number) => i & -i;

// "add 3 2" / "sum 7" の数の部分
const opNumbers = (op: string | undefined) => (op ?? '').split(' ').slice(1).map(Number);

const clamp = (v: number, lo: number, hi: number) => Math.min(Math.max(v, lo), hi);

// Fenwick 木の再生コントロールと実行状態。
export const FenwickPanel: React.FC<Props> = ({
    state, isPlaying, delay, setDelay,
    onReset, onPlayPause, onStepBack, onStepNext, onRunToEnd,
    horizontal, compact,
}) => {
    const dim = { color: '#90a4ae' };
    const fontSize = compact ? '12px' : '13px';

    const phase = state?.phase ?? 'build';
    const ops = state?.ops ?? [];
    const opIndex = state?.opIndex ?? 0;
    const ft = state?.fenTree ?? [];
    const n = ft.length;
    const box = (i: number) => ft[i - 1];
    const k = state?.lastIndex ?? 0;
    const next = state?.nextIndex ?? 0;
    const [opI = 0, opX = 0] = opNumbers(ops[opIndex]);

    const statusText = (() => {
        if (state?.finished) return 'すべての操作を終えました';
        switch (state?.lastKind ?? '') {
            case 'build': {
                const from = state?.lastFrom ?? 0;
                return `ft[${from}] を ft[${k}] に足す: ${box(k) - box(from)} + ${box(from)} = ${box(k)}`;
            }
            case 'add': {
                const first = k === clamp(opI, 1, n) ? `a[${k}] と ` : '';
                const step = `${k} + ${lowbit(k)} = ${next}`;
                return `${first}ft[${k}] に ${opX} を足す: ${box(k) - opX} + ${opX} = ${box(k)}` +
                    (next <= n ? `。次は ${step}` : `。${step} は ${n} を超えるので終わり`);
            }
            case 'sum': {
                const step = `${k} − ${lowbit(k)} = ${next}`;
                return `ft[${k}] (a[${k - lowbit(k) + 1}]〜a[${k}] の和) を足す` +
                    (next > 0 ? `。次は ${step}` : `。${step} なので終わり`);
            }
            case 'empty': return `sum ${clamp(opI, 0, n)} は何も足さない`;
            default:      return 'ft[i] を ft[i + lowbit(i)] に足して、木を組み立てます';
        }
    })();

    // 答えは sum の最中と、最後の操作が sum で終わったときに出す
    const lastOpIsSum = ops.length > 0 && ops[ops.length - 1].startsWith('sum');
    const showsSum = phase === 'sum' || (phase === 'done' && lastOpIsSum);
    const shownOp = phase === 'done' ? ops[ops.length - 1] : ops[opIndex];
    const shownTarget = clamp(opNumbers(shownOp)[0] ?? 0, 0, n);
    const taken = state?.taken ?? [];
    const answerText = taken.length === 0 ? '0'
        : taken.length === 1 ? `ft[${taken[0]}] = ${state?.answer}`
        : `${taken.map((t) => `ft[${t}]`).join(' + ')} = ${taken.map(box).join(' + ')} = ${state?.answer}`;

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
            {showsSum && (
                <div>
                    <b>a[1]〜a[{shownTarget}] の和</b>: {answerText}
                    {phase === 'sum' && !state?.opComplete && <span style={dim}>{'　'}(途中)</span>}
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
            {phase === 'build' ? (
                <>
                    <Swatch color={NODE_STROKE[1]} label="この手で足した先" />
                    <Swatch color={NODE_STROKE[5]} label="この手で読んだ箱 / それまでに足した先" />
                </>
            ) : showsSum ? (
                <>
                    <Swatch color={NODE_STROKE[1]} label="この手で足した箱" />
                    <Swatch color={NODE_STROKE[5]} label="この操作で足した箱" />
                </>
            ) : (
                <>
                    <Swatch color={NODE_STROKE[1]} label="この手で書き換えた箱" />
                    <Swatch color={NODE_STROKE[5]} label="この操作で書き換えた箱" />
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
