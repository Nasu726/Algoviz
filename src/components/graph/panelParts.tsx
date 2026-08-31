import React from 'react';
import { hex } from './types';

// グラフ系ページのサイドバーで共有する小さな部品。

export const Section: React.FC<{ title: string; children: React.ReactNode }> = ({ title, children }) => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        <h3 style={{
            margin: 0, fontSize: '14px', color: '#37474f',
            borderBottom: '1px solid #cfd8dc', paddingBottom: '4px',
        }}>
            {title}
        </h3>
        {children}
    </div>
);

export const Swatch: React.FC<{ color: number; label: string; isEdge?: boolean }> = ({ color, label, isEdge }) => (
    <span style={{ display: 'inline-flex', alignItems: 'center', gap: '5px', fontSize: '12px', whiteSpace: 'nowrap' }}>
        <span style={{
            width: isEdge ? '16px' : '12px',
            height: isEdge ? '3px' : '12px',
            borderRadius: isEdge ? '2px' : '50%',
            border: isEdge ? 'none' : '3px solid ' + hex(color),
            backgroundColor: isEdge ? hex(color) : '#fff',
            flexShrink: 0,
        }} />
        {label}
    </span>
);

export const Field: React.FC<{ label: string; children: React.ReactNode }> = ({ label, children }) => (
    <label style={{ display: 'inline-flex', alignItems: 'center', gap: '4px' }}>
        {label}
        {children}
    </label>
);

export const Check: React.FC<{
    checked: boolean;
    onChange: (v: boolean) => void;
    children: React.ReactNode;
    disabled?: boolean;
}> = ({ checked, onChange, children, disabled }) => (
    <label style={{ display: 'block', opacity: disabled ? 0.45 : 1 }}>
        <input
            type="checkbox" checked={checked} disabled={disabled}
            onChange={(e) => onChange(e.target.checked)}
        />{' '}
        {children}
    </label>
);

// 数字だけを受け付ける入力欄
export const NumberInput: React.FC<{
    value: string;
    onChange: (v: string) => void;
    max?: number;
    width?: string;
    placeholder?: string;
}> = ({ value, onChange, max, width = '52px', placeholder }) => (
    <input
        type="text" value={value} placeholder={placeholder}
        onChange={(e) => onChange(e.target.value.replace(/[^0-9]/g, ''))}
        onBlur={() => {
            if (placeholder && value.trim() === '') return; // 省略可の欄は空のままにする
            if (value.trim() === '') onChange('0');
            else if (max !== undefined && Number(value) > max) onChange(String(max));
        }}
        style={{ width }}
    />
);
