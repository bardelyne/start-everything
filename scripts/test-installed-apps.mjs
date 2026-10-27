import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const output=process.argv[2]||fs.mkdtempSync(path.join(os.tmpdir(),'start-installed-apps-'));fs.mkdirSync(output,{recursive:true});
const source=fs.readFileSync(path.join(root,'start-everything.wh.cpp'),'utf8').replaceAll('\r\n','\n');
function part(a,b){const i=source.indexOf(a),j=source.indexOf(b,i+a.length);if(i<0||j<0)throw Error(a);return source.slice(i,j);}
let fixture=fs.readFileSync(path.join(root,'tests/installed-apps.cpp'),'utf8');
if(process.argv.includes('--full-settings')) {
 const start=fixture.indexOf('namespace settings {'),end=fixture.indexOf('// PRODUCTION_CATALOG',start);
 if(start<0||end<0)throw Error('Missing Settings fixture boundary');
 fixture=fixture.slice(0,start)+part('namespace settings {','} // namespace settings')+'}\n'+fixture.slice(end);
}
let code=fixture
 .replace('// PRODUCTION_CATALOG',part('namespace installed {','}  // namespace installed')+'}\n')
 .replace('// PRODUCTION_UNINSTALL',part('namespace uninstall {','}  // namespace uninstall')+'}\n')
 .replace('// PRODUCTION_APPS',part('namespace apps {','}  // namespace apps')+'}\n')
 .replace('// PRODUCTION_PACKAGE_ICON',part('namespace icons {','inline bool BitmapToBgra(')+'}\n'+part('bool FetchInstalledPackageIcon(','bool FetchAppIcon('));
const cpp=path.join(output,'installed-apps.cpp'),exe=path.join(output,'installed-apps.exe');fs.writeFileSync(cpp,code);
const options={encoding:'utf8',windowsHide:true,env:{...process.env,TEMP:output,TMP:output},timeout:120000};
const compiler='C:/Program Files/Windhawk/Compiler/bin/clang++.exe';
function run(file,args){const result=spawnSync(file,args,options);if(result.status!==0)throw Error((result.stdout||'')+(result.stderr||String(result.error)));}
// Construct a small version resource for our own GUI/console fixtures. These
// executables are inspected, never launched by the discovery tests.
const wide=s=>Buffer.from(s+'\0','utf16le'),align=b=>Buffer.concat([b,Buffer.alloc((4-b.length%4)%4)]);
function block(key,type,value,children=[]){
 const header=Buffer.alloc(6);header.writeUInt16LE(type?value.length/2:value.length,2);header.writeUInt16LE(type,4);
 let bytes=Buffer.concat([align(Buffer.concat([header,wide(key)])),value]);
 for(const child of children)bytes=Buffer.concat([align(bytes),child]);
 bytes.writeUInt16LE(bytes.length,0);return bytes;
}
const fixed=Buffer.alloc(52);[0xfeef04bd,0x10000,0x10000,0,0x10000,0,0x3f,0,0x40004,1,0,0,0].forEach((v,i)=>fixed.writeUInt32LE(v,i*4));
const translation=Buffer.from([0x09,0x04,0xb0,0x04]);
const version=block('VS_VERSION_INFO',0,fixed,[block('StringFileInfo',1,Buffer.alloc(0),[block('040904b0',1,Buffer.alloc(0),[
 block('ProductName',1,wide('FixtureTool')),block('FileDescription',1,wide('Fixture Tool')),block('OriginalFilename',1,wide('FixtureTool.exe'))
])]),block('VarFileInfo',1,Buffer.alloc(0),[block('Translation',0,translation)])]);
const versionFile=path.join(output,'fixture-version.bin');fs.writeFileSync(versionFile,version);
const common=['--target=x86_64-w64-mingw32','-std=c++23','-O2','-static'];
const fixtureSource=path.join(root,'tests/launcher-fixture.cpp'),writer=path.join(output,'resource-writer.exe');
run(compiler,[...common,'-municode','-DLAUNCHER_RESOURCE_WRITER',fixtureSource,'-o',writer]);
for(const [name,flags] of [['launcher-gui.exe',['-municode','-mwindows']],['launcher-console.exe',['-DLAUNCHER_CONSOLE_FIXTURE']]]){
 const target=path.join(output,name);run(compiler,[...common,...flags,fixtureSource,'-o',target]);run(writer,[target,versionFile]);
}
const build=spawnSync(compiler,[...common,cpp,'-o',exe,'-lole32','-loleaut32','-lruntimeobject','-lshell32','-lshlwapi','-ladvapi32','-luuid','-lxmllite','-lversion'],options);
if(build.status!==0){process.stderr.write((build.stdout||'')+(build.stderr||String(build.error)));process.exit(build.status??1);}
const ran=spawnSync(exe,[],{...options,timeout:60000});fs.writeFileSync(path.join(output,'results.log'),(ran.stdout||'')+(ran.stderr||'')+String(ran.error||''));
process.stdout.write(ran.stdout||'');process.stderr.write(ran.stderr||'');if(ran.error)process.stderr.write(String(ran.error));process.exit(ran.status??1);
