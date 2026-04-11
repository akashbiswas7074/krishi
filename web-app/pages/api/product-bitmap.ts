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
    const imageRes = await fetch(product.imageUrl);
    const arrayBuffer = await imageRes.arrayBuffer();
    const buffer = Buffer.from(arrayBuffer);

    // Process image: resize to 320x240 for TFT (contain mode to fit full image)
    const processed = await sharp(buffer)
      .resize(320, 240, { 
        fit: 'contain',
        background: { r: 0, g: 0, b: 0 }
      })
      .jpeg({ 
        quality: 80,
        progressive: false, // Standard baseline for microcontrollers
        chromaSubsampling: '4:2:0'
      })
      .toBuffer();

    res.setHeader('Content-Type', 'image/jpeg');
    res.setHeader('Cache-Control', 'public, max-age=3600');
    res.send(processed);
  } catch (error) {
    console.error('Image processing error:', error);
    res.status(500).json({ error: 'Failed to process image' });
  }
}
