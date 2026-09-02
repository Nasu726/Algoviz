import React from 'react';
import { PlaybackControls } from '../ui/PlaybackControls';
import { NODE_STROKE, EDGE_COLOR } from '../visualizers/PixiGraphApp';
import { Section, Swatch } from '../graph/panelParts';
import { usesWords, usesText } from './types';
import type { TreeVariant } from './types';
import type { GraphState } from '../../types/engine';

// AvlVisualizer の rotation と同じ並び
const ROTATION_LABEL: Record<number, string> = {
    0: '偏りを確かめています',
    1: '右に回しました',
    2: '左に回しました',
    3: '左の子を左に回してから、右に回しました',
    4: '右の子を右に回してから、左に回しました',
};

interface Props {
    variant: TreeVariant;
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

// 見ながら操作するもの。再生コントロールと進行状況。
export const TreePanel: React.FC<Props> = ({
    variant, state, isPlaying, delay, setDelay,
    onReset, onPlayPause, onStepBack, onStepNext, onRunToEnd,
    horizontal, compact,
}) => {
    const fontSize = compact ? '12px' : '13px';
    const dim = { color: '#90a4ae' };

    const trie = usesWords(variant);
    const huffman = usesText(variant);
    const avl = variant === 'avl';
    const btree = variant === 'btree';
    // trie は値の列ではなく単語の列を入れる
    const items: (number | string)[] = huffman
        ? (state?.counts ?? []).map((c) => `${c.ch}:${c.count}`)
        : trie ? (state?.words ?? []) : (state?.values ?? []);
    const pending = state?.pending ?? 0;
    const cursor = state?.cursor ?? -1;
    const inserted = state?.insertedCount ?? 0;

    const heap = variant === 'heap';
    const statusText = huffman
        ? (state?.finished ? '1本の木にまとまりました'
           : (state?.selectedA ?? -1) >= 0 ? 'この2つを繋ぎます'
           : '重みが最小の2つを選びます')
        : state?.finished ? 'すべて挿入し終えました'
        : avl && cursor < 0 && (state?.checking ?? -1) >= 0
            ? (ROTATION_LABEL[state?.rotation ?? 0] ?? '偏りを確かめています')
        : btree && (state?.splitting ?? false) ? 'あふれたので割り、真ん中の値を上へ押し上げました'
        : cursor >= 0 ? (heap ? '親と比べながら上げています'
                        : trie ? '1文字ずつ降りています'
                        : btree ? '値と比べて降りる子を決めています'
                        : '比べながら降りています')
        : (heap ? '次の値を末尾に置きます'
           : trie ? '次の単語を根から入れます'
           : '次の値を根から入れます');

    // 挿入済みと、これから挿入する値を色で分ける
    const queue = (
        <div style={{ fontFamily: 'monospace', fontSize: compact ? '14px' : '16px',
                      wordBreak: 'break-all', lineHeight: 1.7 }}>
            {items.length === 0
                ? <span style={dim}>{trie ? '（単語がありません）' : '（値がありません）'}</span>
                : items.map((v, i) => (
                <span key={i} style={{
                    marginRight: '8px',
                    color: huffman ? '#000' : i < pending ? '#90a4ae' : i === pending ? '#e74c3c' : '#000',
                    fontWeight: !huffman && i === pending ? 'bold' : 'normal',
                }}>
                    {v}
                </span>
            ))}
        </div>
    );

    const progress = (
        <div style={{ fontSize, lineHeight: 1.7, minWidth: 0 }}>
            {queue}
            {huffman ? (
                <div>
                    <b>まだ繋がっていない木</b>: {state?.rootCount ?? 0}
                </div>
            ) : (
                <div>
                    <b>{trie ? '今挿入している単語' : '今挿入している値'}</b>:{' '}
                    {pending < items.length ? items[pending] : <span style={dim}>なし</span>}
                </div>
            )}
            {trie && (
                <div>
                    <b>ここまで読んだ接頭辞</b>:{' '}
                    {state?.prefix ? <code>{state.prefix}</code> : <span style={dim}>なし</span>}
                </div>
            )}
            {!huffman && (
                <div>
                    <b>{heap ? '上げている位置' : trie || btree ? '今いる節点' : '比べている節点'}</b>:{' '}
                    {cursor >= 0 ? '光っている節点' : <span style={dim}>なし</span>}
                </div>
            )}
            {(avl || btree) && (
                <div>
                    <b>木の高さ</b>: {state?.treeHeight ?? 0}
                </div>
            )}
            {btree && (
                <div>
                    <b>1つの節点に入る値</b>: {(state?.order ?? 4) - 1} 個まで
                </div>
            )}
            <div>
                <b>節点の数</b>: {inserted}
                {!trie && !huffman && <span style={dim}> / {items.length}</span>}
            </div>
            {!heap && state?.duplicate && (
                <div style={{ color: '#e67e22' }}>同じ値が既にあったので入れませんでした</div>
            )}
            <div style={{ marginTop: '6px', fontWeight: 'bold',
                          color: state?.finished ? '#27ae60' : '#78909c' }}>
                {statusText}
            </div>
        </div>
    );

    const legend = (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px' }}>
            {btree ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="今いる節点" />
                    <Swatch color={NODE_STROKE[1]} label="上へ動く値" />
                    <Swatch color={NODE_STROKE[3]} label="通った節点" />
                    <Swatch color={NODE_STROKE[4]} label="今入れた / 押し上げた節点" />
                    <Swatch color={EDGE_COLOR[2]} label="今つないだ枝" isEdge />
                    <Swatch color={EDGE_COLOR[3]} label="通った枝" isEdge />
                </>
            ) : avl ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="比べている節点" />
                    <Swatch color={NODE_STROKE[1]} label="偏りを見ている節点" />
                    <Swatch color={NODE_STROKE[3]} label="通った節点" />
                    <Swatch color={NODE_STROKE[4]} label="今つないだ / 回した節点" />
                    <Swatch color={EDGE_COLOR[2]} label="回した枝" isEdge />
                    <Swatch color={EDGE_COLOR[3]} label="通った枝" isEdge />
                </>
            ) : huffman ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="繋ぐ2つのうち左" />
                    <Swatch color={NODE_STROKE[1]} label="繋ぐ2つのうち右" />
                    <Swatch color={NODE_STROKE[4]} label="今作った節点" />
                    <Swatch color={EDGE_COLOR[2]} label="今繋いだ枝" isEdge />
                </>
            ) : trie ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="今いる節点" />
                    <Swatch color={NODE_STROKE[3]} label="通った節点" />
                    <Swatch color={NODE_STROKE[4]} label="今作った節点" />
                    <Swatch color={EDGE_COLOR[2]} label="直前に降りた枝" isEdge />
                    <Swatch color={EDGE_COLOR[3]} label="通った枝" isEdge />
                </>
            ) : heap ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="上げている位置" />
                    <Swatch color={NODE_STROKE[1]} label="比べている親" />
                    <Swatch color={NODE_STROKE[4]} label="直前に入れ替えた位置" />
                    <Swatch color={EDGE_COLOR[2]} label="今比べている枝" isEdge />
                </>
            ) : (
                <>
                    <Swatch color={NODE_STROKE[2]} label="比べている節点" />
                    <Swatch color={NODE_STROKE[3]} label="通った節点" />
                    <Swatch color={NODE_STROKE[4]} label="今つないだ節点" />
                    <Swatch color={EDGE_COLOR[2]} label="直前に降りた枝" isEdge />
                    <Swatch color={EDGE_COLOR[3]} label="通った枝" isEdge />
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
