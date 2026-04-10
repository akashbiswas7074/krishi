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
const hostname = 'localhost';
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
        const product = await Product.findOne({ id: productId }).lean();
        if (product) {
          product._id = product._id.toString(); 
          io.emit('productChanged', product);
          
          io.emit('hardwareCommand', {
            productLed: productId,
            fieldLeds: product.fields || []
          });
        }
      } catch (err) {
        console.error('Error on selectProduct:', err);
      }
    });

    socket.on('productsUpdated', async () => {
       const products = await Product.find({}).lean();
       const mappedProducts = products.map(p => ({...p, _id: p._id.toString()}));
       io.emit('productsUpdated', mappedProducts);
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
