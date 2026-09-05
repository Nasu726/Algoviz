import React from 'react';
import { PlaybackControls } from '../ui/PlaybackControls';
import { NODE_STROKE } from '../visualizers/PixiGraphApp';
import { Section, Swatch } from '../graph/panelParts';
import { settledLabel, settledCountLabel } from './types';
import type { ArrayVariant } from './types';
import type { GraphState } from '../../types/engine';

interface Props {
    variant: ArrayVariant;
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

// 今どの手を打ったか。ソートごとに見どころが違うので言葉を変える。
const statusOf = (variant: ArrayVariant, state: GraphState | null): string => {
    if (state?.finished) return '並び終えました';
    if (variant === 'selection') {
        if (state?.swapped) return '見つけた最小の値を先頭と入れ替えました';
        return state?.swapping ? '探し終えたので先頭と入れ替えます' : '最小の値を探しています';
    }
    if (variant === 'insertion') {
        if ((state?.droppedAt ?? -1) >= 0) return '入る場所が見つかったので差し込みました';
        if (state?.swapped) return '左隣の方が大きいので、右へずらしました';
        if ((state?.heldValue ?? -1) >= 0) return '次の値を取り出しました';
        return '左隣と比べています';
    }
    if (variant === 'quick') {
        if (state?.skippedRange) {
            return '取り出した範囲に並べるものがありませんでした';
        }
        if (state?.placingPivot) return '見終わったので、基準の値を境目へ動かします';
        if ((state?.rangeLo ?? -1) < 0) return '次に並べる範囲を取り出します';
        if (state?.swapped) return '基準より小さいので、左の並びへ入れました';
        return '基準の値と比べています';
    }
    if (variant === 'merge') {
        if (state?.lonelyRun) return '相方がいないので、この並びはそのままです';
        if (state?.copyingBack) return '併合が終わったので、下の段から書き戻します';
        if ((state?.rangeLo ?? -1) < 0) return '次に併合する2つの並びを取り出します';
        return '2つの先頭を比べ、小さい方を下の段へ移しました';
    }
    if (variant === 'shaker') {
        const dir = state?.movingRight === false ? '左へ' : '右へ';
        return state?.swapped ? `大小が逆だったので入れ替えました (${dir}走査中)`
                              : `${dir}向かって隣どうしを比べています`;
    }
    return state?.swapped ? '大小が逆だったので入れ替えました' : '隣どうしを比べています';
};

// 見ながら操作するもの。再生コントロールと進行状況。
export const ArrayPanel: React.FC<Props> = ({
    variant, state, isPlaying, delay, setDelay,
    onReset, onPlayPause, onStepBack, onStepNext, onRunToEnd,
    horizontal, compact,
}) => {
    const fontSize = compact ? '12px' : '13px';

    const total = (state?.values ?? []).length;
    const settled = state?.settledCount ?? 0;
    const pending = state?.pendingRanges ?? 0;

    const progress = (
        <div style={{ fontSize, lineHeight: 1.7, minWidth: 0 }}>
            <div>
                <b>{settledCountLabel(variant)}</b>: {settled}
                <span style={{ color: '#90a4ae' }}> / {total}</span>
            </div>
            {variant === 'quick' && !state?.finished && (
                <div>
                    <b>まだ並べていない範囲</b>: {pending}
                </div>
            )}
            {variant === 'merge' && !state?.finished && (
                <div>
                    <b>今の段で併合する長さ</b>: {state?.runWidth ?? 1}
                </div>
            )}
            <div style={{ marginTop: '6px', fontWeight: 'bold',
                          color: state?.finished ? '#27ae60' : '#78909c' }}>
                {statusOf(variant, state)}
            </div>
        </div>
    );

    const legend = (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px' }}>
            {variant === 'selection' ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="今見ている値" />
                    <Swatch color={NODE_STROKE[1]} label="今のところ最小" />
                    <Swatch color={NODE_STROKE[4]} label="入れ替えた2つ" />
                </>
            ) : variant === 'quick' ? (
                <>
                    <Swatch color={NODE_STROKE[6]} label="今並べている範囲" />
                    <Swatch color={NODE_STROKE[1]} label="基準の値" />
                    <Swatch color={NODE_STROKE[2]} label="今比べている値" />
                    <Swatch color={NODE_STROKE[5]} label="基準より小さいと分かった部分" />
                    <Swatch color={NODE_STROKE[4]} label="入れ替えた2つ" />
                </>
            ) : variant === 'merge' ? (
                <>
                    <Swatch color={NODE_STROKE[6]} label="今併合している2つ" />
                    <Swatch color={NODE_STROKE[5]} label="左の並びの残り" />
                    <Swatch color={NODE_STROKE[4]} label="下の段へ移した値" />
                </>
            ) : variant === 'insertion' ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="取り出して空いたマス" />
                    <Swatch color={NODE_STROKE[4]} label="動かした値" />
                </>
            ) : (
                <>
                    <Swatch color={NODE_STROKE[2]} label="比べている2つ" />
                    <Swatch color={NODE_STROKE[4]} label="入れ替えた2つ" />
                </>
            )}
            <Swatch color={NODE_STROKE[3]} label={settledLabel(variant)} />
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
                <div style={{ flex: '1 1 200px', minWidth: 0 }}>{progress}</div>
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
