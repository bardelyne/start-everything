import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const output=process.argv[2]||fs.mkdtempSync(path.join(os.tmpdir(),'start-everything-services-'));
fs.mkdirSync(output,{recursive:true});
const source=fs.readFileSync(path.join(root,'start-everything.wh.cpp'),'utf8').replaceAll('\r\n','\n');
function part(a,b){const start=source.indexOf(a),end=source.indexOf(b,start+a.length);if(start<0||end<0)throw Error(a);return source.slice(start,end);}
let code=fs.readFileSync(path.join(root,'tests/services.cpp'),'utf8');
for(const [key,value] of Object.entries({
 I18N:part('namespace i18n {','// ==/StartEverythingLocalization=='),
 COMMANDS:part('namespace commands {','}  // namespace commands')+'}\n',
 MODEL:part('struct WeatherDayRow {','// Icons take 24 logical pixels'),
 BASE64:part('std::wstring EncodePowerShellCommand(','std::wstring ExpandCustomShellEnvironment('),
 SERVICES:part('namespace services {\nstruct Entry','}  // namespace services')+'}\n',
}))code=code.replace('// PRODUCTION_'+key,value);
const cpp=path.join(output,'services.cpp'),exe=path.join(output,'services.exe');fs.writeFileSync(cpp,code);
const options={encoding:'utf8',windowsHide:true,env:{...process.env,TEMP:output,TMP:output},timeout:120000};
const fixture=spawnSync('C:/Program Files/Windhawk/Compiler/bin/clang++.exe',['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static','-municode',path.join(root,'tests/service-fixture.cpp'),'-o',path.join(output,'service-fixture.exe'),'-ladvapi32'],options);
if(fixture.status!==0){process.stderr.write((fixture.stdout||'')+(fixture.stderr||String(fixture.error)));process.exit(fixture.status??1);}
const built=spawnSync('C:/Program Files/Windhawk/Compiler/bin/clang++.exe',['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static',cpp,'-o',exe,'-ladvapi32','-lshell32','-lshlwapi'],options);
if(built.status!==0){process.stderr.write((built.stdout||'')+(built.stderr||String(built.error)));process.exit(built.status??1);}
const ran=spawnSync(exe,[],{...options,timeout:30000});
fs.writeFileSync(path.join(output,'results.log'),(ran.stdout||'')+(ran.stderr||'')+String(ran.error||''));
process.stdout.write(ran.stdout||'');process.stderr.write(ran.stderr||'');if(ran.error)process.stderr.write(String(ran.error));
if(ran.status!==0)process.exit(ran.status??1);
const shutdown=spawnSync(exe,['--exit-with-idle-worker'],{...options,timeout:5000});
fs.writeFileSync(path.join(output,'shutdown-results.log'),(shutdown.stdout||'')+(shutdown.stderr||'')+String(shutdown.error||''));
if(shutdown.status!==0){process.stderr.write((shutdown.stdout||'')+(shutdown.stderr||String(shutdown.error||'')));process.exit(shutdown.status??1);}
process.stdout.write('PASS: process exit with a joinable service worker does not terminate the host\n');
