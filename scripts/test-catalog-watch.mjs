import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const output=process.argv[2]||fs.mkdtempSync(path.join(os.tmpdir(),'start-catalog-watch-'));fs.mkdirSync(output,{recursive:true});
const source=fs.readFileSync(path.join(root,'start-everything.wh.cpp'),'utf8');
const start=source.indexOf('struct CatalogChangeBatch {'),end=source.indexOf('[[clang::no_destroy]] std::optional<AppCatalogWatch>',start);
if(start<0||end<0)throw Error('Missing watcher fragment');
// Test-only access to register private fixtures, never real apps or shortcuts.
const production=source.slice(start,end).replaceAll('private:','public:');
const cpp=path.join(output,'catalog-watch.cpp'),exe=path.join(output,'catalog-watch.exe');
fs.writeFileSync(cpp,fs.readFileSync(path.join(root,'tests/catalog-watch.cpp'),'utf8').replace('// PRODUCTION_WATCH',production));
const options={encoding:'utf8',windowsHide:true,timeout:120000,env:{...process.env,TEMP:output,TMP:output}};
const built=spawnSync(process.env.CXX||'C:/Program Files/Windhawk/Compiler/bin/clang++.exe',['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static',cpp,'-o',exe,'-lole32','-loleaut32','-lruntimeobject','-lshell32','-ladvapi32','-luuid'],options);
if(built.status!==0){process.stderr.write((built.stdout||'')+(built.stderr||String(built.error)));process.exit(built.status??1);}
const ran=spawnSync(exe,[],{...options,timeout:30000});const log=(ran.stdout||'')+(ran.stderr||'')+String(ran.error||'');
fs.writeFileSync(path.join(output,'results.log'),log);process.stdout.write(log);process.exit(ran.status??1);
