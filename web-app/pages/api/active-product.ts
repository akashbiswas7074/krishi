import type { NextApiRequest, NextApiResponse } from 'next';
import dbConnect from '@/lib/mongodb';
import Product from '@/models/Product';
import SystemConfig from '@/models/SystemConfig';

export default async function handler(req: NextApiRequest, res: NextApiResponse) {
  await dbConnect();

  // Ensure SysConfig exists
  let config = await SystemConfig.findOne();
  if (!config) {
    config = await SystemConfig.create({ isSlideshowActive: true, focusedProductId: null });
  }

  if (req.method === 'GET') {
    try {
      const activeProducts = await Product.find({ isActive: true }).lean();
      
      let focusedProduct = null;
      if (config.focusedProductId) {
        focusedProduct = await Product.findOne({ id: config.focusedProductId }).lean();
      }

      return res.status(200).json({ 
        status: config.isSlideshowActive ? 'slideshow' : 'fixed',
        isSlideshowActive: config.isSlideshowActive,
        focusedProductId: config.focusedProductId,
        activeProducts: activeProducts.map(p => ({
          id: p.id,
          name: p.name,
          crops: p.crops,
          y25: p.y25,
          y26: p.y26,
          aspiration: p.aspiration,
          ledPin: p.ledPin ?? 2,
          ledPins2: p.ledPins2 ?? [],
          cropPins: p.cropPins ?? [],
          unit: p.unit ?? 'Kg',
          imageUrl: p.imageUrl
        })),
        focusedProduct: focusedProduct ? {
          id: focusedProduct.id,
          name: focusedProduct.name,
          crops: focusedProduct.crops,
          y25: focusedProduct.y25,
          y26: focusedProduct.y26,
          aspiration: focusedProduct.aspiration,
          ledPin: focusedProduct.ledPin ?? 2,
          ledPins2: focusedProduct.ledPins2 ?? [],
          cropPins: focusedProduct.cropPins ?? [],
          unit: focusedProduct.unit ?? 'Kg',
          imageUrl: focusedProduct.imageUrl
        } : null
      });
    } catch (error) {
      return res.status(500).json({ error: 'Failed to fetch active products' });
    }
  }

  if (req.method === 'POST') {
    const { id, action } = req.body;

    try {
      if (action === 'resume') {
        config.isSlideshowActive = true;
        config.focusedProductId = null;
        await config.save();
        return res.status(200).json({ message: 'Slideshow resumed', config });
      }

      if (!id) return res.status(400).json({ error: 'Product ID required' });

      // Lock on specific product
      config.isSlideshowActive = false;
      config.focusedProductId = id;
      await config.save();

      return res.status(200).json({ message: 'View locked on product', id });
    } catch (error) {
      return res.status(500).json({ error: 'Failed to update configuration' });
    }
  }

  return res.status(405).json({ error: 'Method not allowed' });
}
