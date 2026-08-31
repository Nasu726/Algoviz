// 実行速度スライダーの目盛りと delay(ms) の対応。
// 左端で 1秒/ステップ、右端で 0ms になるよう二乗で効かせている。
export const delayToSlider = (delay: number) => 1000 - Math.sqrt(1000 * delay);
export const sliderToDelay = (x: number) => ((x - 1000) * (x - 1000)) / 1000;

// キーボードショートカット (Ctrl + ↑ / ↓) 用。スライダーを 100 目盛り分動かすのと同じ。
export const speedUp = (delay: number) =>
    delay >= 10 ? Math.max(0, ((-Math.sqrt(1000 * delay) + 100) ** 2) / 1000) : 0;
export const speedDown = (delay: number) =>
    Math.max(0, ((-Math.sqrt(1000 * delay) - 100) ** 2) / 1000);
