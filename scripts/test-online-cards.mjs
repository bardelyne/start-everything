import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const output = process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'start-everything-online-'));
fs.mkdirSync(output, {recursive:true});
const source = fs.readFileSync(path.join(root, 'start-everything.wh.cpp'), 'utf8').replaceAll('\r\n','\n');
function part(first, last) {
    const start = source.indexOf(first), end = source.indexOf(last, start + first.length);
    if (start < 0 || end < 0) throw Error('Missing production boundary: ' + first);
    return source.slice(start, end);
}
const fragments = {
    SPEED_FORMAT: part('std::wstring SpeedMeterNumber(', 'constexpr ULONGLONG kSpeedMeterEaseMs'),
    COMMANDS: part('namespace commands {', '}  // namespace commands') + '}\n',
    I18N: part('namespace i18n {', '// ==/StartEverythingLocalization=='),
    TOOLS: 'namespace tools {\n' + part('inline std::wstring ToLower(const std::wstring& str)', '// 4. Math Expression Evaluator').split('// ---------------------------------------------------------------------------')[0] + '\n}\n',
    NUMBERS: 'namespace calc {\n' + part('inline wchar_t DecimalPoint()', '// Scientific notation, as 9.9e4') + '\n}\n',
    MODEL: part('struct WeatherDayRow {', '// Icons take 24 logical pixels') + part('std::vector<Row> g_appRows;', '// Launch requests,'),
    ONLINE: part('namespace online {\n', '}  // namespace online') + '}\n',
    SNAPSHOT: part('struct SearchRenderSnapshot {', '// Replace a fallback icon'),
    PENDING: part('bool ResultsPending() {', '// What Enter does:'),
};
let test = fs.readFileSync(path.join(root, 'tests/online-cards.cpp'), 'utf8');
for (const [key, value] of Object.entries(fragments)) test = test.replace('// PRODUCTION_' + key, value);
const cpp = path.join(output, 'online-cards.cpp'), exe = path.join(output, 'online-cards.exe');
fs.writeFileSync(cpp, test);
const compiler = process.env.CXX || 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe';
const options = {encoding:'utf8', windowsHide:true, env:{...process.env, TEMP:output, TMP:output}, timeout:120000};
const built = spawnSync(compiler, ['--target=x86_64-w64-mingw32', '-std=c++23', '-O2', '-static',
    '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', cpp, '-o', exe, '-lole32', '-loleaut32', '-lruntimeobject', '-lwinhttp', '-lbcrypt'], options);
if (built.status !== 0) { process.stderr.write((built.stdout||'')+(built.stderr||String(built.error))); process.exit(built.status??1); }
const args = process.argv.includes('--live') ? ['--live'] : [];
const ran = spawnSync(exe, args, {...options, timeout:60000});
fs.writeFileSync(path.join(output, args.length ? 'live-results.log' : 'results.log'), (ran.stdout||'')+(ran.stderr||'')+String(ran.error||''));
process.stdout.write(ran.stdout||''); process.stderr.write(ran.stderr||'');
if(ran.error)process.stderr.write(String(ran.error));
process.exit(ran.status??1);
