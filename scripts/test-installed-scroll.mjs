import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const output=process.argv[2]||fs.mkdtempSync(path.join(os.tmpdir(),'start-installed-scroll-'));fs.mkdirSync(output,{recursive:true});
const source=fs.readFileSync(path.join(root,'start-everything.wh.cpp'),'utf8');
function part(a,b){const i=source.indexOf(a),j=source.indexOf(b,i+a.length);if(i<0||j<0)throw Error(a);return source.slice(i,j);}
const code=fs.readFileSync(path.join(root,'tests/installed-scroll.cpp'),'utf8')
 .replace('// PRODUCTION_MODEL',part('struct WeatherDayRow {','// Icons take 24 logical pixels'))
 .replace('// PRODUCTION_BATCH',part('inline constexpr size_t kInstalledBatchSize=','std::vector<Row> g_fileRows;'))
 .replace('// PRODUCTION_SCROLL',part('// UI-thread scroll notification;','// The XAML thread keeps the displayed files'));
const cpp=path.join(output,'installed-scroll.cpp'),exe=path.join(output,'installed-scroll.exe');fs.writeFileSync(cpp,code);
const compiler=process.env.CXX||'C:/Program Files/Windhawk/Compiler/bin/clang++.exe';
const options={encoding:'utf8',windowsHide:true,env:{...process.env,TEMP:output,TMP:output},timeout:120000};
function run(file,args){const r=spawnSync(file,args,options);if(r.status!==0)throw Error((r.stdout||'')+(r.stderr||String(r.error||'')));return r.stdout||'';}
const common=['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static'];
run(compiler,[...common,cpp,'-o',exe,'-lole32','-loleaut32','-lruntimeobject','-luser32']);
const manifest=path.join(output,'test.manifest');
fs.writeFileSync(manifest,'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1"><assemblyIdentity version="1.0.0.0" processorArchitecture="amd64" name="StartEverything.InventoryScrollTests" type="win32"/><compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1"><application><maxversiontested Id="10.0.22621.0"/><supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/></application></compatibility></assembly>');
const embed=path.join(output,'embed-manifest.cpp');
fs.writeFileSync(embed,'#include <windows.h>\nint wmain(int argc,wchar_t** argv){if(argc!=3)return 2;HANDLE file=CreateFileW(argv[2],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(file==INVALID_HANDLE_VALUE)return 3;char bytes[4096];DWORD length=0;BOOL ok=ReadFile(file,bytes,sizeof(bytes),&length,nullptr);CloseHandle(file);if(!ok||!length||length==sizeof(bytes))return 4;HANDLE update=BeginUpdateResourceW(argv[1],FALSE);if(!update)return 5;if(!UpdateResourceW(update,MAKEINTRESOURCEW(24),MAKEINTRESOURCEW(1),MAKELANGID(LANG_NEUTRAL,SUBLANG_NEUTRAL),bytes,length)){EndUpdateResourceW(update,TRUE);return 6;}return EndUpdateResourceW(update,FALSE)?0:7;}\n');
run(compiler,[...common,'-municode',embed,'-o',path.join(output,'embed-manifest.exe')]);
run(path.join(output,'embed-manifest.exe'),[exe,manifest]);
const result=run(exe,[]);fs.writeFileSync(path.join(output,'results.log'),result);process.stdout.write(result);
