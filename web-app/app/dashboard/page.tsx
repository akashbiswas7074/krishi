'use client';

import { useEffect, useState } from 'react';
import { io, Socket } from 'socket.io-client';
import { IProduct } from '@/models/Product';

let socket: Socket;
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
    name: '', crops: '', y25: '', y26: '', aspiration: '', fields: ''
  });
  const [imageFile, setImageFile] = useState<File | null>(null);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [editingProductId, setEditingProductId] = useState<string | null>(null);

  useEffect(() => {
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
      socket.disconnect();
    };
  }, []);

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
      name: p.name, crops: p.crops, y25: p.y25.toString(), y26: p.y26.toString(), aspiration: p.aspiration.toString(), fields: p.fields.join(', ')
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
      socket.emit('productsUpdated');
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
      fields: formData.fields.split(',').map(s => s.trim())
    };
    if (imageUrl) payload.imageUrl = imageUrl;

    try {
      await fetch('/api/products', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      alert(editingProductId ? "Product Updated Successfully!" : "Product Added Successfully!");
      setFormData({ name: '', crops: '', y25: '', y26: '', aspiration: '', fields: '' });
      setImageFile(null);
      setEditingProductId(null);
      setTab('control');
      socket.emit('productsUpdated');
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
      
      <div style={{ display: 'flex', gap: '1rem', marginBottom: '2rem' }}>
        <button className="btn" style={{ background: tab==='control' ? 'var(--accent)' : 'transparent', border: '1px solid var(--glass-border)' }} onClick={() => setTab('control')}>Control Screens</button>
        <button className="btn" style={{ background: tab==='admin' ? 'var(--accent)' : 'transparent', border: '1px solid var(--glass-border)' }} onClick={() => setTab('admin')}>Add Product</button>
      </div>

      {tab === 'control' && (
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
                <div style={{ fontWeight: 'bold' }}>{p.name}</div>
                <div style={{ fontSize: '0.9rem', color: 'var(--text-muted)' }}>{p.crops}</div>
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '0.5rem' }}>
                <button 
                   className="btn" 
                   style={{ padding: '0.3rem 0.6rem', fontSize: '0.8rem' }} 
                   onClick={(e) => handleEdit(e, p)}>Edit</button>
                <button 
                   className="btn" 
                   style={{ padding: '0.3rem 0.6rem', fontSize: '0.8rem', background: '#ef4444' }} 
                   onClick={(e) => handleDelete(e, p.id)}>Delete</button>
              </div>
            </div>
          ))}
        </div>
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
            </div>
            <div className="form-group">
              <label>Hardware Fields (comma sep)</label>
              <input type="text" required value={formData.fields} onChange={e => setFormData({...formData, fields: e.target.value})} />
            </div>
            <button type="submit" disabled={isSubmitting} className="btn">
                {isSubmitting ? 'Saving...' : (editingProductId ? 'Update Product' : 'Add Product')}
            </button>
            {editingProductId && (
              <button type="button" className="btn" style={{ marginLeft: '1rem', background: '#475569' }} onClick={() => {
                 setEditingProductId(null);
                 setFormData({ name: '', crops: '', y25: '', y26: '', aspiration: '', fields: '' });
              }}>Cancel Edit</button>
            )}
          </form>
        </div>
      )}
    </div>
  );
}
