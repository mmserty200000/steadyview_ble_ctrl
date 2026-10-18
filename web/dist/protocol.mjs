// Wire format for Protocol/steadyview.proto. No network dependencies.
export const SERVICE = 'bad8dbcb-268e-4df5-aeef-f3bd1961161a';
export const RX = 'bad8dbcb-268e-4df5-aeef-f3bd1961161b';
export const TX = 'bad8dbcb-268e-4df5-aeef-f3bd1961161c';
export const concat = (...arrays) => Uint8Array.from(arrays.flatMap(a => Array.from(a)));
export function varint(n) {
  if (!Number.isInteger(n) || n < 0 || n > 0xffffffff) throw Error('Invalid uint32');
  const out = []; do { const v = n % 128; n = Math.floor(n / 128); out.push(v | (n ? 128 : 0)); } while (n);
  return Uint8Array.from(out);
}
export const bytes = (tag, data) => concat(varint(tag * 8 + 2), varint(data.length), data);
export const uint = (tag, value) => concat(varint(tag * 8), varint(value));
export function le32(n) { const b = new Uint8Array(4); new DataView(b.buffer).setUint32(0, n, true); return b; }
export const fixed = (tag, n) => concat(varint(tag * 8 + 5), le32(n));
export const request = (id, tag, payload = []) => concat(fixed(1, id), bytes(tag, payload));
export const property = (tag, value) => bytes(1, bytes(tag, value === undefined ? [] : uint(1, value)));
export function fields(data) {
  let pos = 0; const out = [];
  const read = () => {
    let n = 0;
    for (let i = 0; i < 5; i++) {
      if (pos >= data.length) throw Error('Оборванный protobuf');
      const b = data[pos++]; n += (b & 127) * 2 ** (7 * i);
      if (!(b & 128)) { if (n > 0xffffffff) throw Error('Переполнение protobuf'); return n; }
    }
    throw Error('Неверный varint');
  };
  while (pos < data.length) {
    const key = read(), tag = Math.floor(key / 8), wire = key & 7;
    if (!tag) throw Error('Неверное поле protobuf');
    let value;
    if (wire === 0) value = read();
    else if (wire === 2) { const n = read(); if (pos + n > data.length) throw Error('Оборванный protobuf'); value = data.slice(pos, pos += n); }
    else if (wire === 5) { if (pos + 4 > data.length) throw Error('Оборванный protobuf'); value = new DataView(data.buffer, data.byteOffset + pos, 4).getUint32(0, true); pos += 4; }
    else throw Error('Неподдерживаемое поле protobuf');
    out.push([tag, value]);
  }
  return out;
}
const object = data => Object.fromEntries(fields(data));
const settings = data => { const f = object(data); return { saved: !!f[1], dirty: !!f[2], revision: f[3] || 0, restoreStatus: f[4] || 0 }; };
function decodeProperty(data) {
  const f = object(data), choice = f[2] ? fields(f[2])[0] : null;
  return { status: f[1] || 0, tag: choice?.[0], value: choice ? object(choice[1])[1] : undefined };
}
export function response(data) {
  const f = object(data), result = { id: f[1] || 0, status: f[2] || 0 };
  if (f[3]) result.property = decodeProperty(f[3]);
  if (f[4]) {
    const state = fields(f[4]); result.properties = state.filter(([t]) => t === 1).map(([, d]) => decodeProperty(d));
    const info = state.find(([t]) => t === 2); if (info) result.settings = settings(info[1]);
  }
  if (f[5]) result.settings = settings(f[5]);
  if (f[6]) { const a = object(f[6]); result.auth = { configured: !!a[1], authorized: !!a[2], salt: a[3], challenge: a[4], iterations: a[5] }; }
  return result;
}
export class Assembler {
  reset() { this.parts = []; this.count = 0; this.id = 0; }
  constructor() { this.reset(); }
  push(frame) {
    if (frame.length < 5 || frame.length > 20 || frame[0] !== 0xa1 || !frame[3] || frame[3] > 32) throw Error('Неверный BLE-фрагмент');
    if (frame[2] === 0) { if (this.count) throw Error('Пропущен BLE-фрагмент'); this.id = frame[1]; this.count = frame[3]; }
    if (!this.count || this.id !== frame[1] || this.count !== frame[3] || this.parts.length !== frame[2] || frame[2] >= this.count || (frame[2] + 1 < this.count && frame.length !== 20)) throw Error('Нарушен порядок BLE-фрагментов');
    this.parts.push(frame.slice(4));
    if (this.parts.length !== this.count) return null;
    const data = concat(...this.parts); this.reset(); return data;
  }
}
export function frames(data, id) {
  if (!data.length || data.length > 128) throw Error('Слишком большой запрос');
  if (data.length <= 20) return [data];
  const count = Math.ceil(data.length / 16);
  return Array.from({ length: count }, (_, i) => concat([0xa2, id & 255, i, count], data.slice(i * 16, i * 16 + 16)));
}
const utf8 = s => new TextEncoder().encode(s);
export async function derive(passphrase, salt, iterations) {
  if (salt?.length !== 16 || iterations !== 600000) throw Error('Неподдерживаемые параметры авторизации');
  const k = await crypto.subtle.importKey('raw', utf8(passphrase), 'PBKDF2', false, ['deriveBits']);
  return new Uint8Array(await crypto.subtle.deriveBits({ name: 'PBKDF2', hash: 'SHA-256', salt, iterations }, k, 256));
}
export async function mac(key, domain, challenge, ...rest) {
  if (key.length !== 32 || challenge?.length !== 32) throw Error('Неверный ключ или challenge');
  const k = await crypto.subtle.importKey('raw', key, { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  return new Uint8Array(await crypto.subtle.sign('HMAC', k, concat(utf8(domain), challenge, ...rest)));
}
export const hex = b => Array.from(b, v => v.toString(16).padStart(2, '0')).join('');
export function unhex(s) { if (!/^[a-f0-9]{64}$/.test(s)) throw Error('Неверный сохранённый ключ'); return Uint8Array.from(s.match(/../g), v => parseInt(v, 16)); }
export const errors = ['OK', 'Неверный пакет', 'Неизвестное свойство', 'Недопустимое значение', 'Только чтение', 'Ошибка SPI / RNG', 'Нет достоверного состояния', 'Ошибка записи flash', 'Настройки ещё не сохранены', 'Требуется вход', 'Неверная пассфраза или подпись'];
