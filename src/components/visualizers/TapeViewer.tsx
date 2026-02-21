import React from 'react';
import { char } from '../../utils/functions'; // パスは環境に合わせて調整してください

// 必要なデータ（Props）の定義
interface TapeViewerProps {
  tapeData: any[];     // 表示用のセル配列
  ptr: number;         // ポインタ位置
  stepCount: bigint;   // ステップ数
  offsetPx: number;    // スライド用のアニメーション量
}

export const TapeViewer: React.FC<TapeViewerProps> = ({ tapeData, ptr, stepCount, offsetPx }) => {
  return (
    <>
      <h3 style={{ margin: '0 0 20px 0', color: '#555' }}>メモリテープ</h3>
      
      {/* セルを並べる部分 */}
      <div style={{ 
        display: 'flex', 
        justifyContent: 'center', 
        gap: '4px', 
        transform: `translateX(${-offsetPx}px)`, 
        willChange: 'transform'
      }}>
        {Array.isArray(tapeData) ? (
          tapeData.map((cell) => {
            if(!cell.exists){
              return <div key={cell.index} style={{ width: '60px', height: '75px' }} />;
            }
            const isPtr = cell.index === ptr;
            return (
              <div key={cell.index} style={{ display: 'flex', flexDirection: 'column', alignItems: 'center' }}> 
                <div style={{ 
                  width: '60px', height: '75px', 
                  border: isPtr ? '2px solid #e91e63' : '1px solid #b0bec5',
                  backgroundColor: isPtr ? '#fff' : '#eceff1',
                  color: isPtr ? '#e91e63' : '#455a64',
                  display: 'flex', flexDirection: 'column', 
                  justifyContent: 'center', alignItems: 'center',
                  fontWeight: isPtr ? 'bold' : 'normal',
                  borderRadius: '4px',
                  transform: isPtr ? 'scale(1.15)' : 'scale(1)',
                  zIndex: isPtr ? 10 : 1,
                  transition: 'all 0.1s'
                }}>
                  <span style={{ fontSize: '16px' }}>{cell.value}</span>
                  <span style={{ fontSize: '16px' }}>{char(cell.value)}</span>
                  <span style={{ fontSize: '10px', color: '#90a4ae' }}>{cell.index}</span>
                </div>
                <div style={{ height: '20px', marginTop: '6px', fontSize: '12px', fontWeight: 'bold', color: '#e91e63' }}>
                  {cell.name}
                </div>
              </div>
            );
          })
        ) : (
          <div style={{color: 'red'}}>Data Mismatch</div>
        )}
      </div>

      <p style={{ marginTop: '30px', color: '#78909c' }}>
        ポインタ位置: {ptr ?? "?"} | 実行ステップ数: {stepCount ?? 0}
      </p>
    </>
  );
};