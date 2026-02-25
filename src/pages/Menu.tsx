import React from 'react';
import { useNavigate } from 'react-router-dom';

export const MenuPage: React.FC = () => {
  const navigate = useNavigate();
  return (
    <div style={{ 
      display: 'flex', flexDirection: 'column', 
      height: '100vh', width: '100vw',
      textAlign: 'center', alignItems: 'center',
      justifyContent: 'center',
      overflow: 'hidden', fontFamily: 'sans-serif' 
    }}>
      <h1 style={{ fontSize: '45px' }}>AlgoVizへようこそ</h1>
      <h3>利用可能なビジュアライザ</h3>
      <button 
        onClick={() => navigate('/graph')}
        style={{ fontSize: '20px', padding: '10px 20px', border: '1px solid' }}
      >
        グラフ
      </button>
      <button 
        onClick={() => navigate('/brainfuck')} 
        style={{ fontSize: '20px', padding: '10px 20px', border: '1px solid' }}
      >
        Brainfuck
      </button>
      <h4>他のビジュアライザはこれから追加されます</h4>
    </div>
  );
};

export default MenuPage;