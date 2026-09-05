import React from 'react';
import { useNavigate } from 'react-router-dom';

// ビジュアライザ一覧。ジャンルごとに枠で分ける。
// 1ページ1アルゴリズムなので、ここに1行足すのが追加の作業になる。
const GENRES: { name: string; items: { path: string; label: string }[] }[] = [
    {
        name: 'グラフ探索',
        items: [
            { path: '/graph/bfs', label: '幅優先探索 (BFS)' },
            { path: '/graph/dfs', label: '深さ優先探索 (DFS)' },
            { path: '/graph/dijkstra', label: 'ダイクストラ法' },
        ],
    },
    {
        name: '木',
        items: [
            { path: '/tree/bst', label: '二分探索木の構築' },
            { path: '/tree/avl', label: 'AVL 木の構築' },
            { path: '/tree/btree', label: 'B木の構築' },
            { path: '/tree/heap', label: 'ヒープの構築' },
            { path: '/tree/trie', label: 'trie (接頭辞木) の構築' },
            { path: '/tree/huffman', label: 'ハフマン木の構築' },
        ],
    },
    {
        name: '配列',
        items: [
            { path: '/array/bubble', label: 'バブルソート' },
            { path: '/array/selection', label: '選択ソート' },
            { path: '/array/insertion', label: '挿入ソート' },
            { path: '/array/shaker', label: 'シェーカーソート' },
            { path: '/array/quick', label: 'クイックソート' },
            { path: '/array/merge', label: 'マージソート' },
        ],
    },
    {
        name: 'オートマトン',
        items: [
            { path: '/automaton', label: '決定性有限オートマトン (DFA)' },
        ],
    },
    {
        name: 'その他',
        items: [
            { path: '/brainfuck', label: 'Brainfuck' },
        ],
    },
];

export const MenuPage: React.FC = () => {
    const navigate = useNavigate();

    return (
        <div style={{
            display: 'flex', flexDirection: 'column', alignItems: 'center',
            minHeight: '100vh', width: '100vw', boxSizing: 'border-box',
            padding: '40px 20px', textAlign: 'center',
            fontFamily: 'sans-serif', backgroundColor: '#ffffff', color: '#000000',
        }}>
            <h1 style={{ fontSize: '42px', margin: '0 0 28px' }}>AlgoVizへようこそ</h1>

            <div style={{
                display: 'flex', flexWrap: 'wrap', justifyContent: 'center',
                alignItems: 'flex-start', gap: '20px', width: '100%', maxWidth: '1000px',
            }}>
                {GENRES.map((genre) => (
                    <fieldset key={genre.name} style={{
                        border: '1px solid #b0bec5', borderRadius: '8px',
                        padding: '12px 16px 16px', margin: 0,
                        minWidth: '260px', flex: '0 1 300px', textAlign: 'left',
                    }}>
                        <legend style={{ padding: '0 8px', fontWeight: 'bold', color: '#37474f' }}>
                            {genre.name}
                        </legend>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                            {genre.items.map((item) => (
                                <button
                                    key={item.path}
                                    onClick={() => navigate(item.path)}
                                    style={{
                                        padding: '10px 14px', cursor: 'pointer',
                                        border: '1px solid #cfd8dc', borderRadius: '6px',
                                        background: '#fff', textAlign: 'left', width: '100%',
                                        fontSize: '17px', fontWeight: 'bold',
                                    }}
                                >
                                    {item.label}
                                </button>
                            ))}
                        </div>
                    </fieldset>
                ))}
            </div>

            <h4 style={{ color: '#78909c', fontWeight: 'normal' }}>
                他のビジュアライザはこれから追加されます
            </h4>
            <a href="https://github.com/Nasu726/AlgoViz" target="_blank" rel="noreferrer">
                <img src="/github-mark.png" alt="GitHubのリポジトリページ" width="30" height="30" />
            </a>
        </div>
    );
};

export default MenuPage;
