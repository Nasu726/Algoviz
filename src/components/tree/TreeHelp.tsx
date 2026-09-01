import React from 'react';

const codeBlock: React.CSSProperties = {
    background: '#eceff1', padding: '8px', borderRadius: '4px', margin: '4px 0',
};

export const TreeHelp: React.FC<{ maxValues: number }> = ({ maxValues }) => (
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
