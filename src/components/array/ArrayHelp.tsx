import React from 'react';
import type { ArrayVariant } from './types';

const codeBlock: React.CSSProperties = {
    background: '#eceff1', padding: '8px', borderRadius: '4px', margin: '4px 0',
};

// どのソートでも同じ部分。画面の見方の前半と、値の入れ方から下。
const Common: React.FC<{ maxValues: number; sample: string; heading: number }> =
    ({ maxValues, sample, heading }) => (
    <>
        <h3>{heading}. 値の入れ方</h3>
        <ul>
            <li><b>この値で並べ直す</b>：空白か改行で区切って値を並べる</li>
            <li><b>ランダム生成</b>：個数を決めると、重複しない値を選んで並べる</li>
        </ul>
        <pre style={codeBlock}>{sample}</pre>
        <ul>
            <li>値の個数の上限は {maxValues}</li>
        </ul>

        <h3>{heading + 1}. 画面の操作</h3>
        <ul>
            <li><b>ドラッグ</b>：表示位置を動かす</li>
            <li><b>ホイール / 2本指のピンチ</b>：拡大・縮小</li>
        </ul>

        <h3>{heading + 2}. ショートカットキー</h3>
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

// 箱と値の関係はどのソートでも同じ
const Screen: React.FC<{ children: React.ReactNode }> = ({ children }) => (
    <>
        <h3>1. 画面の見方</h3>
        <ul>
            <li>箱が左から順に並んだ配列。箱の中がその位置の値</li>
            <li>箱は動かない。<b>動くのは値だけ</b>で、入れ替えると値が横に移る</li>
            {children}
            <li>色の意味は実行状態のパネルにある凡例のとおり</li>
        </ul>
    </>
);

const BubbleHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <Screen><li>灰色の箱はもう動かない</li></Screen>

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

        <Common maxValues={maxValues} sample="5 2 9 1 7 3 8 4" heading={4} />
    </>
);

const SelectionHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <Screen>
            <li>橙色の箱が、今のところ見つかっている最小の値</li>
            <li>灰色の箱はもう動かない</li>
        </Screen>

        <h3>2. 1ステップの単位</h3>
        <p>
            <b>比べるのと入れ替えるのが別のステップ</b>です。未確定の範囲を1つずつ
            見て最小を探し、探し終えてから先頭と入れ替えます。
        </p>
        <ul>
            <li><b>探すとき</b>：1つ見て、今のところの最小を更新するか決める</li>
            <li><b>入れ替えるとき</b>：見つけた最小の値を、未確定の範囲の先頭へ移す</li>
        </ul>

        <h3>3. 入れ替えは1周に1回だけ</h3>
        <p>
            バブルソートは比べるたびに入れ替えが起きますが、選択ソートは何回比べても
            入れ替えは1周に1回です。バブルソートと違って比べるのと入れ替えるのを
            別のステップにしてあるのは、この違いを見えるようにするためです。
        </p>

        <Common maxValues={maxValues} sample="5 2 9 1 7 3 8 4" heading={4} />
    </>
);

const InsertionHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <Screen>
            <li>灰色の箱は<b>並んでいるだけ</b>で、位置が確定したわけではない
                (後から来た値が割り込む)</li>
        </Screen>

        <h3>2. 1ステップの単位</h3>
        <p>
            <b>左隣と1回比べる</b>のが1ステップです。左隣の方が大きければ入れ替えて
            さらに左へ送り、そうでなければそこがその値の入る場所です。
        </p>

        <h3>3. 左から並んでいく</h3>
        <ul>
            <li>灰色の範囲は、その中では並んでいる</li>
            <li>次の値は、その範囲の中の入るべき位置まで左へ送られる</li>
            <li>既に並んでいる入力なら、1つにつき1回比べるだけで済む</li>
        </ul>

        <Common maxValues={maxValues} sample="5 2 9 1 7 3 8 4" heading={4} />
    </>
);

const ShakerHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <Screen><li>灰色の箱はもう動かない。<b>両端から増えていく</b></li></Screen>

        <h3>2. 1ステップの単位</h3>
        <p>
            <b>隣どうしを1回比べる</b>のが1ステップで、ここはバブルソートと同じです。
            違うのは<b>走査の向きが1周ごとに入れ替わる</b>ところです。
        </p>
        <ul>
            <li><b>右へ走るとき</b>：最大の値が右端へ運ばれ、右端が確定する</li>
            <li><b>左へ走るとき</b>：最小の値が左端へ運ばれ、左端が確定する</li>
        </ul>

        <h3>3. バブルソートとの違いが出る並び</h3>
        <p>
            小さい値が右端の近くにあると、バブルソートは1周で1つしか左へ動かせません。
            シェーカーソートは左向きの走査で一気に左端まで運べます。
        </p>
        <pre style={codeBlock}>2 3 4 5 6 7 8 1</pre>

        <Common maxValues={maxValues} sample="2 3 4 5 6 7 8 1" heading={4} />
    </>
);

export const ArrayHelp: React.FC<{ variant: ArrayVariant; maxValues: number }> =
    ({ variant, maxValues }) =>
        variant === 'selection' ? <SelectionHelp maxValues={maxValues} />
        : variant === 'insertion' ? <InsertionHelp maxValues={maxValues} />
        : variant === 'shaker' ? <ShakerHelp maxValues={maxValues} />
        : <BubbleHelp maxValues={maxValues} />;
