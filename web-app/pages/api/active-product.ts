import type { NextApiRequest, NextApiResponse } from 'next';
import dbConnect from '@/lib/mongodb';
import Product from '@/models/Product';

export default async function handler(req: NextApiRequest, res: NextApiResponse) {
  await dbConnect();

  if (req.method === 'GET') {
    try {
      const activeProduct = await Product.findOne({ isActive: true }).lean();
      if (!activeProduct) {
        return res.status(200).json({ status: 'idle', activeProduct: null });
      }
      return res.status(200).json({ 
        status: 'active', 
        activeProduct: {
          id: activeProduct.id,
          name: activeProduct.name,
          crops: activeProduct.crops,
          y25: activeProduct.y25,
          y26: activeProduct.y26,
          aspiration: activeProduct.aspiration,
          ledPin: activeProduct.ledPin ?? 2,
          unit: activeProduct.unit ?? 'Kg'
        } 
      });
    } catch (error) {
      return res.status(500).json({ error: 'Failed to fetch active product' });
    }
  }

  if (req.method === 'POST') {
    const { id } = req.body;
    if (!id) return res.status(400).json({ error: 'Product ID required' });

    try {
      // 1. Reset logic removed to allow "Selective Slideshow" to function correctly.
      // 2. Simply set the chosen product to active (Ensures it is part of the display rotation)
      const updated = await Product.findOneAndUpdate(
        { id },
        { isActive: true },
        { returnDocument: 'after' }
      );

      if (!updated) return res.status(404).json({ error: 'Product not found' });

      return res.status(200).json({ message: 'Active product updated', product: updated });
    } catch (error) {
      return res.status(500).json({ error: 'Failed to update active product' });
    }
  }

  return res.status(405).json({ error: 'Method not allowed' });
}
