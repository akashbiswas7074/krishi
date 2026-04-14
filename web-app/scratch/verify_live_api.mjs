import fetch from 'node-fetch';
import fs from 'fs';

async function verifyLiveImage() {
  const id = 'prod_centurion_ez';
  const url = `https://krishi-zxek.vercel.app/api/product-bitmap?id=${id}&t=${Date.now()}`;
  
  console.log(`🔍 Verifying LIVE Image for: ${id}`);
  console.log(`🔗 URL: ${url}`);

  try {
    const res = await fetch(url);
    if (!res.ok) {
      console.error(`❌ HTTP Error: ${res.status} ${res.statusText}`);
      return;
    }

    const buffer = await res.buffer();
    console.log(`📥 Downloaded: ${buffer.length} bytes`);

    // 1. SOI Check (Start of Image)
    const header = buffer.slice(0, 2);
    const hasSOI = header[0] === 0xFF && header[1] === 0xD8;
    console.log(`🧐 Header (SOI): ${header.toString('hex')} ${hasSOI ? '✅' : '❌'}`);

    // 2. JFIF Marker Check (APP0)
    const app0Marker = buffer.slice(2, 4);
    const hasJFIF = app0Marker[0] === 0xFF && app0Marker[1] === 0xE0;
    console.log(`🧐 Marker (APP0): ${app0Marker.toString('hex')} ${hasJFIF ? '✅ (JFIF Found)' : '❌ (Missing JFIF)'}`);

    if (hasSOI && hasJFIF) {
      console.log('🎉 SUCCESS: This is a perfect baseline JPEG for hardware!');
    } else if (hasSOI) {
      console.log('⚠️ WARNING: Valid JPEG but missing JFIF header (might cause issues on hardware).');
    } else {
      console.log('🚨 CRITICAL: Not a valid JPEG file!');
    }

    fs.writeFileSync('./verified_image.jpg', buffer);
    console.log('💾 Saved as ./verified_image.jpg for manual inspection.');

  } catch (err) {
    console.error('❌ Network Error:', err.message);
  }
}

verifyLiveImage();
