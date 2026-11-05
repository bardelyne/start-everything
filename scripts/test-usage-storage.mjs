import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const output = process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'start-usage-storage-'));
fs.mkdirSync(output, {recursive: true});
const source = fs.readFileSync(path.join(root, 'start-everything.wh.cpp'), 'utf8');
function part(a, b) {
    const first = source.indexOf(a), last = source.indexOf(b, first + a.length);
    if (first < 0 || last < 0) throw Error('Missing production storage boundary');
    return source.slice(first, last);
}
const cpp = path.join(output, 'usage-storage.cpp'), exe = path.join(output, 'usage-storage.exe');
fs.writeFileSync(cpp, fs.readFileSync(path.join(root, 'tests/usage-storage.cpp'), 'utf8')
    .replace('// PRODUCTION_TEXT', part('// <search-text>', '// </search-text>'))
    .replace('// PRODUCTION_USAGE', part('using UsageCounts =', 'inline std::wstring ResolveAppParsingPath(')));
const options = {encoding: 'utf8', windowsHide: true, timeout: 120000, env: {...process.env, TEMP: output, TMP: output}};
const compiler = process.env.CXX || 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe';
const built = spawnSync(compiler, ['--target=x86_64-w64-mingw32', '-std=c++23', '-O2', '-static', '-municode',
    cpp, '-o', exe], options);
if (built.status !== 0) throw Error(built.stderr || String(built.error));
const ran = spawnSync(exe, [path.join(output, 'private-usage-' + Date.now() + '.ini')], options);
const log = (ran.stdout || '') + (ran.stderr || '');
fs.writeFileSync(path.join(output, 'results.log'), log);
process.stdout.write(log);
if (ran.error) process.stderr.write(String(ran.error));
process.exitCode = ran.status ?? 1;
