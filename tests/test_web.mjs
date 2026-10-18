import assert from 'node:assert/strict';
import { createHmac, pbkdf2Sync } from 'node:crypto';
import { request, property, response, fields, frames, Assembler, concat, bytes, uint, fixed, mac, derive, hex, unhex } from '../web/dist/protocol.mjs';
assert.equal(hex(request(3,4)), '0d030000002200');
assert.equal(hex(request(1,2,property(1,0))), '0d0100000012060a040a020800');
assert.deepEqual(fields(property(1,0)), [[1, new Uint8Array([10,2,8,0])]]);
for (const n of [1,16,17,263,512]) {
  const data = Uint8Array.from({ length:n }, (_,i)=>i&255), a = new Assembler(); let result;
  for(let i=0;i<Math.ceil(n/16);i++) result=a.push(concat([0xa1,5,i,Math.ceil(n/16)],data.slice(i*16,i*16+16)));
  assert.deepEqual(result,data);
}
assert.throws(()=>new Assembler().push(new Uint8Array([0xa1,1,1,2,5])));
const a=new Assembler();a.push(concat([0xa1,1,0,2],new Uint8Array(16)));
assert.throws(()=>a.push(concat([0xa1,1,0,2],new Uint8Array(16))));
assert.throws(()=>fields(new Uint8Array([10,255])));
assert.throws(()=>frames(new Uint8Array(129),1));
assert.equal(frames(new Uint8Array(128),1).length,8);
const decoded = response(concat(fixed(1,3),bytes(4,concat(bytes(1,bytes(2,bytes(1,uint(1,0)))),bytes(2,concat(uint(1,1),fixed(3,123)))))));
assert.equal(decoded.id,3);assert.equal(decoded.properties[0].value,0);assert.equal(decoded.settings.revision,123);
const salt=new Uint8Array(16).fill(0x42), nonce=new Uint8Array(32).fill(7);
const key=await derive('мой тестовый пароль',salt,600000);
assert.equal(hex(key),pbkdf2Sync('мой тестовый пароль',salt,600000,32,'sha256').toString('hex'));
assert.deepEqual(unhex(hex(key)),key);
const signature=await mac(key,'SV-AUTH-v1',nonce);
assert.equal(hex(signature),createHmac('sha256',key).update('SV-AUTH-v1').update(nonce).digest('hex'));
// Export JS-produced protobuf and crypto fixtures for nanopb + TinyCrypt cross-check.
const proof=request(17,8,bytes(1,signature));
const inner=request(18,4),seq=new Uint8Array([1,0,0,0]);
const cmdMac=await mac(key,'SV-CMD-v1',nonce,seq,inner);
const secure=request(18,10,concat(bytes(1,inner),bytes(2,cmdMac),fixed(3,1)));
if(process.argv[2]) {
  const {writeFileSync}=await import('node:fs');
  writeFileSync(process.argv[2],JSON.stringify({key:hex(key),nonce:hex(nonce),proof:hex(proof),secure:hex(secure)}));
}
console.log('Web codec, fragmentation, PBKDF2 and HMAC: passed');
