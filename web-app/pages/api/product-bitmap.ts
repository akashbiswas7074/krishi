import { NextApiRequest, NextApiResponse } from 'next';
import sharp from 'sharp';
import dbConnect from '@/lib/mongodb';
import Product from '@/models/Product';

export default async function handler(req: NextApiRequest, res: NextApiResponse) {
  const { id } = req.query;
  if (!id) return res.status(400).json({ error: 'ID required' });

  await dbConnect();
  const product = await Product.findOne({ id });
  
  if (!product || !product.imageUrl) {
    // Return a blank black JPEG if no image
    const blank = await sharp({
      create: {
        width: 320,
        height: 240,
        channels: 3,
        background: { r: 0, g: 0, b: 0 }
      }
    }).jpeg().toBuffer();
    return res.setHeader('Content-Type', 'image/jpeg').send(blank);
  }

  try {
    let finalUrl = product.imageUrl;
    
    // Cloudinary Smart Optimization: Transform to 320x240 JPEG on the CDN side
    if (finalUrl.includes('cloudinary.com')) {
      // Ensure https protocol
      if (finalUrl.startsWith('//')) finalUrl = 'https:' + finalUrl;
      
      // Inject transformation flags: w_320,h_240,c_fill,f_jpg,q_auto
      // Works by replacing '/upload/' with '/upload/w_320,h_240,c_fill,f_jpg,q_auto/'
      finalUrl = finalUrl.replace('/upload/', '/upload/w_320,h_240,c_fill,f_jpg,q_auto/');
    }

    console.log(`🔄 Fetching Image [${id}]: ${finalUrl}`);

    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 10000); // 10s timeout

    const imageRes = await fetch(finalUrl, { signal: controller.signal });
    clearTimeout(timeout);

    if (!imageRes.ok) throw new Error(`Fetch failed: ${imageRes.statusText}`);

    const arrayBuffer = await imageRes.arrayBuffer();
    const buffer = Buffer.from(arrayBuffer);

    // Final safety resize for ESP32 (in case Cloudinary trans fails or it's a non-cloudinary URL)
    const processed = await sharp(buffer)
      .resize(320, 240, { 
        fit: 'contain',
        background: { r: 0, g: 0, b: 0 }
      })
      .jpeg({ 
        quality: 40, 
        mozjpeg: true, 
        progressive: false, 
        chromaSubsampling: '4:2:0' 
      })
      .toBuffer();

    console.log(`✅ Image Ready [${id}]: ${processed.length} bytes`);
    res.setHeader('Content-Type', 'image/jpeg');
    res.setHeader('Cache-Control', 'public, max-age=3600');
    res.send(processed);
  } catch (error) {
    console.error(`❌ Image Error [${id}]:`, error);
    // Return blank fallback on error
    const blank = await sharp({
      create: { width: 320, height: 240, channels: 3, background: { r: 50, g: 0, b: 0 } }
    }).jpeg().toBuffer();
    res.setHeader('Content-Type', 'image/jpeg').send(blank);
  }
}
