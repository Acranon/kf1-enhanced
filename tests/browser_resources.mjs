// Run in nix develop: node tests/browser_resources.mjs BUILD_DIRECTORY JAPANESE_BIN
// Uses an isolated Chromium profile and local HTTP server; uploads nothing.
import {spawn} from 'node:child_process';
import {createServer} from 'node:http';
import {mkdtemp, readFile, rm, writeFile} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {resolve, join, extname, sep} from 'node:path';

const [buildArg, discArg, screenshotArg] = process.argv.slice(2);
if (!buildArg || !discArg) throw Error('Expected build directory and Japanese BIN/ISO');
const build = resolve(buildArg), profile = await mkdtemp(join(tmpdir(), 'kf-browser-'));
const server = createServer(async (request, response) => {
  try {
    const path = resolve(build, '.' + new URL(request.url, 'http://localhost').pathname);
    if (!path.startsWith(build + sep)) throw Error('Invalid path');
    const bytes = await readFile(path);
    response.setHeader('Content-Type', ({'.html':'text/html', '.js':'text/javascript',
      '.wasm':'application/wasm'})[extname(path)] || 'application/octet-stream');
    response.end(bytes);
  } catch { response.writeHead(404); response.end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const url = `http://127.0.0.1:${server.address().port}/kings-field.html`;
const browser = spawn('chromium', ['--headless', '--no-sandbox', '--disable-dev-shm-usage',
  '--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader',
  '--remote-debugging-port=0', `--user-data-dir=${profile}`, 'about:blank'],
  {stdio:['ignore', 'ignore', 'pipe']});
let browserLog = '';
browser.stderr.on('data', data => { browserLog = (browserLog + data).slice(-16000); });
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
let socket;
try {
  let port;
  for (let attempt = 0; attempt < 100; ++attempt) {
    try { port = (await readFile(join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]; break; }
    catch { await sleep(100); }
  }
  if (!port) throw Error('Chromium did not start: ' + browserLog);
  const pages = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
  socket = new WebSocket(pages.find(page => page.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { socket.onopen = resolve; socket.onerror = reject; });
  let sequence = 0;
  const pending = new Map();
  socket.onmessage = event => {
    const message = JSON.parse(event.data), callback = pending.get(message.id);
    if (callback) { pending.delete(message.id); callback(message); }
  };
  function cdp(method, params = {}) {
    return new Promise((resolve, reject) => {
      const id = ++sequence;
      const timeout = setTimeout(() => { pending.delete(id); reject(Error(`CDP timeout: ${method}`)); }, 60000);
      pending.set(id, message => {
        clearTimeout(timeout);
        message.error ? reject(Error(JSON.stringify(message.error))) : resolve(message.result);
      });
      socket.send(JSON.stringify({id, method, params}));
    });
  }
  async function evaluate(expression) {
    const result = await cdp('Runtime.evaluate', {expression, returnByValue:true, awaitPromise:true});
    if (result.exceptionDetails) throw Error(JSON.stringify(result.exceptionDetails));
    return result.result.value;
  }
  async function until(expression) {
    for (let attempt = 0; attempt < 600; ++attempt) {
      if (await evaluate(expression)) return;
      await sleep(100);
    }
    throw Error('Timed out: ' + await evaluate('document.body.innerText'));
  }
  async function select(language) {
    await evaluate(`languageInput.value=${JSON.stringify(language)}; languageInput.dispatchEvent(new Event('change'));`);
    await until('!discInput.disabled && dataRoot !== null');
    if (!await evaluate('verifyFiles(collectedFiles(dataRoot))')) throw Error('Wrong resource hash');
  }
  async function key(key, code, number) {
    await cdp('Input.dispatchKeyEvent', {type:'keyDown', key, code, windowsVirtualKeyCode:number});
    await sleep(150);
    await cdp('Input.dispatchKeyEvent', {type:'keyUp', key, code, windowsVirtualKeyCode:number});
    await sleep(200);
  }
  async function liveSelect(language) {
    await evaluate(`languageInput.value=${JSON.stringify(language)}; languageInput.dispatchEvent(new Event('change'));`);
    await until(`Module.ccall('kf_current_language', 'string', [], []) === ${JSON.stringify(language)} || stopped`);
    if (await evaluate('stopped')) throw Error(await evaluate('statusLabel.textContent'));
  }
  async function capture(path) {
    await sleep(1000);
    const screenshot = await cdp('Page.captureScreenshot', {format:'png'});
    await writeFile(path, Buffer.from(screenshot.data, 'base64'));
  }
  await cdp('Page.navigate', {url});
  await until("document.getElementById('disc') && !document.getElementById('disc').disabled");
  if (await evaluate('languageInput.value') !== 'en') throw Error('English is not the default');
  const document = await cdp('DOM.getDocument');
  const input = await cdp('DOM.querySelector', {nodeId:document.root.nodeId, selector:'#disc'});
  await cdp('DOM.setFileInputFiles', {nodeId:input.nodeId, files:[resolve(discArg)]});
  await until('!discInput.disabled && dataRoot !== null');
  if (!await evaluate('verifyFiles(collectedFiles(dataRoot))')) throw Error('English import failed hash');
  console.log('Browser: Japanese disc imported and English prepared by default.');
  await select('ja');
  await select('en');
  console.log('Browser: Japanese and English restore without another disc selection.');
  await select('ja');
  await select('en');
  console.log('Browser: both language caches restore and verify.');
  await cdp('Page.reload');
  await until("document.getElementById('disc') && !document.getElementById('disc').disabled");
  await select('en');
  console.log('Browser: English cache survives a page reload.');
  await evaluate('playButton.click();');
  await until("(running && !languageInput.disabled) || stopped");
  if (await evaluate('stopped')) throw Error(await evaluate('statusLabel.textContent'));
  await sleep(3000);
  if (await evaluate('stopped')) throw Error(await evaluate('statusLabel.textContent'));
  await evaluate('Module.canvas.focus();');
  await key('Tab', 'Tab', 9);
  if (await evaluate("Module.ccall('kf_current_language', 'string', [], [])") !== 'en')
    throw Error('Game did not start in English');
  await liveSelect('ja');
  await liveSelect('en');
  await liveSelect('ja');
  await liveSelect('en');
  await liveSelect('ja');
  console.log('Browser: English startup and repeated language switches work without restarting.');
  await evaluate('Module.canvas.focus();');
  await key('Tab', 'Tab', 9);
  await evaluate("languageInput.value='en'; languageInput.dispatchEvent(new Event('change'));");
  await sleep(250);
  if (await evaluate("Module.ccall('kf_current_language', 'string', [], [])") !== 'ja')
    throw Error('Language switched while the root menu was active');
  await key('ArrowUp', 'ArrowUp', 38);
  await key('ArrowUp', 'ArrowUp', 38);
  await key('Enter', 'Enter', 13);
  await until("Module.ccall('kf_current_language', 'string', [], []) === 'en' || stopped");
  for (let row = 0; row < 4; ++row) await key('ArrowDown', 'ArrowDown', 40);
  await key('ArrowRight', 'ArrowRight', 39);
  await until("Module.ccall('kf_current_language', 'string', [], []) === 'ja' || stopped");
  if (screenshotArg) await capture(screenshotArg.replace(/\.png$/, '') + '-ja.png');
  await key('Enter', 'Enter', 13);
  await until("Module.ccall('kf_current_language', 'string', [], []) === 'en' || stopped");
  if (await evaluate('stopped')) throw Error(await evaluate('statusLabel.textContent'));
  console.log('Browser: Configuration language row switches both ways; queued changes wait for a safe menu.');
  if (screenshotArg) {
    await capture(screenshotArg);
    console.log('Browser: captured both Configuration languages for visual review.');
  }
} finally {
  socket?.close();
  browser.kill('SIGTERM');
  await new Promise(resolve => browser.exitCode !== null ? resolve() : browser.once('exit', resolve));
  await new Promise(resolve => server.close(resolve));
  await rm(profile, {recursive:true, force:true, maxRetries:5, retryDelay:100});
}
