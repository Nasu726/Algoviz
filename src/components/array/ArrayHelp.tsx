import React from 'react';
import type { ArrayVariant } from './types';

const codeBlock: React.CSSProperties = {
    background: '#eceff1', padding: '8px', borderRadius: '4px', margin: '4px 0',
};

// 画面の操作とショートカット。どのページも同じで、Ctrl + S の言葉だけ変わる
const Controls: React.FC<{ heading: number; applyLabel: string }> =
    ({ heading, applyLabel }) => (
    <>
        <h3>{heading}. 画面の操作</h3>
        <ul>
            <li><b>ドラッグ</b>：表示位置を動かす</li>
            <li><b>ホイール / 2本指のピンチ</b>：拡大・縮小</li>
        </ul>

        <h3>{heading + 1}. ショートカットキー</h3>
        <ul>
            <li><b>Esc</b>：ビジュアライザ一覧へ戻る</li>
            <li><b>Ctrl + H</b>：ヘルプを開く</li>
            <li><b>Ctrl + S</b>：{applyLabel}</li>
            <li><b>Ctrl + Enter</b>：実行 / 一時停止</li>
            <li><b>Ctrl + &larr;</b> / <b>&rarr;</b>：戻る / 進む</li>
            <li><b>Ctrl + &uarr;</b> / <b>&darr;</b>：実行速度の増減</li>
        </ul>
    </>
);

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

        <Controls heading={heading + 1} applyLabel="この値で並べ直す" />
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
            <li>運ばれた位置はもう動かない (灰色になる)</li>
        </ul>

        <h3>4. 速さの工夫はしていない</h3>
        <ul>
            <li><b>途中で並び終わっても打ち切らない</b></li>
            <li><b>確定した範囲も走査から外さず、毎回、左端から右端まで見る</b></li>
        </ul>
        <p>
            どちらも速くはなりますが、隣どうしを何度も比べて少しずつ運ぶという仕組み
            そのものが見えにくくなります。手数は並びによらず一定です。
        </p>

        <Common maxValues={maxValues} sample="5 2 9 1 7 3 8 4" heading={5} />
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

        <h3>4. どの位置も同じ手順で決める</h3>
        <ul>
            <li>見つけた最小の値が既に先頭にあっても、入れ替えのステップは踏む</li>
            <li>残りが1つになった最後の周も飛ばさない (探す先が無いので、
                入れ替えのステップだけになる)</li>
        </ul>
        <p>
            なお、確定した範囲を探しに行かないのは速さの工夫ではありません。そこを
            見ると、既に置いた小さい値を拾い直してしまいます。
        </p>

        <Common maxValues={maxValues} sample="5 2 9 1 7 3 8 4" heading={5} />
    </>
);

const InsertionHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <Screen>
            <li>配列の上に浮いている箱が、<b>取り出して手に持っている値</b>。
                その値がいた場所は空になる</li>
            <li>灰色の箱は<b>並んでいるだけ</b>で、位置が確定したわけではない
                (後から来た値が割り込む)</li>
        </Screen>

        <h3>2. 1ステップの単位</h3>
        <p>1つの値を入れるのに、3種類の手を踏みます。</p>
        <ul>
            <li><b>取り出す</b>：次の値を持ち上げる。そのマスが空になる</li>
            <li><b>ずらす</b>：空きの左隣の方が大きければ、それを右へずらして
                空きを左へ移す</li>
            <li><b>差し込む</b>：左隣の方が大きくなければ、そこが入る場所。
                空いたマスに置く</li>
        </ul>
        <p>
            隣どうしを繰り返し入れ替える書き方でも結果は同じですが、それだと
            バブルソートと同じ動きに見えてしまいます。<b>取り出して差し込む</b>のが
            このソートの形です。
        </p>

        <h3>3. 左から並んでいく</h3>
        <ul>
            <li>灰色の範囲は、その中では並んでいる</li>
            <li>取り出した値は、その範囲の中の入るべき場所まで運ばれる</li>
            <li>既に並んでいる入力なら、1つにつき取り出して差し込むだけで済む</li>
        </ul>

        <h3>4. どの位置も同じ手順で決める</h3>
        <p>
            先頭の1つも「並んでいる」と決め打ちにせず、同じ手順を踏みます。取り出して、
            左に何も無いのでその場に差し込みます。
        </p>

        <Common maxValues={maxValues} sample="5 2 9 1 7 3 8 4" heading={5} />
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
        <p>
            バブルソートと同じく<b>速さの工夫はしていません</b>。途中で並び終わっても
            打ち切らず、確定した範囲も走査から外しません。手数は同じになるので、
            違いは手数ではなく運ばれ方に出ます。
        </p>

        <Common maxValues={maxValues} sample="2 3 4 5 6 7 8 1" heading={5} />
    </>
);

const QuickHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <Screen>
            <li>紫色の箱が<b>今並べている範囲</b>。橙色がその範囲の<b>基準の値</b></li>
            <li>青色は、基準より小さいと分かって左に集めた部分</li>
            <li>灰色の箱はもう動かない</li>
        </Screen>

        <h3>2. 1ステップの単位</h3>
        <p>4種類の手を踏みます。</p>
        <ul>
            <li><b>範囲を取り出す</b>：次に並べる範囲を1つ取り出し、右端を基準に決める</li>
            <li><b>比べる</b>：1つ見て、基準より小さければ左の並びへ入れ替える</li>
            <li><b>基準を置く</b>：見終わったら基準を境目へ動かす。その位置が確定する</li>
            <li><b>何もしない</b>：取り出した範囲が空か1つだけだったとき</li>
        </ul>

        <h3>3. 分けて、それぞれをまた並べる</h3>
        <ul>
            <li>基準を置いた位置は、左が全部小さく右が全部大きいので、そこで確定する</li>
            <li>その左右がそれぞれ次に並べる範囲になる。積んでおいて順に取り出す</li>
            <li><b>空の範囲も1つだけの範囲も積んで、取り出す手を踏む。</b>
                飛ばすと、分け方が端に寄ったときに何が起きたのか見えない</li>
        </ul>

        <h3>4. 分け方が偏る並び</h3>
        <p>
            基準に選ぶのは範囲の右端です。既に並んでいる入力だと基準が毎回いちばん
            大きい値になり、左右に分かれず片側だけが残ります。範囲が1つずつしか
            減らないので、手数がいちばん多くなります。
        </p>
        <pre style={codeBlock}>1 2 3 4 5 6 7 8</pre>

        <Common maxValues={maxValues} sample="5 2 9 1 7 3 8 4" heading={5} />
    </>
);

const MergeHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <Screen>
            <li><b>下の段が作業用の場所</b>。併合した結果をいったんここへ書く</li>
            <li>青色が今の範囲の<b>左半分</b>、紫色が<b>右半分</b></li>
            <li>値を下の段へ移すと、元のマスは空になる</li>
            <li>灰色は並んでいる範囲</li>
        </Screen>

        <h3>2. 分けて、戻りながら併合する</h3>
        <ul>
            <li>範囲を半分ずつに分けていき、<b>1つ以下になったらそれは並んでいる</b></li>
            <li>戻りながら、並んでいる2つを突き合わせて1つの並びにする</li>
            <li>左を全部片付けてから右へ行き、両方が並んでから併合する</li>
        </ul>

        <h3>3. 1ステップの単位</h3>
        <ul>
            <li><b>分ける</b>：範囲を半分にして、左・右・併合の順に片付ける仕事を積む</li>
            <li><b>1つ以下</b>：分けきったので、この範囲は並んでいる</li>
            <li><b>併合を始める</b>：分けた2つが並び終えたので、突き合わせに入る</li>
            <li><b>小さい方を移す</b>：2つの先頭を比べ、小さい方を下の段へ移す</li>
            <li><b>書き戻す</b>：併合が終わったら、下の段の並びを上の段へまとめて戻す</li>
        </ul>

        <h3>4. 別の場所が要る</h3>
        <p>
            ほかのソートは配列の中だけで入れ替えますが、マージソートは<b>併合の結果を
            置く別の場所</b>が要ります。その場で入れ替えるように書くと、この特徴が
            見えなくなるので、下の段を使う形にしてあります。
        </p>

        <Common maxValues={maxValues} sample="5 2 9 1 7 3 8 4" heading={5} />
    </>
);

