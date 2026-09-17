import React from 'react';
import { PlaybackControls } from '../ui/PlaybackControls';
import { NODE_STROKE } from '../visualizers/PixiGraphApp';
import { Section, Swatch } from '../graph/panelParts';
import { settledLabel, settledCountLabel, isSearch, usesOps } from './types';
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
    if (isSearch(variant)) {
        const found = state?.foundAt ?? -1;
        if (found >= 0) return `見つけました (${found} 番目)`;
        if (state?.finished) return 'この配列には無いと分かりました';
        if (variant === 'binary') {
            return (state?.midIndex ?? -1) < 0
                ? '真ん中を見て、範囲を半分にします'
                : '真ん中の値と比べ、半分を捨てました';
        }
        return '1つずつ見ています';
    }
    if (variant === 'unionfind') {
        if (state?.finished) return '操作の並びを流し終えました';
        if (state?.sameSet) return '根が同じなので、既に同じ集合です';
        if ((state?.linkedChild ?? -1) >= 0) {
            const sizes = state?.size ?? [];
            const root = state?.linkedRoot ?? 0, child = state?.linkedChild ?? 0;
            return `${root} の木 (${(sizes[root] ?? 0) - (sizes[child] ?? 0)} 個) の下に ${child} の木 (${sizes[child] ?? 0} 個) を付けました`;
        }
        if ((state?.compressed ?? -1) >= 0) {
            return `${state?.compressed} を根の直下に付け直しました (経路圧縮)`;
        }
        if ((state?.foundRoot ?? -1) >= 0) return `根は ${state?.foundRoot} でした`;
        if ((state?.climbedTo ?? -1) >= 0) {
            return `${state?.climbedFrom} から親の ${state?.climbedTo} へ上がりました`;
        }
        return '操作の並びを頭から流します';
    }
    if (usesOps(variant)) {
        if (state?.finished) return '操作の並びを流し終えました';
        if (state?.emptyPop) return '空なので、取り出せませんでした';
        const done = state?.opIndex ?? 0;
        const op = (state?.ops ?? [])[done - 1];
        if (!op) return '操作の並びを頭から流します';
        if (op.startsWith('push')) return `入れました (${op})`;
        return `取り出しました (${state?.poppedValue ?? 0})`;
    }
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
    if (variant === 'radix') {
        const place = `下から ${(state?.pass ?? 0) + 1} ビット目`;
        const bit = state?.bit ?? -1;
        if (state?.looked) return `${place}が 1 なので、残します`;
        if (state?.placed) {
            return bit === 0 ? `${place}が 0 なので、別配列の左から詰めました`
                             : '残った 1 を、別配列の 0 の後ろに詰めました';
        }
        if (state?.copied) return '別配列をまとめて元の配列へ戻しました';
        return `${place}が 0 の値を、見つけたそばから別配列へ移します`;
    }
    if (variant === 'bucket') {
        if ((state?.counted ?? -1) >= 0) return `${state?.counted} を数えました (頻度 +1)`;
        if ((state?.written ?? -1) >= 0) return `${state?.written} を配列へ書き出しました`;
        if (state?.sawZero) return `${state?.expandAt} の頻度は 0 なので、何も書きません`;
        return '配列の左から順に、値を数えます';
    }
    if (variant === 'merge') {
        if (state?.leafRange) return '1つ以下になったので、この範囲は並んでいます';
        if (state?.dividing) return '半分に分けました';
        if (state?.copyingBack) return '併合が終わりました。次に下の段から書き戻します';
        if (state?.swapped) return '2つの先頭を比べ、小さい方を下の段へ移しました';
        if ((state?.rangeLo ?? -1) >= 0) return '分けた2つが並んだので、併合します';
        return '次の範囲を取り出します';
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
    const search = isSearch(variant);
    const ops = usesOps(variant);
    const opList = state?.ops ?? [];
    const opIndex = state?.opIndex ?? 0;

    // 操作の並び。実行済み / 次に実行するもの を色で分ける
    const opQueue = (
        <div style={{ fontFamily: 'monospace', fontSize: compact ? '13px' : '15px',
                      wordBreak: 'break-all', lineHeight: 1.7 }}>
            {opList.length === 0
                ? <span style={{ color: '#90a4ae' }}>（操作がありません）</span>
                : opList.map((op, i) => (
                    <span key={i} style={{
                        marginRight: '10px',
                        color: i < opIndex ? '#90a4ae' : i === opIndex ? '#e74c3c' : '#000',
                        fontWeight: i === opIndex ? 'bold' : 'normal',
                    }}>{op}</span>
                ))}
        </div>
    );

    const progress = (
        <div style={{ fontSize, lineHeight: 1.7, minWidth: 0 }}>
            {variant === 'unionfind' ? (
                <>
                    {opQueue}
                    <div>
                        <b>集合の数</b>: {state?.setCount ?? 0}
                        <span style={{ color: '#90a4ae' }}> / {state?.elements ?? 0} 要素</span>
                    </div>
                </>
            ) : ops ? (
                <>
                    {opQueue}
                    <div>
                        <b>入っている数</b>: {state?.heldCount ?? 0}
                        <span style={{ color: '#90a4ae' }}>
                            {'　'}出した数: {state?.poppedCount ?? 0}
                        </span>
                    </div>
                </>
            ) : (
                <>
                    <div>
                        <b>{settledCountLabel(variant)}</b>: {settled}
                        <span style={{ color: '#90a4ae' }}> / {total}</span>
                    </div>
                    {search && (
                        <div>
                            <b>探す値</b>: {state?.target ?? 0}
                        </div>
                    )}
                    {variant === 'binary' && !state?.finished && (
                        <div>
                            <b>まだ見ていない場所</b>: {state?.rangeSize ?? 0}
                        </div>
                    )}
                    {variant === 'binary' && state?.sorted === false && (
                        <div style={{ color: '#e67e22' }}>
                            入力が昇順に並んでいません。二分探索は並んでいることを前提にしています
                        </div>
                    )}
                    {variant === 'quick' && !state?.finished && (
                        <div>
                            <b>まだ並べていない範囲</b>: {pending}
                        </div>
                    )}
                    {variant === 'merge' && !state?.finished && (
                        <div>
                            <b>まだ片付けていない範囲</b>: {state?.pendingTasks ?? 0}
                        </div>
                    )}
                    {variant === 'bucket' && (
                        <div>
                            <b>数えた個数</b>: {state?.countedTotal ?? 0}
                            <span style={{ color: '#90a4ae' }}> / {total}</span>
                        </div>
                    )}
                    {variant === 'radix' && !state?.finished && (
                        <div>
                            <b>今のビット</b>: 下から {(state?.pass ?? 0) + 1} ビット目
                            <span style={{ color: '#90a4ae' }}>
                                {' '}({(state?.pass ?? 0) + 1} / {state?.digits ?? 4} 回目)
                            </span>
                            <br />
                            <b>0 の個数</b>: {state?.zeros ?? 0}
                        </div>
                    )}
                </>
            )}
            <div style={{ marginTop: '6px', fontWeight: 'bold',
                          color: state?.finished ? '#27ae60' : '#78909c' }}>
                {statusOf(variant, state)}
            </div>
        </div>
    );

    const legend = (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px' }}>
            {variant === 'unionfind' ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="今たどっている節点" />
                    <Swatch color={NODE_STROKE[1]} label="見つけた根" />
                    <Swatch color={NODE_STROKE[3]} label="通った節点" />
                    <Swatch color={NODE_STROKE[4]} label="付け替えた節点" />
                </>
            ) : ops ? (
                <>
                    <Swatch color={NODE_STROKE[1]} label="次に出る値" />
                    <Swatch color={NODE_STROKE[4]} label="今出入りした値" />
                </>
            ) : search ? (
                <>
                    {variant === 'binary' && (
                        <Swatch color={NODE_STROKE[6]} label="探す範囲" />
                    )}
                    <Swatch color={NODE_STROKE[2]} label="今見ている値" />
                    <Swatch color={NODE_STROKE[4]} label="見つけた値" />
                </>
            ) : variant === 'selection' ? (
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
                    <Swatch color={NODE_STROKE[5]} label="今の範囲の左半分" />
                    <Swatch color={NODE_STROKE[6]} label="今の範囲の右半分" />
                    <Swatch color={NODE_STROKE[4]} label="下の段へ移した値" />
                </>
            ) : variant === 'insertion' ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="取り出して空いたマス" />
                    <Swatch color={NODE_STROKE[4]} label="動かした値" />
                </>
            ) : variant === 'bucket' ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="今見ている頻度" />
                    <Swatch color={NODE_STROKE[4]} label="今増やした頻度 / 書き出した値" />
                </>
            ) : variant === 'radix' ? (
                <>
                    <Swatch color={NODE_STROKE[1]} label="今のビット (下線)" />
                    <Swatch color={NODE_STROKE[2]} label="今見ている値" />
                    <Swatch color={NODE_STROKE[4]} label="動かした値" />
                </>
            ) : (
                <>
                    <Swatch color={NODE_STROKE[2]} label="比べている2つ" />
                    <Swatch color={NODE_STROKE[4]} label="入れ替えた2つ" />
                </>
            )}
            {!ops && <Swatch color={NODE_STROKE[3]} label={settledLabel(variant)} />}
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
