const XLSX = require('xlsx');
const mongoose = require('mongoose');
const path = require('path');
require('dotenv').config();

const MONGODB_URI = process.env.MONGODB_URI;

if (!MONGODB_URI) {
  console.error("Error: MONGODB_URI not found in .env");
  process.exit(1);
}

// Reuse Schema definition from models/Product.ts (but for a standalone script)
const ProductSchema = new mongoose.Schema({
  id: { type: String, required: true, unique: true },
  name: { type: String, required: true },
  crops: { type: String, required: true },
  y25: { type: Number, required: true, default: 0 },
  y26: { type: Number, required: true, default: 0 },
  aspiration: { type: Number, required: true, default: 0 },
  imageUrl: { type: String, default: null },
  isActive: { type: Boolean, default: false },
  ledPin: { type: Number, default: 2 },
  unit: { type: String, default: 'Kg' }
}, { timestamps: true });

const Product = mongoose.models.Product || mongoose.model('Product', ProductSchema);

async function run() {
  try {
    console.log("Connecting to MongoDB...");
    await mongoose.connect(MONGODB_URI);
    console.log("Connected to MongoDB.");

    const filePath = path.join(__dirname, '../public/Budget_Terget.xlsx');
    const workbook = XLSX.readFile(filePath);
    const sheetName = workbook.SheetNames[0];
    const sheet = workbook.Sheets[sheetName];
    const data = XLSX.utils.sheet_to_json(sheet);

    // Filter out rows that don't have a product name (skipping headers/empty rows)
    // Based on our debug: crop name is in __EMPTY, product name is in ' Budget (Bengal & Bihar)'
    
    // We skip the first element if it's the "Product Name" row
    const startIdx = data[0][' Budget (Bengal & Bihar)'] === 'Product Name' ? 1 : 0;
    const itemsToImport = data.slice(startIdx);

    console.log(`Found ${itemsToImport.length} potential products to import.`);

    let successCount = 0;
    
    for (const item of itemsToImport) {
      const name = item[' Budget (Bengal & Bihar)'];
      const crops = item['__EMPTY'];
      const y25 = parseFloat(item['__EMPTY_1']) || 0;
      const y26 = parseFloat(item['__EMPTY_2']) || 0;
      const aspiration = parseFloat(item['__EMPTY_3']) || 0;
      const unit = item['__EMPTY_4'] || 'Kg';

      if (!name || name === 'Product Name') continue;

      // Generate a simple slug for ID
      const id = 'prod_' + name.toLowerCase().replace(/[^a-z0-9]/g, '_') + '_' + Math.floor(Math.random() * 1000);

      // Upsert: search by name, update all fields
      await Product.findOneAndUpdate(
        { name: name },
        {
          id: id,
          name: name,
          crops: crops,
          y25: y25,
          y26: y26,
          aspiration: aspiration,
          unit: unit,
          ledPin: 2 // Default pin
        },
        { returnDocument: 'after', upsert: true, setDefaultsOnInsert: true }
      );
      
      successCount++;
    }

    console.log(`Successfully processed ${successCount} products.`);
    process.exit(0);
  } catch (err) {
    console.error("Critical error during import:", err);
    process.exit(1);
  }
}

run();