// 探すページ用。値の入れ方の言葉が並べ替えとは違う
const SearchCommon: React.FC<{ maxValues: number; sample: string; heading: number;
                               sortedNote?: boolean }> =
    ({ maxValues, sample, heading, sortedNote }) => (
    <>
        <h3>{heading}. 値の入れ方</h3>
        <ul>
            <li><b>探す値</b>：入力して Enter か「この値を探す」。値の並びはそのまま</li>
            <li><b>この値で作り直す</b>：空白か改行で区切って値を並べる</li>
            <li><b>ランダム生成</b>：個数を決めると、重複しない値を選んで並べる
                {sortedNote ? ' (昇順で作ります)' : ''}</li>
        </ul>
        <pre style={codeBlock}>{sample}</pre>
        <ul>
            <li>値の個数の上限は {maxValues}</li>
        </ul>

        <Controls heading={heading + 1} applyLabel="この値で作り直す" />
    </>
);

// スタック / キュー / デック。操作の並びを流す
const OpsHelp: React.FC<{ maxOps: number; kind: 'stack' | 'queue' | 'deque' }> =
    ({ maxOps, kind }) => (
    <>
        <h3>1. 画面の見方</h3>
        <ul>
            <li>入れ物は<b>今入っている数だけ</b>のマスが横に並んだもの</li>
            <li><b>出入りはマスごと</b>。入れるとマスが1つ外から入り、
                取り出すとマスが1つ外へ出る</li>
            <li>橙色が<b>次に出る値</b></li>
            <li>緑色が今出入りしたマス</li>
        </ul>

        <h3>2. 1ステップの単位</h3>
        <p>
            <b>操作を1つ実行する</b>のが1ステップです。左のパネルに書いた並びを
            頭から流します。取り出したマスは<b>次の手で消えます</b>。
        </p>

        <h3>3. 出し入れする端</h3>
        {kind === 'stack' && (
            <ul>
                <li><b>入れるのも出すのも右端</b> (最後に入れたものが最初に出る)</li>
                <li>この出方を LIFO という</li>
                <li>左に埋まっている値は、右が空くまで出てこない</li>
            </ul>
        )}
        {kind === 'queue' && (
            <ul>
                <li><b>入れるのは右端、出すのは左端</b> (最初に入れたものが最初に出る)</li>
                <li>この出方を FIFO という</li>
                <li>左端から出すので、残りが左へ詰まる</li>
            </ul>
        )}
        {kind === 'deque' && (
            <ul>
                <li><b>左右どちらの端からも入れられるし、出せる</b></li>
                <li><code>pushL</code> / <code>pushR</code> / <code>popL</code> /
                    <code> popR</code> で端を書き分ける</li>
                <li>右だけ使えばスタック、右で入れて左で出せばキューになる</li>
            </ul>
        )}
        <p>
            <b>同じ並びを別のページに入れると、出てくる値の順が変わります。</b>
            そこが見どころです。
        </p>

        <h3>4. 操作の書き方</h3>
        <ul>
            {kind === 'deque' ? (
                <li><code>pushL</code> か <code>pushR</code> に続けて値、
                    <code>popL</code> か <code>popR</code></li>
            ) : (
                <li><code>push</code> に続けて値、<code>pop</code></li>
            )}
            <li><b>ランダム生成</b>：空なのに取り出す操作が出ないように作ります</li>
            <li>空なのに取り出そうとしたときは、何も起きません</li>
        </ul>
        <pre style={codeBlock}>
            {kind === 'deque' ? 'pushR 5 pushL 2 pushR 9 popL popR popL'
                              : 'push 5 push 2 pop push 9 pop pop'}
        </pre>
        <ul>
            <li>操作の数の上限は {maxOps}</li>
        </ul>

        <Controls heading={5} applyLabel="この並びで動かす" />
    </>
);

const LinearHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <h3>1. 画面の見方</h3>
        <ul>
            <li>箱が左から順に並んだ配列。箱の中がその位置の値</li>
            <li>赤色が今見ている値。緑色が見つけた値</li>
            <li>灰色は<b>もう見ない範囲</b>。違うと分かったところ</li>
        </ul>

        <h3>2. 1ステップの単位</h3>
        <p>
            <b>1つ見る</b>のが1ステップです。左から順に見ていき、探す値と同じなら
            そこで止まります。灰色が左から伸びていく長さが、そのまま何回見たかです。
        </p>

        <h3>3. 終わり方は2通り</h3>
        <ul>
            <li>探す値と同じものが見つかる</li>
            <li>右端まで見ても無い。<b>その配列には無い</b>と分かる</li>
        </ul>

        <h3>4. 並んでいる必要は無い</h3>
        <p>
            どんな並びでも同じ手順で探せます。そのぶん、無い値を探すと端まで
            見ることになります。二分探索との違いはここです。
        </p>

        <SearchCommon maxValues={maxValues} sample="5 2 9 1 7 3 8 4" heading={5} />
    </>
);

const BinaryHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <h3>1. 画面の見方</h3>
        <ul>
            <li>紫色が<b>探す範囲</b>。赤色がその真ん中で、今見ている値</li>
            <li>緑色が見つけた値</li>
            <li>灰色は<b>もう見ない範囲</b>。真ん中と比べて捨てた側</li>
        </ul>

        <h3>2. 1ステップの単位</h3>
        <p>
            <b>真ん中を見て、範囲を半分にする</b>のが1ステップです。真ん中が探す値と
            同じなら見つかり、違えば<b>半分を丸ごと捨てられます</b>。
        </p>
        <ul>
            <li>真ん中の方が小さい → 探す値は右にあるので、左半分を捨てる</li>
            <li>真ん中の方が大きい → 探す値は左にあるので、右半分を捨てる</li>
        </ul>

        <h3>3. 並んでいることが前提</h3>
        <p>
            半分を捨てられるのは、並んでいるから「こちら側には無い」と言い切れるためです。
            <b>並んでいない入力でも動かせるようにしてあります</b>が、そのときは
            在るのに見つからないことが起きます。並んでいなければパネルに出ます。
        </p>
        <pre style={codeBlock}>1 2 3 4 5 7 8 9</pre>

        <h3>4. 線形探索との違い</h3>
        <p>
            線形探索は1回見ると候補が1つ減りますが、二分探索は1回で半分に減ります。
            「まだ見ていない場所」の数がどう減るかを見比べてください。
        </p>

        <SearchCommon maxValues={maxValues} sample="1 2 3 4 5 7 8 9" heading={5}
                      sortedNote />
    </>
);

export const ArrayHelp: React.FC<{ variant: ArrayVariant; maxValues: number }> =
    ({ variant, maxValues }) =>
        variant === 'stack' || variant === 'queue' || variant === 'deque'
            ? <OpsHelp maxOps={maxValues} kind={variant} />
        : variant === 'linear' ? <LinearHelp maxValues={maxValues} />
        : variant === 'binary' ? <BinaryHelp maxValues={maxValues} />
        : variant === 'merge' ? <MergeHelp maxValues={maxValues} />
        : variant === 'quick' ? <QuickHelp maxValues={maxValues} />
        : variant === 'selection' ? <SelectionHelp maxValues={maxValues} />
        : variant === 'insertion' ? <InsertionHelp maxValues={maxValues} />
        : variant === 'shaker' ? <ShakerHelp maxValues={maxValues} />
        : <BubbleHelp maxValues={maxValues} />;
