import type { NextApiRequest, NextApiResponse } from 'next';
import dbConnect from '@/lib/mongodb';
import Product from '@/models/Product';

export default async function handler(req: NextApiRequest, res: NextApiResponse) {
  if (req.method !== 'POST' && req.method !== 'DELETE') {
    return res.status(405).json({ error: 'Method not allowed' });
  }

  try {
    await dbConnect();

    if (req.method === 'DELETE') {
      const { id } = req.body;
      if (!id) return res.status(400).json({ error: 'Product ID required' });
      await Product.findOneAndDelete({ id });
      return res.status(200).json({ message: 'Deleted successfully' });
    }

    // Handle POST (Create / Update)
    const body = req.body;
    const product = await Product.findOneAndUpdate(
      { id: body.id },
      body,
      { new: true, upsert: true }
    );
    
    return res.status(201).json(product);
  } catch (error) {
    console.error(error);
    return res.status(500).json({ error: 'Operation failed' });
  }
}
