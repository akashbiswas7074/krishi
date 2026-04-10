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
          fields: activeProduct.fields
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
      // 1. Reset all products to inactive
      await Product.updateMany({}, { isActive: false });
      
      // 2. Set the chosen product to active
      const updated = await Product.findOneAndUpdate(
        { id },
        { isActive: true },
        { new: true }
      );

      if (!updated) return res.status(404).json({ error: 'Product not found' });

      return res.status(200).json({ message: 'Active product updated', product: updated });
    } catch (error) {
      return res.status(500).json({ error: 'Failed to update active product' });
    }
  }

  return res.status(405).json({ error: 'Method not allowed' });
}
