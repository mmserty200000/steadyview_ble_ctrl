import assert from 'node:assert/strict';
import { createHmac, randomBytes } from 'node:crypto';
import { Receiver } from '../web/dist/ble.mjs';
import { RX, TX, fields, concat, bytes, fixed, uint, property, derive } from '../web/dist/protocol.mjs';
const obj = b => Object.fromEntries(fields(b));
class Characteristic extends EventTarget {
  async startNotifications() { return this; }
  async writeValueWithResponse(frame) { return peripheral.write(frame); }
}
const peripheral = {
  tx: new Characteristic(), rx: new Characteristic(), salt: new Uint8Array(16).fill(0x42), key: null,
  fragments: [], channel: 0, sequence: 0, transfer: 0,
  signature(domain, ...data) { const h=createHmac('sha256',this.key).update(domain).update(this.challenge); for(const b of data)h.update(b);return new Uint8Array(h.digest()); },
  auth() { return concat(uint(1,this.key?1:0),uint(2,this.authorized?1:0),bytes(3,this.salt),bytes(4,this.challenge||[]),uint(5,600000)); },
  async write(frame) {
    let packet=frame;
    if(frame[0]===0xa2) {
      if(frame[2]===0)this.fragments=[];
      assert.equal(frame[2],this.fragments.length);this.fragments.push(frame.slice(4));
      if(this.fragments.length<frame[3])return;
      packet=concat(...this.fragments);this.fragments=[];
    }
    const outer=obj(packet), id=outer[1];let command=Object.entries(outer).find(([t])=>t!=='1');
    let tag=Number(command[0]), payload=command[1], status=0, result;
    if(tag===10) {
      const cmd=obj(payload),seq=new Uint8Array(4);new DataView(seq.buffer).setUint32(0,cmd[3],true);
      if(!this.authorized)status=9;
      else {assert.equal(cmd[3],++this.sequence);assert.deepEqual(cmd[2],this.signature('SV-CMD-v1',seq,cmd[1]));
        const inner=obj(cmd[1]);assert.equal(inner[1],id);command=Object.entries(inner).find(([t])=>t!=='1');tag=Number(command[0]);payload=command[1];}
    } else if(this.key && ![7,8].includes(tag))status=9;
    if(!status) {
      if(tag===7) {this.challenge=new Uint8Array(randomBytes(32));this.authorized=false;this.sequence=0;result=bytes(6,this.auth());}
      else if(tag===8) {this.authorized=!!this.key && Buffer.from(obj(payload)[1]).equals(Buffer.from(this.signature('SV-AUTH-v1')));status=this.authorized?0:10;result=bytes(6,this.auth());}
      else if(tag===9) {let key=obj(payload)[1];if(this.key){const mask=this.signature('SV-KEY-v1');key=key.map((v,i)=>v^mask[i]);}this.key=key;this.authorized=false;result=bytes(6,this.auth());}
      else if(tag===2) {const prop=obj(obj(payload)[1]);this.channel=obj(prop[1])[1];result=bytes(3,bytes(2,bytes(1,uint(1,this.channel))));}
      else if(tag===4)result=bytes(4,bytes(1,bytes(2,bytes(1,uint(1,this.channel)))));
      else throw Error('Unexpected command');
    }
    const reply=concat(fixed(1,id),uint(2,status),result||[]), count=Math.ceil(reply.length/16);
    ++this.transfer;
    for(let i=0;i<count;i++){
      const frame=concat([0xa1,this.transfer&255,i,count],reply.slice(i*16,i*16+16));
      this.tx.value=new DataView(frame.buffer);this.tx.dispatchEvent(new Event('characteristicvaluechanged'));
    }
  }
};
class Device extends EventTarget {
  constructor(){super();this.id='fake-test-device';this.gatt={connected:false,
    connect:async()=>{this.gatt.connected=true;return {getPrimaryService:async()=>({getCharacteristic:async uuid=>uuid===RX?peripheral.rx:uuid===TX?peripheral.tx:null})};},
    disconnect:()=>{this.gatt.connected=false;peripheral.authorized=false;this.dispatchEvent(new Event('gattserverdisconnected'));}};}
}
const device=new Device(), client=new Receiver(()=>{});
await client.connect(device);assert.equal((await client.getAuth()).configured,false);
const key=await derive('a long test passphrase',client.auth.salt,600000);
await client.setPassphrase(key);assert.equal(client.auth.authorized,true);
await client.rpc(2,property(1,12));assert.equal(peripheral.channel,12);
assert.equal((await client.rpc(4)).properties[0].value,12);
await assert.rejects(client.rpc(4,[],false),/Требуется вход/);
const next=await derive('a different long passphrase',client.auth.salt,600000);
await client.setPassphrase(next);assert.deepEqual(peripheral.key,next);
client.disconnect();await client.connect(device);await client.getAuth();
await assert.rejects(client.login(key),/Неверная пассфраза/);
await client.getAuth();await client.login(next);assert.equal((await client.rpc(4)).properties[0].value,12);
client.disconnect();
console.log('Web BLE client: provisioning, framing, signed commands, rotation and reconnect passed');
