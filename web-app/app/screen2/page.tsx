'use client';

import { useEffect, useState } from 'react';
import { io, Socket } from 'socket.io-client';
import { IProduct } from '@/models/Product';

let socket: Socket;

export default function Screen2() {
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

  const maxVal = Math.max(activeProduct.y25, activeProduct.y26, activeProduct.aspiration, 100);
  const getHeight = (val: number) => Math.max((val / maxVal) * 100, 5) + '%';

  return (
    <div className="screen-container fade-in">
      <div style={{ textAlign: 'center' }}>
        <h1 className="text-gradient" style={{ fontSize: '3rem' }}>{activeProduct.name} Analytics</h1>
        <div className="s1-subtitle">Sales & Market Projections</div>
      </div>
      
      <div className="chart-container">
        <div className="bar-wrapper">
          <div className="bar" style={{ height: getHeight(activeProduct.y25) }}>
            <span className="bar-value">{activeProduct.y25}</span>
          </div>
          <div className="bar-label">2025-26</div>
        </div>
        
        <div className="bar-wrapper">
          <div className="bar" style={{ height: getHeight(activeProduct.y26) }}>
            <span className="bar-value">{activeProduct.y26}</span>
          </div>
          <div className="bar-label">2026-27</div>
        </div>
        
        <div className="bar-wrapper">
          <div className="bar" style={{ height: getHeight(activeProduct.aspiration) }}>
            <span className="bar-value">{activeProduct.aspiration}</span>
          </div>
          <div className="bar-label">Aspiration</div>
        </div>
      </div>
    </div>
  );
}
