import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const source=fs.readFileSync(path.join(root,'start-everything.wh.cpp'),'utf8');
const output=process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(),'start-everything-tests-'));
fs.mkdirSync(output,{recursive:true});
function between(first,last) {
    const start=source.indexOf(first), end=source.indexOf(last,start);
    if(start<0||end<0) throw Error('Missing production boundary: '+first);
    return source.slice(start,end);
}
const includes=`#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <string>
#include <string_view>
#include <initializer_list>
#include <vector>
#include <random>
#include <mutex>
#include <limits>
#include <cstdint>
#include <thread>
#include <atomic>
`;
const i18n=between('namespace i18n {','// ==/StartEverythingLocalization==');
const tools='namespace tools {\n'+between('inline std::wstring FormatCleanNumber','} // namespace tools')+'}\n';
const custom=between('struct CustomConversion {','struct Settings {');
// Row/settings fields used by the production calculator; no app index or UI doubles.
const model=`
struct Row { std::wstring title,subtitle,copyText,customGlyph; uint32_t glyphColor=0; bool canRunAsAdmin=true; };
struct Settings { std::vector<CustomConversion> unitConversions; } g_settings;
std::mutex g_settingsMutex;
`;
const calculator=between('namespace calc {','}  // namespace calc')+'}\n';
const cpp=path.join(output,'calculator-localization.cpp');
fs.writeFileSync(cpp,includes+i18n+tools+custom+model+calculator+fs.readFileSync(path.join(root,'tests/calculator-localization.cpp'),'utf8'));
const compiler=process.env.CXX || 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe';
const exe=path.join(output,'calculator-localization.exe');
const options={encoding:'utf8',windowsHide:true,env:{...process.env,TEMP:output,TMP:output}};
const built=spawnSync(compiler,['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static',cpp,'-o',exe],options);
if(built.status!==0) { process.stderr.write(built.stderr||String(built.error)); process.exit(built.status??1); }
const ran=spawnSync(exe,[],{...options,timeout:120000});
fs.writeFileSync(path.join(output,'results.log'),(ran.stdout||'')+(ran.stderr||'')+String(ran.error||''));
process.stdout.write(ran.stdout||'');process.stderr.write(ran.stderr||'');
if(ran.error) process.stderr.write(String(ran.error));
process.exit(ran.status??1);
