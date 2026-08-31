import { useState } from 'react';
import { useInterval } from 'react-use';
import { speedUp, speedDown } from '../components/ui/playbackSpeed';

// 再生の状態と一定間隔のループ。1手をどう進めるかはページごとに違うので
// tick で受け取り、ここは「動いているか」と「間隔」だけを持つ。
export const usePlayback = (tick: () => void, initialDelay = 300) => {
    const [isPlaying, setIsPlaying] = useState(false);
    const [delay, setDelay] = useState(initialDelay);

    useInterval(() => {
        if (isPlaying) tick();
    }, isPlaying ? delay : null);

    return {
        isPlaying,
        setIsPlaying,
        delay,
        setDelay,
        toggle: () => setIsPlaying(!isPlaying),
        onSpeedUp: () => setDelay(speedUp(delay)),
        onSpeedDown: () => setDelay(speedDown(delay)),
    };
};
