'use client';

import { useEffect, useState } from 'react';
import { io, Socket } from 'socket.io-client';
import { IProduct } from '@/models/Product';

let socket: Socket;

export default function Screen2() {
  const [activeProduct, setActiveProduct] = useState<IProduct | null>(null);
  const [liveHardwareStatus, setLiveHardwareStatus] = useState<any>(null);

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

    socket.on('liveHardwareStatus', (status) => {
      // Parse status if it's a string (ESP32 might send it as a string)
      try {
        const parsed = typeof status === 'string' ? JSON.parse(status) : status;
        setLiveHardwareStatus(parsed);
      } catch (e) {
        console.error('Failed to parse hardware status', e);
      }
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

      {/* Live Hardware Status Section */}
      <div className="glass-panel" style={{ marginTop: '4rem', padding: '1.5rem 3rem', width: '100%', maxWidth: '900px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1.5rem' }}>
           <h2 style={{ fontSize: '1.5rem', color: 'var(--text-muted)' }}>Live Diorama Status</h2>
           <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
              <div className={`status-dot ${liveHardwareStatus ? 'active' : ''}`}></div>
              <span style={{ fontSize: '0.9rem' }}>{liveHardwareStatus ? 'Hardware Connected' : 'Waiting for Hardware...'}</span>
           </div>
        </div>
        
        <div className="status-grid">
           {['paddy', 'jute', 'corn', 'potato', 'sugarcane', 'cauliflower', 'cabbage', 'brinjal', 'chilli', 'capsicum'].map(field => {
             const isActive = liveHardwareStatus?.fieldLeds?.includes(field);
             return (
               <div key={field} className={`status-item ${isActive ? 'active' : ''}`}>
                 <div className="indicator-box"></div>
                 <span style={{ textTransform: 'capitalize' }}>{field}</span>
               </div>
             );
           })}
        </div>
      </div>

    </div>
  );
}
