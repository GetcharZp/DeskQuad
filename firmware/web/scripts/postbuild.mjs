import { readdir, readFile, stat, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { gzipSync } from 'node:zlib';

// SPIFFS 的 SPIFFS_OBJ_NAME_LEN 为 32，含开头斜杠后路径最长 31 字符
const MAX_SPIFFS_PATH = 31;
const TEXT_EXTENSIONS = new Set(['.html', '.js', '.css', '.svg', '.json', '.txt']);
const BUDGET = {
  jsRaw: 180 * 1024,
  jsGzip: 60 * 1024,
  cssRaw: 28 * 1024,
  cssGzip: 6 * 1024,
  total: 320 * 1024,
};
const FORBIDDEN_NAMES = new Set(['ssid.json', 'ai_config.json']);
// 版权符号 © 属于 Extended_Pictographic，但它是正常文案，单独放行
const EMOJI_PATTERN = /[\p{Extended_Pictographic}\u{1F1E6}-\u{1F1FF}️]/u;

const distDir = path.resolve('dist');
const failures = [];

function relativeUrl(file) {
  return '/' + path.relative(distDir, file).split(path.sep).join('/');
}

async function walk(dir) {
  const entries = await readdir(dir, { withFileTypes: true });
  const files = [];
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      files.push(...(await walk(full)));
    } else {
      files.push(full);
    }
  }
  return files;
}

async function main() {
  const files = await walk(distDir);
  const sources = files.filter((file) => !file.endsWith('.gz'));
  let totalBytes = 0;
  let jsRaw = 0;
  let jsGzip = 0;
  let cssRaw = 0;
  let cssGzip = 0;

  for (const file of sources) {
    const url = relativeUrl(file);
    const ext = path.extname(file).toLowerCase();
    const info = await stat(file);
    totalBytes += info.size;

    if (url.length > MAX_SPIFFS_PATH) {
      failures.push(`路径超过 ${MAX_SPIFFS_PATH} 字符: ${url}`);
    }
    if (FORBIDDEN_NAMES.has(path.basename(file))) {
      failures.push(`产物中不允许出现敏感文件: ${url}`);
    }
    if (ext === '.map') {
      failures.push(`产物中不允许出现 source map: ${url}`);
    }

    const content = await readFile(file);
    if (TEXT_EXTENSIONS.has(ext)) {
      const text = content.toString('utf8');
      const match = text.match(EMOJI_PATTERN);
      if (match && match[0] !== '©') {
        failures.push(`产物中仍有 emoji: ${url} -> ${match[0]}`);
      }

      const gzPath = file + '.gz';
      const gzUrl = relativeUrl(gzPath);
      if (gzUrl.length > MAX_SPIFFS_PATH) {
        failures.push(`gzip 路径超过 ${MAX_SPIFFS_PATH} 字符: ${gzUrl}`);
        continue;
      }
      const gz = gzipSync(content, { level: 9 });
      if (gz.length < 2 || gz[0] !== 0x1f || gz[1] !== 0x8b) {
        failures.push(`gzip 文件头异常: ${gzUrl}`);
        continue;
      }
      await writeFile(gzPath, gz);
      totalBytes += gz.length;

      if (ext === '.js') {
        jsRaw += info.size;
        jsGzip += gz.length;
      }
      if (ext === '.css') {
        cssRaw += info.size;
        cssGzip += gz.length;
      }
    }
  }

  const html = await readFile(path.join(distDir, 'index.html'), 'utf8');
  const references = [...html.matchAll(/(?:src|href)="(\/assets\/[^"]+)"/g)].map((m) => m[1]);
  if (references.length === 0) {
    failures.push('index.html 没有引用任何 /assets/ 资源');
  }
  for (const ref of references) {
    const target = path.join(distDir, ...ref.split('/').filter(Boolean));
    try {
      await stat(target);
    } catch {
      failures.push(`index.html 引用的资源不存在: ${ref}`);
    }
  }

  if (jsRaw > BUDGET.jsRaw) failures.push(`JS 原始体积超预算: ${jsRaw} > ${BUDGET.jsRaw}`);
  if (jsGzip > BUDGET.jsGzip) failures.push(`JS gzip 体积超预算: ${jsGzip} > ${BUDGET.jsGzip}`);
  if (cssRaw > BUDGET.cssRaw) failures.push(`CSS 原始体积超预算: ${cssRaw} > ${BUDGET.cssRaw}`);
  if (cssGzip > BUDGET.cssGzip) failures.push(`CSS gzip 体积超预算: ${cssGzip} > ${BUDGET.cssGzip}`);
  if (totalBytes > BUDGET.total) failures.push(`dist 总体积超预算: ${totalBytes} > ${BUDGET.total}`);

  const kb = (bytes) => (bytes / 1024).toFixed(1) + ' KB';
  console.log(`JS  ${kb(jsRaw)} raw / ${kb(jsGzip)} gzip`);
  console.log(`CSS ${kb(cssRaw)} raw / ${kb(cssGzip)} gzip`);
  console.log(`dist 总计 ${kb(totalBytes)}（含 .gz）`);

  if (failures.length > 0) {
    console.error('\n产物校验失败：');
    for (const failure of failures) console.error(`- ${failure}`);
    process.exit(1);
  }
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
