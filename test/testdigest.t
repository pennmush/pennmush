login mortal
run tests:
test('digest.1', $mortal, 'think digest(md5,foo)', 'acbd18db4cc2f85cedef654fccc4a4d8');
# No longer supported on recent OpenSSL
test('digest.2', $mortal, 'think digest(sha,foo)', '752678a483e77799a3651face01d064f9ca86779|UNSUPPORTED DIGEST');
test('digest.3', $mortal, 'think digest(sha1,foo)', '0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33');
# Also no longer supported
test('digest.4', $mortal, 'think digest(dss1,foo)', '0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33|UNSUPPORTED DIGEST');
test('digest.5', $mortal, 'think digest(ripemd160,foo)', '42cfa211018ea492fdee45ac637b7972a0ad6873');
test('digest.6', $mortal, 'think digest(md4,foo)', '0ac6700c491d70fb8650940b1ca1e4b2');
test('digest.7', $mortal, 'think digest(mdc2, foo)','5da2a8f36bf237c84fddf81b67bd0afc|UNSUPPORTED DIGEST');
test('digest.8', $mortal, 'think digest(sha224, foo)', '0808f64e60d58979fcb676c96ec938270dea42445aeefcd3a4e6f8db|UNSUPPORTED DIGEST');
test('digest.9', $mortal, 'think digest(sha512, foo)', 'f7fbba6e0636f890e56fbbf3283e524c6fa3204ae298382d624741d0dc6638326e282c41be5e4254d8820772c5518a2c5a8c0c7f7eda19594a7eb539453e1ed7|UNSUPPORTED DIGEST');
test('digest.10', $mortal, 'think digest(whirlpool, foo)', 'b1b2ee48991281a439da2b8b406d579a9b9878d94bd1de813de8124a1df79d9dd603c728068aff13d724ef55e0a24313a7b84b0bf638682facb5f71fef96701f|UNSUPPORTED DIGEST');

test('base64.1', $mortal, 'think encode64(test string)', 'dGVzdCBzdHJpbmc=');
test("base64.2", $mortal, "think decode64(encode64(this is another fine mess you've gotten us into))", "this is another fine mess you've gotten us into");
