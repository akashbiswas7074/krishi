import type { NextApiRequest, NextApiResponse } from 'next';
import { getSession } from 'next-auth/react';
import dbConnect from '@/lib/mongodb';
import User from '@/models/User';
import { getServerSession } from "next-auth/next";
import { authOptions } from "../auth/[...nextauth]";

export default async function handler(req: NextApiRequest, res: NextApiResponse) {
  const session = await getServerSession(req, res, authOptions as any);

  if (!session || (session.user as any).role !== 'admin') {
    return res.status(403).json({ error: 'Access denied. Admins only.' });
  }

  await dbConnect();

  if (req.method === 'GET') {
    try {
      const users = await User.find({}, { password: 0 }).lean();
      return res.status(200).json(users);
    } catch (error) {
      return res.status(500).json({ error: 'Failed to fetch users' });
    }
  }

  if (req.method === 'DELETE') {
    const { id } = req.body;
    try {
      if (!id) return res.status(400).json({ error: 'User ID required' });
      await User.findByIdAndDelete(id);
      return res.status(200).json({ message: 'User deleted' });
    } catch (error) {
      return res.status(500).json({ error: 'Failed to delete user' });
    }
  }

  if (req.method === 'PATCH') {
    const { id, role } = req.body;
    try {
      if (!id || !role) return res.status(400).json({ error: 'ID and Role required' });
      const updated = await User.findByIdAndUpdate(id, { role }, { new: true });
      return res.status(200).json(updated);
    } catch (error) {
      return res.status(500).json({ error: 'Failed to update user' });
    }
  }

  return res.status(405).json({ error: 'Method not allowed' });
}
