import React from 'react';
import type { TreeVariant } from './types';

const codeBlock: React.CSSProperties = {
    background: '#eceff1', padding: '8px', borderRadius: '4px', margin: '4px 0',
};

const BstHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <h3>1. 画面の見方</h3>
        <ul>
            <li>丸の中の数字がその節点の値。小さい値が左、大きい値が右に来る</li>
            <li>左のパネルに並ぶ値のうち、赤いものが今挿入している値</li>
            <li>色の意味は実行状態のパネルにある凡例のとおり</li>
        </ul>

        <h3>2. 1ステップの単位</h3>
        <p>
            <b>1回比べて1つ降りる</b>のが1ステップです。値を1つ挿入するには、
            根から空いている場所まで降りるぶんだけステップがかかります。
        </p>
        <ul>
            <li>降りた先が空いていれば、そこに新しい節点をつないで次の値へ移る</li>
            <li>同じ値が既にあるときは挿入しない</li>
        </ul>

        <h3>3. 値の入れ方</h3>
        <ul>
            <li><b>この値で作り直す</b>：空白か改行で区切って値を並べる。左から順に挿入する</li>
            <li><b>ランダム生成</b>：個数を決めると、重複しない値を選んで並べる</li>
        </ul>
        <pre style={codeBlock}>50 30 70 20 40 60 80</pre>
        <ul>
            <li>値の個数の上限は {maxValues}</li>
            <li>並べる順で木の形が変わる。昇順に入れると片側に伸び続ける</li>
        </ul>

        <h3>4. 画面の操作</h3>
        <ul>
            <li><b>ドラッグ</b>：表示位置を動かす</li>
            <li><b>ホイール / 2本指のピンチ</b>：拡大・縮小</li>
            <li>木が育つたびに、自動で全体が画面に収まる位置に戻る</li>
        </ul>

        <h3>5. ショートカットキー</h3>
        <ul>
            <li><b>Esc</b>：ビジュアライザ一覧へ戻る</li>
            <li><b>Ctrl + H</b>：ヘルプを開く</li>
            <li><b>Ctrl + S</b>：この値で作り直す</li>
            <li><b>Ctrl + Enter</b>：実行 / 一時停止</li>
            <li><b>Ctrl + ←</b> / <b>→</b>：戻る / 進む</li>
            <li><b>Ctrl + ↑</b> / <b>↓</b>：実行速度の増減</li>
        </ul>
    </>
);

const HeapHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <h3>1. 画面の見方</h3>
        <ul>
            <li>丸の中の数字がその位置の値。形は完全二分木で決まっていて、<b>入れ替えで動くのは値だけ</b></li>
            <li>左のパネルに並ぶ値のうち、赤いものが今入れている値</li>
            <li>色の意味は実行状態のパネルにある凡例のとおり</li>
        </ul>

        <h3>2. 1ステップの単位</h3>
        <p>
            <b>親と一度比べて、必要なら入れ替える</b>のが1ステップです。
            値は末尾に置かれ、順序が崩れているあいだ上へ上がっていきます。
        </p>
        <ul>
            <li>入れ替えが要らなくなった時点で、その値の挿入は終わり</li>
            <li>上がる距離は木の高さまでなので、多くの値は1〜2手で落ち着く</li>
        </ul>

        <h3>3. 値の入れ方</h3>
        <ul>
            <li><b>この値で作り直す</b>：空白か改行で区切って値を並べる。左から順に挿入する</li>
            <li><b>ランダム生成</b>：個数を決めると、重複しない値を選んで並べる</li>
            <li><b>最大ヒープ</b>：外すと最小ヒープになり、小さい値が上に来る</li>
        </ul>
        <pre style={codeBlock}>20 40 30 80 50 70 60</pre>
        <ul>
            <li>値の個数の上限は {maxValues}</li>
        </ul>

        <h3>4. 画面の操作</h3>
        <ul>
            <li><b>ドラッグ</b>：表示位置を動かす</li>
            <li><b>ホイール / 2本指のピンチ</b>：拡大・縮小</li>
        </ul>

        <h3>5. ショートカットキー</h3>
        <ul>
            <li><b>Esc</b>：ビジュアライザ一覧へ戻る</li>
            <li><b>Ctrl + H</b>：ヘルプを開く</li>
            <li><b>Ctrl + S</b>：この値で作り直す</li>
            <li><b>Ctrl + Enter</b>：実行 / 一時停止</li>
            <li><b>Ctrl + &larr;</b> / <b>&rarr;</b>：戻る / 進む</li>
            <li><b>Ctrl + &uarr;</b> / <b>&darr;</b>：実行速度の増減</li>
        </ul>
    </>
);


