import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const output=process.argv[2]||fs.mkdtempSync(path.join(os.tmpdir(),'start-result-batches-'));
fs.mkdirSync(output,{recursive:true});
const source=fs.readFileSync(path.join(root,'start-everything.wh.cpp'),'utf8');
function part(first,last){const a=source.indexOf(first),b=source.indexOf(last,a+first.length);if(a<0||b<0)throw Error(first);return source.slice(a,b);}
let code=fs.readFileSync(path.join(root,'tests/result-batches.cpp'),'utf8')
 .replace('// PRODUCTION_BATCH_SIZES',part('inline constexpr size_t kInstalledBatchSize=','std::wstring g_installedBatchQuery;'))
 .replace('// PRODUCTION_EVERYTHING',part('namespace everything {','}  // namespace everything')+'}\n')
 .replace('// PRODUCTION_RANKER',part('namespace filefilter {','}  // namespace filefilter')+'}\n')
 .replace('// PRODUCTION_PAGES',part('// <file-result-pages>','// </file-result-pages>'))
 .replace('// PRODUCTION_BATCHES',part('struct FileSearchRequest {','// Publish only the current request.'));
// Only the isolated provider is visible to the generated client's discovery.
code=code.replaceAll('EVERYTHING_TASKBAR_NOTIFICATION','StartEverything.PrivatePagingTestProvider');
const cpp=path.join(output,'result-batches.cpp'),exe=path.join(output,'result-batches.exe');
fs.writeFileSync(cpp,code);
const compiler=process.env.CXX||'C:/Program Files/Windhawk/Compiler/bin/clang++.exe';
const options={encoding:'utf8',windowsHide:true,timeout:120000,env:{...process.env,TEMP:output,TMP:output}};
let result=spawnSync(compiler,['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static',cpp,'-o',exe,'-luser32'],options);
if(result.status!==0)throw Error((result.stdout||'')+(result.stderr||String(result.error||'')));
result=spawnSync(exe,[],{...options,timeout:15000});
const log=(result.stdout||'')+(result.stderr||String(result.error||''));
fs.writeFileSync(path.join(output,'results.log'),log);process.stdout.write(log);process.exitCode=result.status??1;
