import type { NextApiRequest, NextApiResponse } from 'next';
import { v2 as cloudinary } from 'cloudinary';
import sharp from 'sharp';

cloudinary.config({
  cloud_name: process.env.CLOUDINARY_CLOUD_NAME,
  api_key: process.env.CLOUDINARY_API_KEY,
  api_secret: process.env.CLOUDINARY_API_SECRET,
});

export const config = {
  api: {
    bodyParser: {
      sizeLimit: '50mb',
    },
  },
};

export default async function handler(req: NextApiRequest, res: NextApiResponse) {
  if (req.method !== 'POST') {
    return res.status(405).json({ error: 'Method not allowed' });
  }

  try {
    const { image } = req.body;
    
    if (!image) {
      return res.status(400).json({ error: 'No image provided' });
    }

    // 1. Convert Base64 to Buffer
    // Handle both raw base64 and data URI
    const base64Data = image.split(';base64,').pop();
    if (!base64Data) throw new Error('Invalid image format');
    const buffer = Buffer.from(base64Data, 'base64');

    // 2. Convert to WebP using Sharp
    const webpBuffer = await sharp(buffer)
      .webp({ quality: 80 })
      .toBuffer();

    // 3. Upload specifically as WebP to Cloudinary via Stream
    const uploadFromBuffer = (buffer: Buffer) => {
      return new Promise((resolve, reject) => {
        const stream = cloudinary.uploader.upload_stream(
          { 
            folder: 'agrarian_products',
            format: 'webp'
          },
          (error, result) => {
            if (result) resolve(result);
            else reject(error);
          }
        );
        stream.end(buffer);
      });
    };

    const uploadResponse: any = await uploadFromBuffer(webpBuffer);

    return res.status(200).json({ imageUrl: uploadResponse.secure_url });
  } catch (error) {
    console.error('Image processing/upload error:', error);
    return res.status(500).json({ error: 'Image conversion or upload failed' });
  }
}
