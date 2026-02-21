import React from 'react';

interface MenuProps {
  onSelect: (visualizerId: string) => void;
}

export const Menu: React.FC<MenuProps> = ({ onSelect }) => {
  return (
    <div style={{ padding: '40px', textAlign: 'center', fontFamily: 'sans-serif' }}>
      <h1>アルゴリズム ビジュアライザ</h1>
      <p>学習・実行したいアルゴリズムを選択してください。</p>

      <div style={{ display: 'flex', gap: '20px', justifyContent: 'center', marginTop: '40px' }}>
        {/* Brainfuck ボタン */}
        <div 
          onClick={() => onSelect('brainfuck')}
          style={{
            border: '2px solid #ccc', borderRadius: '8px', padding: '20px', 
            width: '200px', cursor: 'pointer', backgroundColor: '#f9f9f9',
            transition: 'background-color 0.2s'
          }}
          onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#e0f7fa'}
          onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#f9f9f9'}
        >
          <h2 style={{ margin: '0 0 10px 0' }}>Brainfuck</h2>
          <p style={{ fontSize: '14px', color: '#666', margin: 0 }}>
            チューリング完全な難解プログラミング言語の実行過程を可視化します。
          </p>
        </div>

        {/* グラフアルゴリズム ボタン (将来用) */}
        <div 
          onClick={() => alert("グラフアルゴリズムは現在開発中です！")}
          style={{
            border: '2px solid #ccc', borderRadius: '8px', padding: '20px', 
            width: '200px', cursor: 'pointer', backgroundColor: '#f9f9f9',
            opacity: 0.6
          }}
        >
          <h2 style={{ margin: '0 0 10px 0' }}>グラフ探索</h2>
          <p style={{ fontSize: '14px', color: '#666', margin: 0 }}>
            ダイクストラ法やA*アルゴリズムの経路探索を可視化します。(Coming Soon)
          </p>
        </div>
      </div>
    </div>
  );
};

export default Menu;