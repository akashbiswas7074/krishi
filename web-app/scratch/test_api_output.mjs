import fetch from 'node-fetch';
import fs from 'fs';
import path from 'path';

async function testApi() {
  const id = 'prod_centurion_ez';
  const url = `http://localhost:3000/api/product-bitmap?id=${id}`;
  
  console.log(`Testing API for ID: ${id}`);
  try {
    const res = await fetch(url);
    if (!res.ok) {
      console.error(`Status: ${res.status} ${res.statusText}`);
      const text = await res.text();
      console.error(`Body: ${text}`);
      return;
    }
    
    const buffer = await res.buffer();
    console.log(`Received ${buffer.length} bytes`);
    
    const header = buffer.slice(0, 2);
    console.log(`Header: ${header.toString('hex')}`);
    
    if (header[0] === 0xFF && header[1] === 0xD8) {
      console.log('✅ Valid JPEG header (SOI)');
    } else {
      console.log('❌ Invalid JPEG header');
    }
    
    const outPath = path.join(__dirname, 'test_output.jpg');
    fs.writeFileSync(outPath, buffer);
    console.log(`Saved to ${outPath}`);
    
  } catch (error) {
    console.error('Fetch error:', error);
  }
}

testApi();
