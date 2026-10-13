import {build} from 'esbuild';
import {readFile,mkdir,writeFile} from 'node:fs/promises';
const names=['hello','landing','expired','card','admin'];const pages={};for(const name of names)pages[name]=await readFile('public/'+name+'.html','utf8');pages.favicon=await readFile('public/favicon.svg','utf8');pages.glyphs=JSON.parse(await readFile('src/glyphs.json','utf8'));
await writeFile('src/pages.js','// Generated from public/ by scripts/build.mjs\nexport default '+JSON.stringify(pages)+';\n');
await mkdir('dist/server',{recursive:true});await build({entryPoints:['src/worker.js'],outfile:'dist/server/index.js',bundle:true,format:'esm',platform:'browser',target:'es2022'});
