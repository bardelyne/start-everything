import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const output=process.argv[2]||fs.mkdtempSync(path.join(os.tmpdir(),'start-hardware-'));
fs.mkdirSync(output,{recursive:true});
const source=fs.readFileSync(path.join(root,'start-everything.wh.cpp'),'utf8').replaceAll('\r\n','\n');
function part(first,last){const a=source.indexOf(first),b=source.indexOf(last,a+first.length);if(a<0||b<0)throw Error(first);return source.slice(a,b);}
let test=fs.readFileSync(path.join(root,'tests/hardware-cards.cpp'),'utf8')
 .replace('// PRODUCTION_MODEL',part('struct WeatherDayRow {','// Icons take 24 logical pixels'))
 .replace('// PRODUCTION_HARDWARE',part('namespace hardware {\nconstexpr size_t kHistory', '}  // namespace hardware')+'}\n');
const cpp=path.join(output,'hardware-cards.cpp'),exe=path.join(output,'hardware-cards.exe');fs.writeFileSync(cpp,test);
const options={encoding:'utf8',windowsHide:true,env:{...process.env,TEMP:output,TMP:output},timeout:120000};
const built=spawnSync(process.env.CXX||'C:/Program Files/Windhawk/Compiler/bin/clang++.exe',[
 '--target=x86_64-w64-mingw32','-std=c++23','-O2','-static','-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00',cpp,'-o',exe,
 '-lpdh','-ldxgi','-liphlpapi','-ladvapi32','-lole32','-lruntimeobject'],options);
if(built.status!==0){process.stderr.write((built.stdout||'')+(built.stderr||String(built.error)));process.exit(built.status??1);}
const native=process.argv.includes('--require-counters');
const ran=spawnSync(exe,native?['--require-counters']:[],{...options,timeout:30000});const log=(ran.stdout||'')+(ran.stderr||'')+String(ran.error||'');
fs.writeFileSync(path.join(output,native?'native-results.log':'results.log'),log);process.stdout.write(log);process.exit(ran.status??1);
