import React from 'react';
import type { ArrayVariant } from './types';

const codeBlock: React.CSSProperties = {
    background: '#eceff1', padding: '8px', borderRadius: '4px', margin: '4px 0',
};

const BubbleHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <h3>1. 画面の見方</h3>
        <ul>
            <li>箱が左から順に並んだ配列。箱の中がその位置の値</li>
            <li>箱は動かない。<b>動くのは値だけ</b>で、入れ替えると値が横に移る</li>
            <li>色の意味は実行状態のパネルにある凡例のとおり</li>
        </ul>

        <h3>2. 1ステップの単位</h3>
        <p>
            <b>隣どうしを1回比べる</b>のが1ステップです。大小が逆なら、その手で
            入れ替えます。左端から右へ順に比べていき、右端まで行くと1回の走査が
            終わります。
        </p>

        <h3>3. 右から確定していく</h3>
        <ul>
            <li>1回の走査で、その範囲の最大の値が右端まで運ばれる</li>
            <li>運ばれた位置はもう動かないので、走査する範囲が1つずつ狭くなる</li>
            <li>走査中に一度も入れ替えが起きなければ、そこで全体が並んでいる</li>
        </ul>

        <h3>4. 値の入れ方</h3>
        <ul>
            <li><b>この値で並べ直す</b>：空白か改行で区切って値を並べる</li>
            <li><b>ランダム生成</b>：個数を決めると、重複しない値を選んで並べる</li>
        </ul>
        <pre style={codeBlock}>5 2 9 1 7 3 8 4</pre>
        <ul>
            <li>値の個数の上限は {maxValues}</li>
        </ul>

        <h3>5. 画面の操作</h3>
        <ul>
            <li><b>ドラッグ</b>：表示位置を動かす</li>
            <li><b>ホイール / 2本指のピンチ</b>：拡大・縮小</li>
        </ul>

        <h3>6. ショートカットキー</h3>
        <ul>
            <li><b>Esc</b>：ビジュアライザ一覧へ戻る</li>
            <li><b>Ctrl + H</b>：ヘルプを開く</li>
            <li><b>Ctrl + S</b>：この値で並べ直す</li>
            <li><b>Ctrl + Enter</b>：実行 / 一時停止</li>
            <li><b>Ctrl + &larr;</b> / <b>&rarr;</b>：戻る / 進む</li>
            <li><b>Ctrl + &uarr;</b> / <b>&darr;</b>：実行速度の増減</li>
        </ul>
    </>
);

export const ArrayHelp: React.FC<{ variant: ArrayVariant; maxValues: number }> =
    ({ maxValues }) => <BubbleHelp maxValues={maxValues} />;
