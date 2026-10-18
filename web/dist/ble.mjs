import { SERVICE, RX, TX, request, bytes, fixed, concat, frames, Assembler, response, mac, le32, errors } from './protocol.mjs';
export class Receiver {
  constructor(onDisconnect) { this.onDisconnect = onDisconnect; this.nextId = 0; this.assembler = new Assembler(); }
  async connect(device) {
    this.device = device;
    this.disconnected = () => {
      if (this.tx && this.listener) this.tx.removeEventListener('characteristicvaluechanged', this.listener);
      device.removeEventListener('gattserverdisconnected', this.disconnected);
      this.reset(Error('Приёмник отключён')); this.onDisconnect(); };
    device.addEventListener('gattserverdisconnected', this.disconnected);
    try {
      const server = await device.gatt.connect();
      const service = await server.getPrimaryService(SERVICE);
      this.rx = await service.getCharacteristic(RX); this.tx = await service.getCharacteristic(TX);
      this.listener = event => {
        try {
          const v = event.target.value, data = this.assembler.push(new Uint8Array(v.buffer, v.byteOffset, v.byteLength));
          if (!data) return;
          const reply = response(data);
          if (!this.pending || reply.id !== this.pending.id) throw Error('Неожиданный ответ приёмника');
          const p = this.pending; this.pending = null; clearTimeout(p.timer);
          // Even an error can contain applied property / storage status; retain the reply.
          if (reply.status) { const error = Error(errors[reply.status] || `Ошибка ${reply.status}`); error.reply = reply; p.reject(error); }
          else p.resolve(reply);
        } catch (error) { this.reset(error); this.disconnect(); }
      };
      this.tx.addEventListener('characteristicvaluechanged', this.listener);
      await this.tx.startNotifications();
    } catch (error) { this.disconnect(); throw error; }
  }
  reset(error) {
    if (this.pending) { clearTimeout(this.pending.timer); this.pending.reject(error); this.pending = null; }
    this.assembler.reset(); this.key = null; this.auth = null; this.sequence = 0;
  }
  disconnect() {
    this.reset(Error('Соединение закрыто'));
    if (this.tx && this.listener) this.tx.removeEventListener('characteristicvaluechanged', this.listener);
    this.device?.gatt.disconnect();
    if (this.device && this.disconnected) this.device.removeEventListener('gattserverdisconnected', this.disconnected);
    this.onDisconnect();
  }
  async rpc(tag, payload = [], protect = true) {
    if(this.inflight) throw Error('Предыдущая команда ещё выполняется');
    this.inflight = true;
    try { return await this.send(tag, payload, protect); } finally { this.inflight = false; }
  }
  async send(tag, payload = [], protect = true) {
    if (this.pending) throw Error('Предыдущая команда ещё выполняется');
    const id = ++this.nextId, inner = request(id, tag, payload); let packet = inner;
    if (protect && this.auth?.configured) {
      if (!this.key || !this.auth.authorized) throw Error('Требуется вход');
      const sequence = ++this.sequence;
      if (sequence > 0xffffffff) throw Error('Переподключитесь для обновления сессии');
      const signature = await mac(this.key, 'SV-CMD-v1', this.auth.challenge, le32(sequence), inner);
      packet = request(id, 10, concat(bytes(1, inner), bytes(2, signature), fixed(3, sequence)));
    }
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => { this.reset(Error('Нет ответа. Результат команды неизвестен; подключитесь заново.')); this.disconnect(); }, 10000);
      this.pending = { id, resolve, reject, timer };
      (async () => {
        for (const frame of frames(packet, id)) await this.rx.writeValueWithResponse(frame);
      })().catch(error => { this.reset(error); this.disconnect(); });
    });
  }
  async getAuth() { this.auth = (await this.rpc(7, [], false)).auth; this.sequence = 0; this.key = null; return this.auth; }
  async login(key) {
    const proof = await mac(key, 'SV-AUTH-v1', this.auth.challenge);
    this.auth = (await this.rpc(8, bytes(1, proof), false)).auth;
    if (!this.auth?.authorized) throw Error('Вход не подтверждён');
    this.key = key; this.sequence = 0;
  }
  async setPassphrase(nextKey) {
    let wrapped = nextKey;
    if (this.auth.configured) {
      const mask = await mac(this.key, 'SV-KEY-v1', this.auth.challenge);
      wrapped = nextKey.map((v, i) => v ^ mask[i]);
    }
    await this.rpc(9, bytes(1, wrapped));
    await this.getAuth(); await this.login(nextKey);
  }
}
