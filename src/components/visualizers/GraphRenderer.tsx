import React, { useEffect, useRef } from 'react';
import { PixiGraphApp } from './PixiGraphApp';

interface GraphRendererProps {
  engine: any;
  isDirected: boolean;
  showWeights: boolean;
  isAutomaton: boolean;
  startNode: string;
  acceptingNodes: string;
}

export const GraphRenderer: React.FC<GraphRendererProps> = ({ 
    engine, isDirected, showWeights, isAutomaton, startNode, acceptingNodes 
}) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const pixiAppRef = useRef<PixiGraphApp | null>(null);

  // 初回マウント時のセットアップ
  useEffect(() => {
    if (!containerRef.current || !engine) return;

    pixiAppRef.current = new PixiGraphApp(containerRef.current, engine);
    pixiAppRef.current.init();

    return () => {
      if (pixiAppRef.current) {
        pixiAppRef.current.destroy();
        pixiAppRef.current = null;
      }
    };
  }, [engine]);

  // 設定が変わった時にPixiJS側に通知
  useEffect(() => {
    if (pixiAppRef.current) {
      pixiAppRef.current.updateSettings({
        isDirected,
        showWeights,
        isAutomaton,
        startNode,
        acceptingNodes
      });
    }
  }, [isDirected, showWeights, isAutomaton, startNode, acceptingNodes]);

  return (
    <div 
      ref={containerRef} 
      style={{ border: '1px solid #ccc', width: '800px', height: '600px', backgroundColor: '#fff' }} 
    />
  );
};