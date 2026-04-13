const XLSX = require('xlsx');
const mongoose = require('mongoose');
const path = require('path');
require('dotenv').config();

const MONGODB_URI = process.env.MONGODB_URI;

if (!MONGODB_URI) {
  console.error("Error: MONGODB_URI not found in .env");
  process.exit(1);
}

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
  ledPins2: { type: [Number], default: [] },
  cropPins: [{
    cropName: String,
    pin: Number
  }],
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

    const startIdx = data[0][' Budget (Bengal & Bihar)'] === 'Product Name' ? 1 : 0;
    const itemsToImport = data.slice(startIdx);

  // Ultra-Safe Pin Pool (excluding screen pins and reserved IOs)
  const globalPinPool = [
      4, 6, 7, 8, 9, 16, 18, 
      33, 34, 35, 36, 37, 38, 41
  ];
    let poolIdx = 0;
    const cropToPinMap = {}; // RE-ENABLED for efficiency

    for (let i = 0; i < itemsToImport.length; i++) {
        const item = itemsToImport[i];
        const name = item[' Budget (Bengal & Bihar)'];
        const rawCrops = item['__EMPTY'] || '';
        
        if (!name || name === 'Product Name' || name.trim() === '') continue;

        const y25 = parseFloat(item['__EMPTY_1']) || 0;
        const y26 = parseFloat(item['__EMPTY_2']) || 0;
        const aspiration = parseFloat(item['__EMPTY_3']) || 0;
        const unit = item['__EMPTY_4'] || 'Kg';

        // 1. Assign unique 5V LED Pin for the product (Feedback pin)
        const ledPin = globalPinPool[poolIdx % globalPinPool.length];
        poolIdx++;

        // 2. Parse crops and assign pins (REUSING pins for the same crop to save space)
        const individualCrops = rawCrops.split('/').map(s => s.trim()).filter(s => s !== '');
        const cropPins = [];
        const ledPins2 = [];

        for (const cropName of individualCrops) {
            let assignedPin;
            if (cropToPinMap[cropName]) {
                assignedPin = cropToPinMap[cropName];
            } else {
                assignedPin = globalPinPool[poolIdx % globalPinPool.length];
                cropToPinMap[cropName] = assignedPin;
                poolIdx++;
            }
            
            console.log(` - Product ${name} | Crop ${cropName} -> Pin ${assignedPin}`);
            
            cropPins.push({ cropName, pin: assignedPin });
            ledPins2.push(assignedPin);
        }

        const id = 'prod_' + name.toLowerCase().replace(/[^a-z0-9]/g, '_');

        console.log(`Product: ${name}`);
        console.log(` - Status Pin (5V): ${ledPin}`);
        console.log(` - Crop Pins (12V): ${ledPins2.join(', ')}`);

        await Product.findOneAndUpdate(
            { name: name },
            {
                id: id,
                name: name,
                crops: rawCrops,
                y25: y25,
                y26: y26,
                aspiration: aspiration,
                unit: unit,
                ledPin: ledPin,
                ledPins2: ledPins2,
                cropPins: cropPins
            },
            { returnDocument: 'after', upsert: true, setDefaultsOnInsert: true }
        );
    }

    console.log(`\nImport Complete.`);
    console.log(`- Total Unique Pins Consumed: ${poolIdx}`);
    
    process.exit(0);
  } catch (err) {
    console.error("Critical error during import:", err);
    process.exit(1);
  }
}

run();
