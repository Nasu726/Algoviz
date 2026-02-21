import { useEffect } from 'react';

interface ShortcutHandlers {
  onEsc?: () => void;        // Esc (ビジュアライザ選択画面へ戻る)
  onSave?: () => void;      // Ctrl+S (Load/Reset)
  onSaveFile?: () => void;      // Ctrl+Alt+S (Save code as a File)
  onFocus?: () => void;      // Ctrl+F (Focus)
  onPlayPause?: () => void; // Space or Ctrl+Enter
  onStepNext?: () => void;  // Ctrl+Right
  onStepBack?: () => void;  // Ctrl+Left
  onSpeedUp?: () => void;  // Ctrl+Up
  onSpeedDown?: () => void;  // Ctrl+Down
  onHelp?: () => void;      // Ctrl+H
}

export const useKeyboardShortcuts = (handlers: ShortcutHandlers) => {
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // Ctrl (Command) キーが押されているか
      const isCtrl = e.ctrlKey || e.metaKey;

      // Esc: ビジュアライザ選択画面へ戻る
      if (e.key === 'Escape') {
        e.preventDefault();
        handlers.onEsc?.();
      }

      // Ctrl + H: ヘルプ
      if (isCtrl && e.key === 'h') {
        e.preventDefault();
        handlers.onHelp?.();
      }

      // Ctrl + S: コードをファイルとして保存
      if (isCtrl && e.key === 's' && !e.altKey) {
        e.preventDefault();
        handlers.onSave?.();
      }

      // Ctrl + S: コードをファイルとして保存
      if (isCtrl && e.key === 's' && e.altKey) {
        e.preventDefault();
        handlers.onSaveFile?.();
      }

      // Ctrl + F: 追従切り替え
      if (isCtrl && e.key === 'f') {
        e.preventDefault();
        handlers.onFocus?.();
      }

      // Ctrl + Enter: 実行/停止
      if (isCtrl && e.key === 'Enter') {
        e.preventDefault();
        handlers.onPlayPause?.();
      }
      
      // Ctrl + →: 進む
      if (isCtrl && e.key === 'ArrowRight') {
        handlers.onStepNext?.();
      }

      // Ctrl + ←: 戻る
      if (isCtrl && e.key === 'ArrowLeft') {
        handlers.onStepBack?.();
      }

      // Ctrl + ↑: スピードアップ
      if (isCtrl && e.key === 'ArrowUp') {
        handlers.onSpeedUp?.();
      }

      // Ctrl + ↓: スピードダウン
      if (isCtrl && e.key === 'ArrowDown') {
        handlers.onSpeedDown?.();
      }
    
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [handlers]); // ハンドラが変わったら再登録
};