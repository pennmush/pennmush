login mortal
run tests:
test('digest.1', $mortal, 'think digest(md5,foo)', 'acbd18db4cc2f85cedef654fccc4a4d8');
test('digest.2', $mortal, 'think digest(sha,foo)', '752678a483e77799a3651face01d064f9ca86779');
test('digest.3', $mortal, 'think digest(sha1,foo)', '0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33');
test('digest.4', $mortal, 'think digest(dss1,foo)', '0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33');
test('digest.5', $mortal, 'think digest(ripemd160,foo)', '42cfa211018ea492fdee45ac637b7972a0ad6873');
test('digest.6', $mortal, 'think digest(md4,foo)', '0ac6700c491d70fb8650940b1ca1e4b2');


test('base64.1', $mortal, 'think encode64(test string)', 'dGVzdCBzdHJpbmc=');
test("base64.2", $mortal, "think decode64(encode64(this is another fine mess you've gotten us into))", "this is another fine mess you've gotten us into");
