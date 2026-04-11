import mongoose from 'mongoose';
import bcrypt from 'bcryptjs';
import fs from 'fs';
import path from 'path';

// Import models
import Product from '../models/Product';
import User from '../models/User';
import dotenv from 'dotenv';

dotenv.config();

const MONGODB_URI = process.env.MONGODB_URI || 'mongodb://127.0.0.1:27017/agrarian';

async function seed() {
  try {
    await mongoose.connect(MONGODB_URI);
    console.log('Connected to MongoDB:', MONGODB_URI);

    // 1. Setup Admin User
    const existingAdmin = await User.findOne({ username: 'admin' });
    if (!existingAdmin) {
      const hashedPassword = await bcrypt.hash('password123', 10);
      await User.create({
        username: 'admin',
        password: hashedPassword,
      });
      console.log('Created default admin user (admin / password123)');
    } else {
      console.log('Admin user already exists');
    }

    // 2. Migrate Data from data.json (if it exists)
    const dataPath = path.join(process.cwd(), '../data.json');
    if (fs.existsSync(dataPath)) {
      const rawData = fs.readFileSync(dataPath, 'utf-8');
      const products = JSON.parse(rawData);

      let count = 0;
      for (const p of products) {
        await Product.findOneAndUpdate(
          { id: p.id },
          p,
          { returnDocument: 'after', upsert: true }
        );
        count++;
      }
      console.log(`Migrated ${count} products from data.json`);
    } else {
      console.log('No data.json found up one level. Skipping product migration.');
    }

    console.log('Seeding complete!');
    process.exit(0);
  } catch (err) {
    console.error('Seeding failed:', err);
    process.exit(1);
  }
}

seed();
