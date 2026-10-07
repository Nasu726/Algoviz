import js from '@eslint/js'
import globals from 'globals'
import reactHooks from 'eslint-plugin-react-hooks'
import reactRefresh from 'eslint-plugin-react-refresh'
import tseslint from 'typescript-eslint'
import { defineConfig, globalIgnores } from 'eslint/config'

export default defineConfig([
  globalIgnores(['dist']),
  {
    files: ['**/*.{ts,tsx}'],
    extends: [
      js.configs.recommended,
      tseslint.configs.recommended,
      reactHooks.configs.flat.recommended,
      reactRefresh.configs.vite,
    ],
    languageOptions: {
      ecmaVersion: 2020,
      globals: globals.browser,
    },
    rules: {
      // eslint-plugin-react-hooks 7.1 で入った React Compiler 由来のルール。
      // 各ページが使っている2つの形を一律に禁じるので外してある。
      //   - 「入力欄の最新値を持つ ref を描画中に書く」
      //     (依存配列に入れると1文字打つたびに C++ 側を作り直すことになる)
      //   - 「初期化の effect で engine の状態を React の状態に読む」
      //     (engine は外部の状態なので、読む側はここしか無い)
      // どちらも意図した形。直すならページの作りごとで、別の判断。
      'react-hooks/refs': 'off',
      'react-hooks/immutability': 'off',
      'react-hooks/set-state-in-effect': 'off',
    },
  },
])
