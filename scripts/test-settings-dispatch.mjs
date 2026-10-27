import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const out=process.argv[2]||fs.mkdtempSync(path.join(os.tmpdir(),'start-settings-dispatch-'));
fs.mkdirSync(out,{recursive:true});
const source=fs.readFileSync(path.join(root,'start-everything.wh.cpp'),'utf8');
function part(first,last){const a=source.indexOf(first),b=source.indexOf(last,a+first.length);if(a<0||b<0)throw Error(first);return source.slice(a,b);}
let test=fs.readFileSync(path.join(root,'tests/settings-dispatch.cpp'),'utf8')
 .replace('// PRODUCTION_REFRESH',part('void RequestAppIndexRefresh() {','// A row as the XAML thread needs it:'))
 .replace('// PRODUCTION_HELPER',part('static LRESULT CALLBACK ExplorerHelperWndProc(', '// WindowsXamlManager::Close schedules asynchronous cleanup'))
 .replace('// PRODUCTION_SETTINGS',part('void Wh_ModSettingsChanged() {','void Wh_ModUninit() {'));
const cpp=path.join(out,'settings-dispatch.cpp'),exe=path.join(out,'settings-dispatch.exe');fs.writeFileSync(cpp,test);
const options={encoding:'utf8',windowsHide:true,env:{...process.env,TEMP:out,TMP:out},timeout:60000};
const built=spawnSync(process.env.CXX||'C:/Program Files/Windhawk/Compiler/bin/clang++.exe',
 ['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static',cpp,'-luser32','-o',exe],options);
if(built.status!==0){process.stderr.write(built.stderr||String(built.error));process.exit(built.status??1);}
const ran=spawnSync(exe,[],{...options,timeout:10000});const result=(ran.stdout||'')+(ran.stderr||'');
fs.writeFileSync(path.join(out,'results.log'),result);process.stdout.write(result);if(ran.error)console.error(ran.error);process.exit(ran.status??1);
