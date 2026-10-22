import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const output = process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'start-everything-opening-input-'));
fs.mkdirSync(output, {recursive: true});
const source = fs.readFileSync(path.join(root, 'start-everything.wh.cpp'), 'utf8');
function part(first, last) {
    const start = source.indexOf(first), end = source.indexOf(last, start + first.length);
    if (start < 0 || end < 0) throw Error('Missing production boundary: ' + first);
    return source.slice(start, end);
}
const code = [
    part('struct SearchWindowInput {', 'void ClearSearchWindowDeadKey()'),
    part('void ClearSearchWindowDeadKey()', '// The foreground host'),
    part('bool QueueSearchWindowInput(HWND host', '// XAML can report logical'),
    part('SearchWindowInput NativeSearchKey(', '// Re-enter opening keys'),
    part('LRESULT CALLBACK SearchWindowKeyboardProc(', 'DWORD WINAPI SearchWindowInputThreadProc('),
    part('void ApplySearchWindowInput() try {', '// Move the complete host'),
].join('\n');
const cpp = path.join(output, 'opening-input.cpp'), exe = path.join(output, 'opening-input.exe');
fs.writeFileSync(cpp, fs.readFileSync(path.join(root, 'tests/opening-input.cpp'), 'utf8').replace('// PRODUCTION_INPUT', code));
const compiler = process.env.CXX || 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe';
const options = {encoding: 'utf8', windowsHide: true, env: {...process.env, TEMP: output, TMP: output}, timeout: 120000};
const build = spawnSync(compiler, ['--target=x86_64-w64-mingw32', '-std=c++23', '-O2', '-static', cpp, '-o', exe, '-luser32'], options);
if (build.status !== 0) {
    process.stderr.write((build.stdout || '') + (build.stderr || String(build.error || '')));
    process.exit(build.status ?? 1);
}
const result = spawnSync(exe, [], options);
fs.writeFileSync(path.join(output, 'results.log'), (result.stdout || '') + (result.stderr || ''));
process.stdout.write(result.stdout || '');
if (result.stderr) process.stderr.write(result.stderr);
if (result.error) process.stderr.write(String(result.error));
process.exit(result.status ?? 1);
