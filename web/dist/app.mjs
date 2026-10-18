import { SERVICE, property, derive, hex, unhex, errors } from './protocol.mjs';
import { Receiver } from './ble.mjs';
const $ = id => document.getElementById(id);
const frequencies = [5865,5845,5825,5805,5785,5765,5745,5725,5733,5752,5771,5790,5809,5828,5847,5866,5705,5685,5665,5645,5885,5905,5925,5945,5740,5760,5780,5800,5820,5840,5860,5880,5658,5695,5732,5769,5806,5843,5880,5917,5333,5373,5413,5453,5493,5533,5573,5613];
const bands = ['A','B','E','F','R','L'];
const registerNames = ['Synthesizer A','Synthesizer B','Synthesizer C','Synthesizer D','VCO switch cap','DFC control','6M audio demodulator','6M5 audio demodulator','Receiver control 1','Receiver control 2','Power down','State'];
let connected = false, authorized = false, busy = false, changing = false, selectedBand = 0, currentChannel;
const storage = {
  get(k) { try { return localStorage.getItem(k); } catch { return null; } },
  set(k, v) { try { localStorage.setItem(k, v); return true; } catch { return false; } },
  remove(k) { try { localStorage.removeItem(k); } catch { /* unavailable storage */ } }
};
const keyName = () => `steadyview.key.v1.${receiver.device.id}`;
const note = (text, error = false) => { $('notice').textContent = text; $('notice').classList.toggle('error', error); };
function updateUI() {
  $('controls').disabled = !authorized || busy;
  $('connect').hidden = connected; $('connect').disabled = busy;
  $('reconnect').hidden = connected || !storage.get('steadyview.lastDevice') || !navigator.bluetooth?.getDevices;
  $('reconnect').disabled = busy;
  $('disconnect').hidden = !connected;
  $('auth-card').hidden = !connected || (authorized && !changing);
  for (const el of $('auth-form').elements) el.disabled = busy;
  $('connection').textContent = connected ? (authorized ? '● Доступ открыт' : '● Требуется вход') : '● Не подключён';
  $('connection').classList.toggle('online', connected);
}
const receiver = new Receiver(() => {
  connected = false; authorized = false; changing = false;
  $('frequency').textContent = '—'; $('channel-label').textContent = '—';
  $('frequency-hint').textContent = 'Состояние недоступно без подключения';
  currentChannel = undefined; renderChannels();
  for (const id of ['mix', 'diversity']) $(id).setAttribute('aria-pressed', 'false');
  $('saved-title').textContent = 'Нет соединения'; $('saved-info').textContent = 'Подключись, чтобы прочитать память';
  $('saved-led').classList.remove('saved');
  for (const input of $('registers').querySelectorAll('input')) input.value = '';
  $('device-hint').textContent = 'Соединение закрыто. Можно подключиться снова.';
  $('passphrase').value = ''; $('confirm').value = ''; updateUI();
});
async function action(fn) {
  if (busy) return;
  busy = true; updateUI();
  try { await fn(); }
  catch (error) {
    let message = error.name === 'NotFoundError' ? 'Приёмник не выбран. Убедись, что он включён и свободен.' : error.message;
    if (error.reply?.property?.status === 0) message += '. Значение применено, но не сохранено. Нажми «Сохранить».';
    if (connected && !receiver.auth) receiver.disconnect();
    else if (connected) {
      authorized = !!receiver.auth.authorized;
      authForm();
    }
    note(message, true);
  } finally { busy = false; updateUI(); }
}
function authForm() {
  const first = !receiver.auth.configured;
  $('auth-title').textContent = changing ? 'Новая пассфраза' : first ? 'Задай пассфразу' : 'Вход по пассфразе';
  $('auth-hint').textContent = changing ? 'После смены другим устройствам потребуется новая пассфраза.' : first ? 'Этот приёмник ещё не защищён. Первую пассфразу может задать любой подключившийся клиент. Выполни настройку в доверенном месте.' : 'Введи пассфразу этого приёмника. Сохранённый ключ позволит входить без повторного ввода.';
  $('confirm-row').hidden = !(first || changing);
  $('confirm').required = first || changing;
  $('passphrase').autocomplete = first || changing ? 'new-password' : 'current-password';
  $('passphrase').minLength = first || changing ? 12 : 1;
  $('unlock').textContent = first || changing ? 'Сохранить пассфразу' : 'Войти';
}
function remember(key) {
  if (!$('remember').checked) { storage.remove(keyName()); return true; }
  return storage.set(keyName(), JSON.stringify({ key: hex(key), salt: hex(receiver.auth.salt) }));
}
async function attach(device) {
  note('Подключение к приёмнику…');
  await receiver.connect(device); connected = true; storage.set('steadyview.lastDevice', device.id);
  $('device-name').textContent = device.name || 'SteadyView'; $('device-hint').textContent = 'BLE-сервис найден. Соединение установлено.';
  await receiver.getAuth(); authForm();
  const saved = storage.get(keyName());
  if (receiver.auth.configured && saved) {
    try {
      const record = JSON.parse(saved);
      if (record.salt !== hex(receiver.auth.salt)) throw Error('Изменилась идентификация приёмника');
      await receiver.login(unhex(record.key)); authorized = true; await refresh();
      note('Подключено. Вход выполнен с сохранённым ключом.'); return;
    } catch {
      storage.remove(keyName());
      if (!device.gatt.connected) throw Error('Соединение потеряно при входе');
      await receiver.getAuth(); authForm();
      note('Сохранённый ключ не подошёл. Введи пассфразу заново.', true); return;
    }
  }
  note(receiver.auth.configured ? 'Соединение установлено. Введи пассфразу.' : 'Задай первую пассфразу для защиты приёмника.');
}
$('connect').onclick = () => action(async () => {
  if (!navigator.bluetooth || !globalThis.isSecureContext) throw Error('Нужен браузер с Web Bluetooth и адрес HTTPS или localhost.');
  const device = await navigator.bluetooth.requestDevice({ filters: [{ services: [SERVICE] }] });
  await attach(device);
});
$('reconnect').onclick = () => action(async () => {
  const devices = await navigator.bluetooth.getDevices();
  const device = devices.find(d => d.id === storage.get('steadyview.lastDevice'));
  if (!device) { storage.remove('steadyview.lastDevice'); throw Error('Разрешение не сохранено. Нажми «Найти приёмник».'); }
  await attach(device);
});
$('disconnect').onclick = () => { receiver.disconnect(); note('Приёмник отключён.'); };
$('auth-form').onsubmit = event => {
  event.preventDefault();
  action(async () => {
    const passphrase = $('passphrase').value;
    if ((!receiver.auth.configured || changing) && (passphrase.length < 12 || !passphrase.trim())) throw Error('Для новой пассфразы нужно не менее 12 символов.');
    if ((!receiver.auth.configured || changing) && passphrase !== $('confirm').value) throw Error('Пассфразы не совпадают.');
    const key = await derive(passphrase, receiver.auth.salt, receiver.auth.iterations);
    // Refresh the single-use challenge for each login attempt; retain it for a signed rotation.
    if (!receiver.auth.configured || changing) await receiver.setPassphrase(key);
    else { await receiver.getAuth(); await receiver.login(key); }
    authorized = true; changing = false;
    const stored = remember(key);
    $('passphrase').value = ''; $('confirm').value = '';
    await refresh();
    note(stored ? 'Доступ открыт. Изменения настроек сохраняются в приёмнике автоматически.' : 'Доступ открыт, но браузер запретил сохранение ключа. В следующий раз потребуется пассфраза.');
  });
};
function renderChannels() {
  for (const [i, button] of Array.from($('bands').children).entries()) button.setAttribute('aria-pressed', String(i === selectedBand));
  $('channels').replaceChildren();
  for (let i = 0; i < 8; i++) {
    const index = selectedBand * 8 + i, button = document.createElement('button');
    button.setAttribute('aria-pressed', String(index === currentChannel));
    button.setAttribute('aria-label', `${bands[selectedBand]}${i + 1}, ${frequencies[index]} MHz`);
    button.append(document.createTextNode(String(i + 1))); const caption = document.createElement('small'); caption.textContent = frequencies[index]; button.append(caption);
    button.onclick = () => action(() => setProperty(1, index)); $('channels').append(button);
  }
}
for (const [i, band] of bands.entries()) {
  const b = document.createElement('button'); b.textContent = band; b.onclick = () => { selectedBand = i; renderChannels(); }; $('bands').append(b);
}
for (const [i, name] of registerNames.entries()) {
  const tag = i === 11 ? 31 : i + 16, row = document.createElement('tr');
  const label = document.createElement('td'); label.textContent = `${i === 11 ? '0F' : i.toString(16).toUpperCase().padStart(2, '0')} · ${name}`;
  const cell = document.createElement('td'), input = document.createElement('input');
  input.id = `reg-${tag}`; input.setAttribute('aria-label', name + ', hex'); input.maxLength = 5; input.placeholder = '—'; input.spellcheck = false; input.readOnly = tag === 31; cell.append(input);
  const control = document.createElement('td');
  if (tag !== 31) {
    const button = document.createElement('button'); button.textContent = 'Записать';
    button.onclick = () => action(async () => { if (!/^[0-9a-f]{1,5}$/i.test(input.value)) throw Error('Регистр: от 00000 до FFFFF в hex.'); await setProperty(tag, parseInt(input.value, 16)); }); control.append(button);
  } else control.textContent = 'Только чтение';
  row.append(label, cell, control); $('registers').append(row);
}
function applyState(reply) {
  const props = new Map(reply.properties.map(p => [p.tag, p]));
  const value = tag => props.get(tag)?.status === 0 ? props.get(tag).value : undefined;
  currentChannel = value(1); if (currentChannel !== undefined) selectedBand = Math.floor(currentChannel / 8);
  $('frequency').textContent = value(3) ?? '—';
  $('channel-label').textContent = currentChannel === undefined ? 'RAW' : `${bands[selectedBand]}${currentChannel % 8 + 1}`;
  $('frequency-hint').textContent = currentChannel === undefined ? 'Канал неизвестен: выбери канал после прямой записи регистров' : `Диапазон ${bands[selectedBand]} · канал ${currentChannel % 8 + 1}`;
  renderChannels();
  $('mix').setAttribute('aria-pressed', String(value(2) === 0)); $('diversity').setAttribute('aria-pressed', String(value(2) === 1));
  for (let i = 0; i < 12; i++) { const tag = i === 11 ? 31 : i + 16, v = value(tag); $(`reg-${tag}`).value = v === undefined ? '' : v.toString(16).toUpperCase().padStart(5, '0'); }
  const info = reply.settings;
  $('saved-title').textContent = info.dirty ? 'Есть несохранённые изменения' : info.saved ? 'Сохранено в приёмнике' : 'Нет сохранённых настроек';
  $('saved-info').textContent = `Ревизия ${info.revision} · восстановление: ${errors[info.restoreStatus] || info.restoreStatus}`;
  $('saved-led').classList.toggle('saved', info.saved && !info.dirty);
  const failed = reply.properties.filter(p => p.status && !(p.status === 6 && [1,2,3].includes(p.tag)));
  if (failed.length) note(`Не удалось прочитать ${failed.length} свойств: ${errors[failed[0].status]}. Проверь связь с приёмником.`, true);
}
async function refresh() { applyState(await receiver.rpc(4)); }
async function setProperty(tag, value) {
  await receiver.rpc(2, property(tag, value)); note('Настройка применена и сохранена.'); await refresh();
}
$('mix').onclick = () => action(() => setProperty(2, 0)); $('diversity').onclick = () => action(() => setProperty(2, 1));
$('refresh').onclick = () => action(async () => { note('Состояние обновлено.'); await refresh(); });
$('save').onclick = () => action(async () => { await receiver.rpc(5); note('Настройки сохранены во flash.'); await refresh(); });
$('initialize').onclick = () => { if (confirm('Сбросить настройки приёмника на A1 / Mix?')) action(() => setProperty(4, 1)); };
$('change-passphrase').onclick = () => { changing = !changing; authForm(); updateUI(); if (changing) $('passphrase').focus(); };
$('forget').onclick = () => { storage.remove(keyName()); receiver.disconnect(); note('Ключ удалён из этого браузера. Пассфраза приёмника сохранена.'); };
renderChannels(); updateUI();
if (!navigator.bluetooth || !globalThis.isSecureContext || !crypto.subtle) note('Web Bluetooth недоступен. Открой страницу по HTTPS или localhost в поддерживаемом Chrome/Edge. Встроенный WebView может не поддерживать BLE.', true);
