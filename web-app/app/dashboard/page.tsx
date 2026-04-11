'use client';

import { useEffect, useState } from 'react';
import { io, Socket } from 'socket.io-client';
import { IProduct } from '@/models/Product';

let socket: Socket | null = null;
import { signOut } from 'next-auth/react';

// Helper to convert File to Base64
const toBase64 = (file: File): Promise<string> => new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.readAsDataURL(file);
    reader.onload = () => resolve(reader.result as string);
    reader.onerror = error => reject(error);
});

export default function Dashboard() {
  const [products, setProducts] = useState<IProduct[]>([]);
  const [activeProduct, setActiveProduct] = useState<IProduct | null>(null);
  const [tab, setTab] = useState<'control' | 'admin'>('control');

  const [formData, setFormData] = useState({
    name: '', crops: '', y25: '', y26: '', aspiration: '', ledPin: '', unit: 'Kg'
  });
  const [isAutoCycle, setIsAutoCycle] = useState(false);
  const [imageFile, setImageFile] = useState<File | null>(null);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [editingProductId, setEditingProductId] = useState<string | null>(null);

  const fetchProducts = async () => {
    try {
      const res = await fetch('/api/products');
      if (res.ok) {
        const data = await res.json();
        setProducts(data);
      }
    } catch (err) {
      console.error('Failed to fetch products via HTTP', err);
    }
  };

  useEffect(() => {
    fetchProducts();
    socket = io();

    socket.on('initialData', (data) => {
      setProducts(data.products);
      if(data.activeProductId) {
        const found = data.products.find((p: any) => p.id === data.activeProductId);
        setActiveProduct(found || null);
      }
    });

    socket.on('productsUpdated', (updatedProducts) => {
      setProducts(updatedProducts);
    });

    socket.on('productChanged', (product) => {
      setActiveProduct(product);
    });

    return () => {
      socket?.disconnect();
    };
  }, []);

  useEffect(() => {
    let interval: NodeJS.Timeout;
    if (isAutoCycle && tab === 'control' && products.length > 1) {
      interval = setInterval(() => {
        const currentIndex = products.findIndex(p => p.id === activeProduct?.id);
        const nextIndex = (currentIndex + 1) % products.length;
        selectProduct(products[nextIndex].id);
      }, 5000);
    }
    return () => clearInterval(interval);
  }, [isAutoCycle, tab, activeProduct, products]);

  const selectProduct = async (id: string) => {
    if (socket && socket.connected) {
      socket.emit('selectProduct', id);
    }
    
    try {
      await fetch('/api/active-product', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id })
      });
    } catch (err) {
      console.error('Failed to sync active product to DB:', err);
    }
  };

  const handleEdit = (e: React.MouseEvent, p: IProduct) => {
    e.stopPropagation();
    setFormData({
      name: p.name, 
      crops: p.crops, 
      y25: p.y25.toString(), 
      y26: p.y26.toString(), 
      aspiration: p.aspiration.toString(), 
      ledPin: p.ledPin?.toString() || '0',
      unit: p.unit || ''
    });
    setEditingProductId(p.id);
    setTab('admin');
  };

  const handleDelete = async (e: React.MouseEvent, id: string) => {
    e.stopPropagation();
    if (!confirm('Are you sure you want to delete this product?')) return;
    try {
      await fetch('/api/products', {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id })
      });
      if (socket) socket.emit('productsUpdated');
    } catch(err) {
      console.error(err);
      alert('Failed to delete');
    }
  };

  const submitAdminForm = async (e: React.FormEvent) => {
    e.preventDefault();
    setIsSubmitting(true);
    let imageUrl = null;

    if (imageFile) {
      try {
        const base64Img = await toBase64(imageFile);
        const res = await fetch('/api/upload', { 
            method: 'POST', 
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ image: base64Img }) 
        });
        const data = await res.json();
        
        if (data.error) throw new Error(data.error);
        imageUrl = data.imageUrl;
      } catch(err) {
        alert("Image upload failed");
        console.error(err);
        setIsSubmitting(false);
        return;
      }
    }

    const payload: any = {
      id: editingProductId || ('prod_' + Date.now()),
      name: formData.name,
      crops: formData.crops,
      y25: Number(formData.y25),
      y26: Number(formData.y26),
      aspiration: Number(formData.aspiration),
      ledPin: isNaN(Number(formData.ledPin)) ? 0 : Number(formData.ledPin),
      unit: formData.unit
    };
    if (imageUrl) payload.imageUrl = imageUrl;

    try {
      await fetch('/api/products', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      alert(editingProductId ? "Product Updated Successfully!" : "Product Added Successfully!");
      setFormData({ 
        name: '', crops: '', y25: '', y26: '', aspiration: '', ledPin: '', unit: 'Kg'
      });
      setImageFile(null);
      setEditingProductId(null);
      setTab('control');
      if (socket) socket.emit('productsUpdated');
    } catch(err) {
      console.error(err);
      alert("Error saving product");
    } finally {
      setIsSubmitting(false);
    }
  };

  return (
    <div className="dashboard-container fade-in">
      <div className="header-actions">
        <h1 className="text-gradient">Master Remote Control</h1>
        <button className="btn" style={{ background: '#ef4444' }} onClick={() => signOut()}>Sign Out</button>
      </div>
      
      <div style={{ display: 'flex', gap: '1rem', marginBottom: '2rem', alignItems: 'center' }}>
        <button className="btn" style={{ background: tab==='control' ? 'var(--accent)' : 'transparent', border: '1px solid var(--glass-border)' }} onClick={() => setTab('control')}>Control Screens</button>
        <button className="btn" style={{ background: tab==='admin' ? 'var(--accent)' : 'transparent', border: '1px solid var(--glass-border)' }} onClick={() => setTab('admin')}>Add Product</button>
        {tab === 'control' && (
          <button 
            className="btn" 
            style={{ marginLeft: 'auto', background: isAutoCycle ? '#22c55e' : '#475569', display: 'flex', alignItems: 'center', gap: '8px' }}
            onClick={() => setIsAutoCycle(!isAutoCycle)}
          >
            <div style={{ width: 8, height: 8, borderRadius: '50%', background: isAutoCycle ? '#fff' : '#94a3b8', boxShadow: isAutoCycle ? '0 0 8px #fff' : 'none' }}></div>
            Auto Cycle (5s)
          </button>
        )}
      </div>

      {tab === 'control' && (
        <>
          <div className="preview-container" style={{ display: 'flex', gap: '1.5rem', marginBottom: '3rem', flexWrap: 'wrap', justifyContent: 'center' }}>
             <div className="virtual-tft" style={{ border: '8px solid #334155', borderRadius: '12px', overflow: 'hidden', width: '320px', height: '240px', background: '#000', position: 'relative', boxShadow: '0 10px 25px rgba(0,0,0,0.5)' }}>
                <div style={{ position: 'absolute', top: 5, right: 10, fontSize: '0.6rem', color: '#64748b', fontWeight: 'bold' }}>SCREEN 1 - IMAGE</div>
                {activeProduct?.imageUrl ? (
                  <img src={activeProduct.imageUrl} style={{ width: '100%', height: '100%', objectFit: 'contain' }} alt="preview" />
                ) : (
                  <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#475569' }}>No Image</div>
                )}
             </div>

             <div className="virtual-tft" style={{ border: '8px solid #334155', borderRadius: '12px', overflow: 'hidden', width: '320px', height: '240px', background: '#000', color: '#fff', position: 'relative', fontFamily: 'monospace', boxShadow: '0 10px 25px rgba(0,0,0,0.5)' }}>
                <div style={{ position: 'absolute', bottom: 5, right: 10, fontSize: '0.6rem', color: '#64748b', fontWeight: 'bold' }}>SCREEN 2 - DETAILS</div>
                <div style={{ background: '#182828', height: '50px', padding: '10px', display: 'flex', alignItems: 'center' }}>
                   <div style={{ fontSize: '1.5rem', fontWeight: 'bold', overflow: 'hidden', whiteSpace: 'nowrap', textOverflow: 'ellipsis' }}>{activeProduct?.name || 'Waiting...'}</div>
                </div>
                <div style={{ padding: '10px 15px' }}>
                   <div style={{ color: '#00FF00', fontSize: '1.2rem', marginBottom: '8px' }}>Crops: <span style={{ color: '#fff' }}>{activeProduct?.crops || '-'}</span></div>
                   
                   <div style={{ display: 'flex', flexDirection: 'column', gap: '15px', marginTop: '15px' }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', borderBottom: '1px solid #1e293b', paddingBottom: '8px' }}>
                         <div style={{ color: '#2563eb', fontWeight: 'bold' }}>2025-26 Sales:</div>
                         <div style={{ fontSize: '1.2rem' }}>{activeProduct?.y25?.toLocaleString() || 0} <span style={{fontSize: '0.8rem'}}>{activeProduct?.unit}</span></div>
                      </div>
                      
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', borderBottom: '1px solid #1e293b', paddingBottom: '8px' }}>
                         <div style={{ color: '#06b6d4', fontWeight: 'bold' }}>2026-27 Sales:</div>
                         <div style={{ fontSize: '1.2rem' }}>{activeProduct?.y26?.toLocaleString() || 0} <span style={{fontSize: '0.8rem'}}>{activeProduct?.unit}</span></div>
                      </div>

                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                         <div style={{ color: '#22c55e', fontWeight: 'bold' }}>Aspiration Target:</div>
                         <div style={{ fontSize: '1.2rem', fontWeight: 'bold' }}>{activeProduct?.aspiration?.toLocaleString() || 0} <span style={{fontSize: '0.8rem'}}>{activeProduct?.unit}</span></div>
                      </div>
                   </div>
                </div>
             </div>
          </div>
        
          <div className="products-grid">
          {products.map(p => (
            <div 
              key={p.id} 
              className={`product-card ${activeProduct?.id === p.id ? 'active' : ''}`}
              onClick={() => selectProduct(p.id)}
            >
              {p.imageUrl ? 
                <img src={p.imageUrl} alt="product" /> : 
                <div style={{ width: 60, height: 60, background: '#334155', borderRadius: 8 }}></div>
              }
              <div style={{ flex: 1 }}>
                <div style={{ fontWeight: 'bold', display: 'flex', alignItems: 'center', gap: '8px' }}>
                  {p.name}
                  {activeProduct?.id === p.id && (
                    <span style={{ fontSize: '0.7rem', background: '#22c55e', padding: '2px 6px', borderRadius: '4px', color: 'white' }}>ACTIVE</span>
                  )}
                </div>
                <div style={{ fontSize: '0.9rem', color: 'var(--text-muted)' }}>{p.crops} {p.unit ? `(${p.unit})` : ''}</div>
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '0.5rem', minWidth: '90px' }}>
                {activeProduct?.id !== p.id ? (
                  <button 
                    className="btn" 
                    style={{ padding: '0.3rem 0.6rem', fontSize: '0.8rem', background: 'var(--accent)' }} 
                    onClick={(e) => { e.stopPropagation(); selectProduct(p.id); }}
                  >Activate</button>
                ) : (
                  <button 
                    className="btn" 
                    disabled
                    style={{ padding: '0.3rem 0.6rem', fontSize: '0.8rem', background: '#22c55e', opacity: 1, cursor: 'default' }} 
                  >Live ON</button>
                )}
                <div style={{ display: 'flex', gap: '0.3rem' }}>
                  <button 
                     className="btn" 
                     style={{ flex: 1, padding: '0.3rem', fontSize: '0.7rem' }} 
                     onClick={(e) => { e.stopPropagation(); handleEdit(e, p); }}>Edit</button>
                  <button 
                     className="btn" 
                     style={{ flex: 1, padding: '0.3rem', fontSize: '0.7rem', background: '#ef4444' }} 
                     onClick={(e) => { e.stopPropagation(); handleDelete(e, p.id); }}>Del</button>
                </div>
              </div>
            </div>
          ))}
          </div>
        </>
      )}

      {tab === 'admin' && (
        <div className="glass-panel" style={{ maxWidth: '600px' }}>
          <form onSubmit={submitAdminForm}>
            <div className="form-group">
              <label>Product Name</label>
              <input type="text" required value={formData.name} onChange={e => setFormData({...formData, name: e.target.value})} />
            </div>
            <div className="form-group">
              <label>Target Crops</label>
              <input type="text" required value={formData.crops} onChange={e => setFormData({...formData, crops: e.target.value})} />
            </div>
            <div className="form-group">
              <label>Product Image</label>
              <input type="file" accept="image/*" onChange={e => {
                  if (e.target.files && e.target.files.length > 0) setImageFile(e.target.files[0]);
              }} />
            </div>
            <div style={{ display: 'flex', gap: '1rem' }}>
              <div className="form-group" style={{ flex: 1 }}>
                <label>Sales 25-26</label>
                <input type="number" required value={formData.y25} onChange={e => setFormData({...formData, y25: e.target.value})} />
              </div>
              <div className="form-group" style={{ flex: 1 }}>
                <label>Sales 26-27</label>
                <input type="number" required value={formData.y26} onChange={e => setFormData({...formData, y26: e.target.value})} />
              </div>
              <div className="form-group" style={{ flex: 1 }}>
                <label>Aspiration</label>
                <input type="number" required value={formData.aspiration} onChange={e => setFormData({...formData, aspiration: e.target.value})} />
              </div>
              <div className="form-group" style={{ flex: 1 }}>
                <label>Unit</label>
                <input type="text" required value={formData.unit} onChange={e => setFormData({...formData, unit: e.target.value})} placeholder="Kg" />
              </div>
            </div>
            <div className="form-group" style={{ flex: 1 }}>
              <label>ESP32 LED Pin</label>
              <input type="number" required value={formData.ledPin} onChange={e => setFormData({...formData, ledPin: e.target.value})} placeholder="e.g. 2" />
            </div>
            <button type="submit" disabled={isSubmitting} className="btn">
                {isSubmitting ? 'Saving...' : (editingProductId ? 'Update Product' : 'Add Product')}
            </button>
            {editingProductId && (
              <button type="button" className="btn" style={{ marginLeft: '1rem', background: '#475569' }} onClick={() => {
                 setEditingProductId(null);
                 setFormData({ 
                    name: '', crops: '', y25: '', y26: '', aspiration: '', ledPin: '', unit: 'Kg'
                 });
              }}>Cancel Edit</button>
            )}
          </form>
        </div>
      )}
    </div>
  );
}
