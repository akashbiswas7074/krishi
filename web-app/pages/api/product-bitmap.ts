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
    
    // Cloudinary Smart Optimization: Transform to 320x240 on the CDN side
    if (finalUrl.includes('cloudinary.com')) {
      // Ensure https protocol
      if (finalUrl.startsWith('//')) finalUrl = 'https:' + finalUrl;
      
      // Inject transformation flags: w_640,h_480,c_limit (preserves aspect ratio)
      finalUrl = finalUrl.replace('/upload/', '/upload/w_640,h_480,c_limit/');
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
      .trim() // Automatically remove white/transparent margins
      .resize(320, 240, { 
        fit: 'fill',
        background: { r: 255, g: 255, b: 255 }
      })
      .rotate() // Auto-rotate based on EXIF before stripping
      .jpeg({ 
        quality: 70,                // Balanced quality
        chromaSubsampling: '4:2:0', // Standard subsampling for hardware
        progressive: false,         // MUST be false for hardware decoders
        optimiseScans: false,       // Force baseline JPEG
        mozjpeg: false,             // Use standard encoder
        jfif: true,                 // FORCE JFIF header (APP0) - Critical for some hardware
      } as any)
      .toBuffer();

    console.log(`✅ Image Ready [${id}]: ${processed.length} bytes`);
    
    // CACHE CONTROL: Set to no-store to force hardware to get fresh images during fix
    res.setHeader('Content-Type', 'image/jpeg');
    res.setHeader('Content-Length', processed.length);
    res.setHeader('Cache-Control', 'no-store, no-cache, must-revalidate, proxy-revalidate');
    res.setHeader('Pragma', 'no-cache');
    res.setHeader('Expires', '0');
    
    // Use .end() with the buffer for strict binary transport
    res.end(processed);
  } catch (error) {
    console.error(`❌ Image Error [${id}]:`, error);
    // Return blank fallback on error
    const blank = await sharp({
      create: { width: 320, height: 240, channels: 3, background: { r: 50, g: 0, b: 0 } }
    }).jpeg().toBuffer();
    res.setHeader('Content-Type', 'image/jpeg').send(blank);
  }
}
