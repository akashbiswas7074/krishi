'use client';

import { useEffect, useState } from 'react';
import { io, Socket } from 'socket.io-client';
import { IProduct } from '@/models/Product';

let socket: Socket;

export default function Screen1() {
  const [activeProduct, setActiveProduct] = useState<IProduct | null>(null);

  useEffect(() => {
    socket = io();
    
    socket.on('initialData', (data) => {
      if (data.activeProductId) {
        const found = data.products.find((p: any) => p.id === data.activeProductId);
        setActiveProduct(found || null);
      }
    });

    socket.on('productChanged', (product) => {
      setActiveProduct(product);
    });

    return () => {
        socket.disconnect();
    };
  }, []);

  if (!activeProduct) {
    return <div className="screen-container" style={{ color: 'var(--text-muted)' }}>Waiting for remote control...</div>;
  }

  return (
    <div className="screen-container fade-in">
      {activeProduct.imageUrl ? (
        <img src={activeProduct.imageUrl} className="s1-image" alt={activeProduct.name} />
      ) : (
        <div style={{ width: 400, height: 300, background: '#334155', borderRadius: 20, marginBottom: '2rem' }}></div>
      )}
      <h1 className="s1-title text-gradient">{activeProduct.name}</h1>
      <div className="s1-subtitle">{activeProduct.crops}</div>
    </div>
  );
}
