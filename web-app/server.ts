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
  
  // REQUIRED for POST/DELETE requests from the dashboard
  expressApp.use(express.json());
  expressApp.use(express.urlencoded({ extended: true }));

  // Silence repetitive polling logs in the terminal
  expressApp.use((req: Request, res: Response, next: any) => {
    // Silence routine GET polls
    const silentPaths = ['/api/active-product', '/api/products', '/api/auth/session'];
    if (req.method === 'GET' && silentPaths.some(path => req.url.startsWith(path))) {
       // Note: Silencing logs in Next.js internal handle is difficult, 
       // but we ensure we don't add our own noise here.
    }
    next();
  });

  // All routes are handled by Next.js App Router
  expressApp.use((req: Request, res: Response) => {
    const protocol = req.headers['x-forwarded-proto'] || 'http';
    const host = req.headers.host || 'localhost';
    const baseUrl = `${protocol}://${host}`;
    const parsedUrl = new URL(req.url!, baseUrl);
    
    // Convert WHATWG URL to Next.js-compatible object to satisfy handle()
    const query: Record<string, string | string[]> = {};
    parsedUrl.searchParams.forEach((value, key) => {
      query[key] = value;
    });

    return handle(req, res, {
      pathname: parsedUrl.pathname,
      query,
      search: parsedUrl.search,
      hash: parsedUrl.hash,
      href: parsedUrl.href
    } as any);
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
  let isSlideshowActive = true;

  const startRotation = async () => {
    if (cycleInterval) {
      clearInterval(cycleInterval);
    }
    
    cycleInterval = setInterval(async () => {
      // If slideshow is paused, do nothing
      if (!isSlideshowActive) return;

      try {
        const products = await Product.find({ isActive: true }).lean();
        
        if (products.length === 0) {
          if (activeProductId !== null) {
            activeProductId = null;
            io.emit('productChanged', null);
          }
          return;
        }

        let nextIndex = 0;
        const currentIndex = products.findIndex(p => p.id === activeProductId);
        
        if (currentIndex !== -1) {
          nextIndex = (currentIndex + 1) % products.length;
        }
        
        const nextProduct: any = products[nextIndex];
        // Silenced rotation logs to avoid terminal clutter unless there is an error
        // console.log(`[Rotation] Advancing: ${activeProductId} -> ${nextProduct.id} (${nextProduct.name})`);
        
        activeProductId = nextProduct.id;
        const mappedProduct = { ...nextProduct, _id: nextProduct._id.toString() };
        
        io.emit('productChanged', mappedProduct);
        io.emit('hardwareCommand', {
          productLed: activeProductId,
          fieldLeds: nextProduct.fields || []
        });
      } catch (err) {
        console.error('[Rotation Error]', err);
      }
    }, 5000);
  };

  // Start the slideshow immediately
  startRotation();

  io.on('connection', async (socket) => {
    console.log('Client connected:', socket.id);
    
    // Fallback: Ensure rotation is running when the first client connects
    if (!cycleInterval) {
      console.log('[Server] Initializing rotation on client connection.');
      startRotation();
    }
    try {
      const products = await Product.find({}).lean();
      const mappedProducts = products.map(p => ({...p, _id: p._id.toString()}));
      socket.emit('initialData', { products: mappedProducts, activeProductId });
    } catch(err) {
      console.error('Error fetching initial products:', err);
    }

    socket.on('selectProduct', async (productId: string) => {
      isSlideshowActive = false;
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
       if (isSlideshowActive) startRotation();
    });

    socket.on('resumeSlideshow', () => {
      isSlideshowActive = true;
      io.emit('slideshowStatus', true);
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
