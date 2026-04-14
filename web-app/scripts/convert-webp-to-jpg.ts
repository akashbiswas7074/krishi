import mongoose from 'mongoose';
import dotenv from 'dotenv';
import path from 'path';

// Load environment variables from .env
dotenv.config({ path: path.join(__dirname, '../.env') });

const MONGODB_URI = process.env.MONGODB_URI;

if (!MONGODB_URI) {
  console.error('MONGODB_URI is not defined in .env');
  process.exit(1);
}

// Define Schema here to avoid dependency issues in one-off script
const ProductSchema = new mongoose.Schema({
  imageUrl: { type: String, default: null },
});

const Product = mongoose.models.Product || mongoose.model('Product', ProductSchema);

async function migrate() {
  try {
    console.log('Connecting to MongoDB...');
    await mongoose.connect(MONGODB_URI!);
    console.log('Connected successfully.');

    // Find all products with .webp in imageUrl
    const products = await Product.find({ imageUrl: { $regex: /\.webp$/i } });
    console.log(`Found ${products.length} products with WebP images.`);

    let updatedCount = 0;
    for (const product of products) {
      if (product.imageUrl) {
        const oldUrl = product.imageUrl;
        const newUrl = oldUrl.replace(/\.webp$/i, '.jpg');
        
        product.imageUrl = newUrl;
        await product.save();
        updatedCount++;
        console.log(`Updated: ${oldUrl} -> ${newUrl}`);
      }
    }

    console.log(`Migration complete. ${updatedCount} products updated.`);
  } catch (error) {
    console.error('Migration failed:', error);
  } finally {
    await mongoose.disconnect();
    console.log('Disconnected from MongoDB.');
  }
}

migrate();
