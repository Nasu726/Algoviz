import React, { useEffect, useRef } from 'react';
import { PixiGraphApp } from './PixiGraphApp';
import type { VisualizerEngine } from '../../types/engine';

interface GraphRendererProps {
  engine: VisualizerEngine;
  showWeights: boolean;
}

export const GraphRenderer: React.FC<GraphRendererProps> = ({ engine, showWeights }) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const pixiAppRef = useRef<PixiGraphApp | null>(null);

  // 初回マウント時のセットアップ
  useEffect(() => {
    if (!containerRef.current || !engine) return;

    const app = new PixiGraphApp(containerRef.current, engine);
    pixiAppRef.current = app;
    app.init();

    // 外側の要素に合わせてキャンバスの大きさを追従させる
    const observer = new ResizeObserver((entries) => {
      for (const entry of entries) {
        app.resize(entry.contentRect.width, entry.contentRect.height);
      }
    });
    observer.observe(containerRef.current);

    return () => {
      observer.disconnect();
      app.destroy();
      pixiAppRef.current = null;
    };
  }, [engine]);

  // 表示の好みが変わった時にPixiJS側に通知
  useEffect(() => {
    pixiAppRef.current?.updateSettings({ showWeights });
  }, [showWeights]);

  return (
    <div
      ref={containerRef}
      style={{
        flex: 1,
        minWidth: 0,
        border: '1px solid #ccc',
        borderRadius: '8px',
        overflow: 'hidden',
        backgroundColor: '#fff',
      }}
    />
  );
};
