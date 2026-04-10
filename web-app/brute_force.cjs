const cloudinary = require('cloudinary').v2;

const cloud_name = 'dlrlet9fg';
const api_key = '416837962276128';

// Ambiguous characters: Z2-[char1]V92Hx-[char2]kxyrJSqCf8HpNyWs
const char1_options = ['I', 'l', '1'];
const char2_options = ['I', 'l', '1'];

// 1x1 transparent png
const testImage = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII=';

async function testConfigurations() {
  for (const c1 of char1_options) {
    for (const c2 of char2_options) {
      const secret = `Z2-${c1}V92Hx-${c2}kxyrJSqCf8HpNyWs`;
      cloudinary.config({
        cloud_name,
        api_key,
        api_secret: secret
      });
      
      try {
        const res = await cloudinary.uploader.upload(testImage, { folder: 'test_folder' });
        console.log(`\n\n✅ SUCCESS! The correct secret is: ${secret}\n\n`);
        
        // Clean up the test image
        await cloudinary.uploader.destroy(res.public_id);
        return secret;
      } catch (err) {
        process.stdout.write('.');
      }
    }
  }
  
  console.log("\n\n❌ All combinations failed. There must be another mistyped letter!");
}

testConfigurations();
