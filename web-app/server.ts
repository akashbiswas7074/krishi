import { createServer } from 'http';
import { parse } from 'url';
import next from 'next';
import { Server as SocketIOServer } from 'socket.io';
import dotenv from 'dotenv';
import dbConnect from './lib/mongodb';
import Product from './models/Product';
import { Application, Request, Response } from 'express';

// Use require since express might have default import issues in this setup
const express = require('express');

dotenv.config();

const dev = process.env.NODE_ENV !== 'production';
const hostname = '0.0.0.0';
const port = parseInt(process.env.PORT || '3000', 10);

const app = next({ dev, hostname, port });
const handle = app.getRequestHandler();

app.prepare().then(async () => {
  await dbConnect();

  const expressApp: Application = express();
  
  // Removed express.json() and express.urlencoded() globally 
  // because Next.js App Router API routes must read the raw request stream themselves.

  // All routes are handled by Next.js App Router
  expressApp.use((req: Request, res: Response) => {
    const parsedUrl = parse(req.url, true);
    return handle(req, res, parsedUrl);
  });

  const httpServer = createServer(expressApp);

  const io = new SocketIOServer(httpServer, {
    cors: {
      origin: '*',
      methods: ['GET', 'POST']
    }
  });

  let activeProductId: string | null = null;
  let cycleInterval: NodeJS.Timeout | null = null;

  const startRotation = async () => {
    if (cycleInterval) clearInterval(cycleInterval);
    
    cycleInterval = setInterval(async () => {
      try {
        const products = await Product.find({ isActive: true }).lean();
        console.log(`[Rotation] Active products found: ${products.length}`);
        
        if (products.length === 0) {
          if (activeProductId !== null) {
            console.log(`[Rotation] No active products. Going to IDLE.`);
            activeProductId = null;
            io.emit('productChanged', null);
          }
          return;
        }

        const currentIndex = products.findIndex(p => p.id === activeProductId);
        const nextIndex = (currentIndex + 1) % products.length;
        const nextProduct: any = products[nextIndex];
        
        console.log(`[Rotation] Advancing to: ${nextProduct.name} (${nextProduct.id})`);
        
        activeProductId = nextProduct.id;
        const mappedProduct = { ...nextProduct, _id: nextProduct._id.toString() };
        
        io.emit('productChanged', mappedProduct);
        io.emit('hardwareCommand', {
          productLed: activeProductId,
          fieldLeds: nextProduct.fields || []
        });
      } catch (err) {
        console.error('Rotation error:', err);
      }
    }, 5000);
  };

  // Start the slideshow immediately
  startRotation();

  io.on('connection', async (socket) => {
    console.log('Client connected:', socket.id);
    
    try {
      const products = await Product.find({}).lean();
      const mappedProducts = products.map(p => ({...p, _id: p._id.toString()}));
      socket.emit('initialData', { products: mappedProducts, activeProductId });
    } catch(err) {
      console.error('Error fetching initial products:', err);
    }

    socket.on('selectProduct', async (productId: string) => {
      activeProductId = productId;
      try {
        const product: any = await Product.findOne({ id: productId }).lean();
        if (product) {
          const mappedProduct = { ...product, _id: product._id.toString() };
          io.emit('productChanged', mappedProduct);
          
          io.emit('hardwareCommand', {
            productLed: productId,
            fieldLeds: product.fields || []
          });

          // Reset the timer so the user gets a full 5s of the selected product
          startRotation(); 
        }
      } catch (err) {
        console.error('Error on selectProduct:', err);
      }
    });

    socket.on('productsUpdated', async () => {
       const products = await Product.find({}).lean();
       const mappedProducts = products.map(p => ({...p, _id: p._id.toString()}));
       io.emit('productsUpdated', mappedProducts);
       // Refresh rotation to include any new items
       startRotation();
    });
    
    socket.on('hardwareStatus', (status) => {
      console.log('Hardware status received:', status);
      io.emit('liveHardwareStatus', status);
    });

    socket.on('disconnect', () => {
      console.log('Client disconnected:', socket.id);
    });
  });

  httpServer.listen(port, () => {
    console.log(`> Ready on http://${hostname}:${port}`);
  });
}).catch((err) => {
  console.error(err);
  process.exit(1);
});
