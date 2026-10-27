import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const output=process.argv[2]||fs.mkdtempSync(path.join(os.tmpdir(),'start-app-warmup-'));fs.mkdirSync(output,{recursive:true});
const source=fs.readFileSync(path.join(root,'start-everything.wh.cpp'),'utf8');
const teardown=source.slice(source.indexOf('void SearchWindowTeardown() {'),source.indexOf('void RefreshSearchUiSettings() {'));
if(!teardown.includes('StopAppsWarmup();') || teardown.indexOf('StopAppsWarmup();')>teardown.indexOf('g_searchThread->join();') ||
   teardown.indexOf('StopAppsWarmup();')>teardown.indexOf('TeardownSearchUi();'))throw Error('Native I/O must stop before worker/UI cleanup');
function part(first,last){const a=source.indexOf(first),b=source.indexOf(last,a+first.length);if(a<0||b<0)throw Error(first);return source.slice(a,b);}
const cpp=path.join(output,'app-warmup.cpp'),exe=path.join(output,'app-warmup.exe');
fs.writeFileSync(cpp,fs.readFileSync(path.join(root,'tests/app-warmup.cpp'),'utf8')
 .replace('// PRODUCTION_WARMUP',part('class WarmIndex {','}  // namespace apps'))
 .replace('// PRODUCTION_STARTUP',part('static void TryInitializeExplorerSearch(HWND helper) {','static void RequestExplorerSearch(HWND helper) {')));
const options={encoding:'utf8',windowsHide:true,env:{...process.env,TEMP:output,TMP:output},timeout:60000};
const compiler=process.env.CXX||'C:/Program Files/Windhawk/Compiler/bin/clang++.exe';
const built=spawnSync(compiler,['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static',cpp,'-o',exe,'-lole32'],options);
if(built.status!==0){process.stderr.write(built.stderr||String(built.error));process.exit(built.status??1);}
const ran=spawnSync(exe,[],options),result=(ran.stdout||'')+(ran.stderr||'');
fs.writeFileSync(path.join(output,'results.log'),result);process.stdout.write(result);if(ran.error)process.stderr.write(String(ran.error));process.exit(ran.status??1);
