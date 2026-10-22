import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const output=process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(),'start-everything-material-tests-'));
fs.mkdirSync(output,{recursive:true});
const source=fs.readFileSync(path.join(root,'start-everything.wh.cpp'),'utf8');
function part(first,last){const a=source.indexOf(first),b=source.indexOf(last,a+first.length);if(a<0||b<0)throw Error('Missing production boundary: '+first);return source.slice(a,b);}
const data=part('// Bounded, pointer-free snapshots','struct SearchSurfaceData');
const effects=part('// Small Direct2D effect graph for Styler glass:','SearchSurfaceData CaptureLiveSurface(');
const recreate=part('wf::IInspectable RecreateSearchEffectValue','struct SearchGlassBrush :');
const colors='uint32_t SearchPackColor(winrt::Windows::UI::Color c){return (uint32_t(c.A)<<24)|(uint32_t(c.R)<<16)|(uint32_t(c.G)<<8)|c.B;}';
const cpp=path.join(output,'material-recovery.cpp');
fs.writeFileSync(cpp,fs.readFileSync(path.join(root,'tests/material-recovery.cpp'),'utf8').replace('// PRODUCTION',data+'\n'+colors+'\n'+effects+'\n'+recreate));
const manifest=path.join(output,'test.manifest');
fs.writeFileSync(manifest,'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1"><assemblyIdentity version="1.0.0.0" processorArchitecture="amd64" name="StartEverything.MaterialTests" type="win32"/><compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1"><application><maxversiontested Id="10.0.22621.0"/><supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/></application></compatibility></assembly>');
const embed=path.join(output,'embed-manifest.cpp');
fs.writeFileSync(embed,`#include <windows.h>
int wmain(int argc,wchar_t** argv){if(argc!=3)return 2;HANDLE file=CreateFileW(argv[2],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(file==INVALID_HANDLE_VALUE)return 3;char bytes[4096];DWORD length=0;BOOL ok=ReadFile(file,bytes,sizeof(bytes),&length,nullptr);CloseHandle(file);if(!ok||!length||length==sizeof(bytes))return 4;HANDLE update=BeginUpdateResourceW(argv[1],FALSE);if(!update)return 5;if(!UpdateResourceW(update,MAKEINTRESOURCEW(24),MAKEINTRESOURCEW(1),MAKELANGID(LANG_NEUTRAL,SUBLANG_NEUTRAL),bytes,length)){EndUpdateResourceW(update,TRUE);return 6;}return EndUpdateResourceW(update,FALSE)?0:7;}
`);
const compiler=process.env.CXX || 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe';
const options={encoding:'utf8',windowsHide:true,env:{...process.env,TEMP:output,TMP:output}};
function run(file,args){const r=spawnSync(file,args,{...options,timeout:120000});if(r.status!==0){process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||String(r.error||''));process.exit(r.status??1);}return r.stdout||'';}
const exe=path.join(output,'material-recovery.exe'),embedExe=path.join(output,'embed-manifest.exe');
run(compiler,['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static',cpp,'-o',exe,'-ld2d1','-lole32','-loleaut32','-lruntimeobject','-luuid','-luser32']);
run(compiler,['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static','-municode',embed,'-o',embedExe]);
run(embedExe,[exe,manifest]);
const result=spawnSync(exe,[],{...options,timeout:120000});
fs.writeFileSync(path.join(output,'results.log'),(result.stdout||'')+(result.stderr||''));
process.stdout.write((result.stdout||'').split('\n').filter(line=>/FAIL|ERROR|FAULT|RESULT|CACHE/.test(line)).join('\n')+'\n');
if(result.stderr)process.stderr.write(result.stderr);
if(result.error)process.stderr.write(String(result.error));
if(result.status!==0)process.exit(result.status??1);
const shutdown=spawnSync(exe,['--exit-live-cache'],{...options,timeout:30000});
fs.writeFileSync(path.join(output,'process-exit.log'),(shutdown.stdout||'')+(shutdown.stderr||''));
process.stdout.write(shutdown.stdout||'');
if(shutdown.stderr)process.stderr.write(shutdown.stderr);
if(shutdown.error)process.stderr.write(String(shutdown.error));
process.exit(shutdown.status??1);