const TrieHelp: React.FC<{ maxWords: number }> = ({ maxWords }) => (
    <>
        <h3>1. 画面の見方</h3>
        <ul>
            <li><b>枝に書かれた文字</b>がその一歩で読む文字。根からの道をつなげたものが接頭辞になる</li>
            <li>節点そのものには名前が無い。だから丸の中は空</li>
            <li><b>二重丸</b>がそこで終わる単語がある印</li>
            <li>枝は文字の順に左から並ぶ</li>
        </ul>

        <h3>2. 1ステップの単位</h3>
        <p><b>1文字進む</b>のが1ステップです。単語を1つ入れるのに、根から文字数ぶんのステップがかかります。</p>
        <ul>
            <li>その文字の枝が既にあればたどるだけ。無ければ新しい節点を作る</li>
            <li>共通の接頭辞を持つ単語は、枝を共有して1本にまとまる</li>
            <li>読み切ったところで二重丸が付く</li>
        </ul>

        <h3>3. 単語の入れ方</h3>
        <p>空白か改行で区切って並べます。左から順に挿入します。</p>
        <pre style={codeBlock}>to tea ten ted i in inn</pre>
        <ul>
            <li>単語の数の上限は {maxWords}</li>
            <li>同じ単語を2回入れても節点は増えない</li>
        </ul>

        <h3>4. 画面の操作</h3>
        <ul>
            <li><b>ドラッグ</b>：表示位置を動かす</li>
            <li><b>ホイール / 2本指のピンチ</b>：拡大・縮小</li>
        </ul>

        <h3>5. ショートカットキー</h3>
        <ul>
            <li><b>Esc</b>：ビジュアライザ一覧へ戻る</li>
            <li><b>Ctrl + H</b>：ヘルプを開く</li>
            <li><b>Ctrl + S</b>：この単語で作り直す</li>
            <li><b>Ctrl + Enter</b>：実行 / 一時停止</li>
            <li><b>Ctrl + &larr;</b> / <b>&rarr;</b>：戻る / 進む</li>
            <li><b>Ctrl + &uarr;</b> / <b>&darr;</b>：実行速度の増減</li>
        </ul>
    </>
);


