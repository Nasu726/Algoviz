// emcc が吐くテスト用 JS は CommonJS だが、ルートの package.json が
// "type": "module" なので、出力先だけスコープを切って CommonJS に戻す。
// test/out は .gitignore 済みなので、クローン直後でも作れるようにここで用意する。
import { mkdirSync, writeFileSync } from 'node:fs';

mkdirSync('test/out', { recursive: true });
writeFileSync('test/out/package.json', '{ "type": "commonjs" }\n');
