import dbConnect from '../lib/mongodb';
import Product from '../models/Product';
import dotenv from 'dotenv';
import path from 'path';

dotenv.config();

async function debug() {
    await dbConnect();
    const products = await Product.find({});
    console.log('--- DATABASE STATUS ---');
    products.forEach(p => {
        console.log(`[${p.isActive ? 'ACTIVE' : 'OFF'}] ID: ${p.id} Name: ${p.name}`);
    });
    process.exit(0);
}

debug();