const HuffmanHelp: React.FC<{ maxLeaves: number }> = ({ maxLeaves }) => (
    <>
        <h3>1. 画面の見方</h3>
        <ul>
            <li><b>葉の丸の中</b>がその文字。内部の節点には文字が無い</li>
            <li>丸の脇の数字が<b>重み</b>。葉なら出現回数、内部の節点なら下にある葉の合計</li>
            <li><b>枝の 0 と 1</b> がそのまま符号のビット。根から葉までをつなげたものがその文字の符号になる</li>
            <li>途中は木が複数ある状態 (森) になり、横に並んで表示される</li>
        </ul>

        <h3>2. 1ステップの単位</h3>
        <p><b>「重みが最小の2つを選ぶ」</b>と<b>「その2つを繋ぐ」</b>の2手で1回の併合です。</p>
        <ul>
            <li>選んだ2つを子にする新しい節点ができ、その重みは2つの合計になる</li>
            <li>木が1本になったら完成</li>
            <li>重みが同じときは先にできた方から選ぶので、同じ入力なら必ず同じ木になる</li>
        </ul>

        <h3>3. 入力</h3>
        <p>文章を入れると、出てくる文字を数えて葉にします。空白は数えません。</p>
        <pre style={codeBlock}>abracadabra</pre>
        <ul>
            <li>文字の種類の上限は {maxLeaves}</li>
            <li>出現回数が多い文字ほど根に近くなり、符号が短くなる</li>
        </ul>

        <h3>4. 画面の操作</h3>
        <ul>
            <li><b>ドラッグ</b>：表示位置を動かす</li>
            <li><b>ホイール / 2本指のピンチ</b>：拡大・縮小</li>
        </ul>

        <h3>5. ショートカットキー</h3>
        <ul>
            <li><b>Esc</b>：ビジュアライザ一覧へ戻る</li>
            <li><b>Ctrl + H</b>：ヘルプを開く</li>
            <li><b>Ctrl + S</b>：この文章で作り直す</li>
            <li><b>Ctrl + Enter</b>：実行 / 一時停止</li>
            <li><b>Ctrl + &larr;</b> / <b>&rarr;</b>：戻る / 進む</li>
            <li><b>Ctrl + &uarr;</b> / <b>&darr;</b>：実行速度の増減</li>
        </ul>
    </>
);


const AvlHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
    <>
        <h3>1. 画面の見方</h3>
        <ul>
            <li>丸の中がその節点の値。小さい値が左、大きい値が右に来る (二分探索木と同じ)</li>
            <li><b>丸の脇の数字が偏り</b>。左の高さから右の高さを引いたもの</li>
            <li>偏りが <b>2</b> か <b>-2</b> になった節点で回転が起きる</li>
            <li>色の意味は実行状態のパネルにある凡例のとおり</li>
        </ul>

        <h3>2. 1ステップの単位</h3>
        <p>降りるときと戻るときで単位が変わります。</p>
        <ul>
            <li><b>降りるとき</b>：1回比べて1つ降りる (二分探索木と同じ)</li>
            <li><b>戻るとき</b>：通ってきた道を根へ向かって1つずつ、偏りを見て必要なら回す</li>
        </ul>

        <h3>3. 回転の4通り</h3>
        <ul>
            <li><b>右回転</b>：左の子の左が重いとき</li>
            <li><b>左回転</b>：右の子の右が重いとき</li>
            <li><b>左右</b>：左の子の<b>右</b>が重いとき。左の子を左に回してから、右に回す</li>
            <li><b>右左</b>：右の子の<b>左</b>が重いとき。右の子を右に回してから、左に回す</li>
        </ul>

        <h3>4. 値の入れ方</h3>
        <ul>
            <li><b>この値で作り直す</b>：空白か改行で区切って値を並べる。左から順に挿入する</li>
            <li><b>ランダム生成</b>：個数を決めると、重複しない値を選んで並べる</li>
        </ul>
        <pre style={codeBlock}>10 20 30 40 50 25</pre>
        <ul>
            <li>値の個数の上限は {maxValues}</li>
            <li>昇順に入れても一直線にならない。そこが二分探索木との違い</li>
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
            <li><b>Ctrl + S</b>：この値で作り直す</li>
            <li><b>Ctrl + Enter</b>：実行 / 一時停止</li>
            <li><b>Ctrl + &larr;</b> / <b>&rarr;</b>：戻る / 進む</li>
            <li><b>Ctrl + &uarr;</b> / <b>&darr;</b>：実行速度の増減</li>
        </ul>
    </>
);

export const TreeHelp: React.FC<{ variant: TreeVariant; maxValues: number }> = ({ variant, maxValues }) =>
    variant === 'avl' ? <AvlHelp maxValues={maxValues} />
    : variant === 'huffman' ? <HuffmanHelp maxLeaves={maxValues} />
    : variant === 'trie' ? <TrieHelp maxWords={maxValues} />
    : variant === 'heap' ? <HeapHelp maxValues={maxValues} />
    : <BstHelp maxValues={maxValues} />;
