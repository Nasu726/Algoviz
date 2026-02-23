import React, { useEffect, useRef } from 'react';
import { PixiGraphApp } from './PixiGraphApp'; // 先ほど作ったクラスをインポート

interface GraphRendererProps {
  engine: any;
  isDirected?: boolean;
}

export const GraphRenderer: React.FC<GraphRendererProps> = ({ engine, isDirected = false }) => {
  const containerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!containerRef.current || !engine) return;

    // 1. クラスをインスタンス化
    const pixiApp = new PixiGraphApp(containerRef.current, engine, isDirected);
    
    // 2. 初期化を実行
    pixiApp.init();

    // 3. コンポーネントが消える時の後片付けは、クラスに丸投げするだけ！
    return () => {
      pixiApp.destroy();
    };
  }, [engine, isDirected]);

  return (
    <div 
      ref={containerRef} 
      style={{ border: '1px solid #ccc', marginTop: '20px', width: '600px', height: '400px' }} 
    />
  );
};