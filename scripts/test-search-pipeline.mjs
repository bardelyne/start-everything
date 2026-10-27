import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const output = process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'start-everything-pipeline-'));
fs.mkdirSync(output, {recursive: true});
const source = fs.readFileSync(path.join(root, 'start-everything.wh.cpp'), 'utf8');
function between(first, last) {
    const start = source.indexOf(first), end = source.indexOf(last, start + first.length);
    if (start < 0 || end < 0) throw Error('Missing production boundary: ' + first);
    return source.slice(start, end);
}
const fragments = {
    WARMUP: between('class WarmIndex {', '}  // namespace apps'),
    COMMANDS: between('namespace commands {', '}  // namespace commands') + '}\n',
    MODEL: between('struct WeatherDayRow {', '// Icons take 24 logical pixels') +
        between('std::vector<Row> g_appRows;', '// Opens what was clicked.'),
    PENDING: between('bool ResultsPending() {', '// What Enter does:'),
    RENDER_SNAPSHOT: between('struct SearchRenderSnapshot {', '// Replace a fallback icon'),
    QUEUE: between('void QueueQuery(std::wstring text) {', '// An app\'s icon, from its shell identity.'),
    WORKERS: between('void FileSearchThreadMain() {', 'void TeardownSearchUi() {'),
};
let test = fs.readFileSync(path.join(root, 'tests/search-pipeline.cpp'), 'utf8');
for (const [name, code] of Object.entries(fragments)) test = test.replace('// PRODUCTION_' + name, code);
const cpp = path.join(output, 'search-pipeline.cpp'), exe = path.join(output, 'search-pipeline.exe');
fs.writeFileSync(cpp, test);
const compiler = process.env.CXX || 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe';
const options = {encoding: 'utf8', windowsHide: true, env: {...process.env, TEMP: output, TMP: output}, timeout: 120000};
const built = spawnSync(compiler, ['--target=x86_64-w64-mingw32', '-std=c++23', '-O2', '-static', cpp, '-o', exe,
    '-lole32', '-lshell32'], options);
if (built.status !== 0) {
    process.stderr.write((built.stdout || '') + (built.stderr || String(built.error || '')));
    process.exit(built.status ?? 1);
}
const ran = spawnSync(exe, [], {...options, timeout: 30000});
fs.writeFileSync(path.join(output, 'results.log'), (ran.stdout || '') + (ran.stderr || '') + String(ran.error || ''));
process.stdout.write(ran.stdout || '');
process.stderr.write(ran.stderr || '');
if (ran.error) process.stderr.write(String(ran.error));
process.exit(ran.status ?? 1);
