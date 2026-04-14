'use client';

import React, { useEffect, useState } from 'react';
import { io, Socket } from 'socket.io-client';
import { IProduct } from '@/models/Product';
import { signOut, useSession } from 'next-auth/react';

let socket: Socket | null = null;

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
  const [tab, setTab] = useState<'control' | 'admin' | 'admin_manage'>('control');
  const [users, setUsers] = useState<any[]>([]);
  
  const { data: session } = useSession();
  const isAdmin = (session?.user as any)?.role === 'admin';

  const [formData, setFormData] = useState({
    name: '', crops: '', y25: '', y26: '', aspiration: '', ledPin: '', ledPins2: '', cropPins: [{ cropName: '', pin: '' }], unit: 'Kg'
  });
  const [imageFile, setImageFile] = useState<File | null>(null);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [editingProductId, setEditingProductId] = useState<string | null>(null);
  const [isSlideshowActive, setIsSlideshowActive] = useState(true);
  const [mounted, setMounted] = useState(false);
  const [hardwareStatus, setHardwareStatus] = useState<any>(null);

  useEffect(() => {
    setMounted(true);
  }, []);

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

  const fetchUsers = async () => {
    if (!isAdmin) return;
    try {
      const res = await fetch('/api/admin/users');
      if (res.ok) {
        const data = await res.json();
        setUsers(data);
      }
    } catch (err) {
      console.error('Failed to fetch users', err);
    }
  };

  useEffect(() => {
    const fetchStatus = async () => {
      try {
        const res = await fetch('/api/active-product');
        if (res.ok) {
          const data = await res.json();
          setIsSlideshowActive(data.isSlideshowActive);
          if (data.status === 'fixed' && data.focusedProduct) {
            setActiveProduct(data.focusedProduct);
          }
        }
      } catch (err) {
        console.error('Failed to fetch slideshow status', err);
      }
    };

    fetchProducts();
    fetchStatus();

    // Socket.io only works in local / custom server environment
    if (window.location.hostname === 'localhost') {
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

      socket.on('slideshowStatus', (status) => {
        setIsSlideshowActive(status);
      });

      socket.on('liveHardwareStatus', (status) => {
        setHardwareStatus(status);
      });
    }

    return () => {
      if (socket) socket.disconnect();
    };
  }, [tab, isAdmin]);

  // Auto-resume slideshow on first dashboard load to sync hardware
  useEffect(() => {
    resumeSlideshow();
  }, []);

  useEffect(() => {
    if (tab === 'admin_manage' && isAdmin) {
      fetchUsers();
    }
  }, [tab, isAdmin]);

  const toggleActiveProduct = async (p: IProduct) => {
    const newStatus = !p.isActive;
    try {
      const res = await fetch('/api/products', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ...p, isActive: newStatus })
      });
      if (res.ok) {
        // Update local state immediately for Vercel/Serverless support
        setProducts(prev => prev.map(prod => prod.id === p.id ? { ...prod, isActive: newStatus } : prod) as any);
        if (socket) socket.emit('productsUpdated');
      }
    } catch (err) {
      console.error('Failed to toggle product status:', err);
      alert("Failed to update status. Please check your connection.");
    }
  };

  const selectProduct = async (id: string) => {
    // Optimistically update local active product for instant feedback (Vercel support)
    const selected = products.find(p => p.id === id);
    if (selected) {
      setActiveProduct(selected);
      setIsSlideshowActive(false); // Lock the view
    }

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

  const resumeSlideshow = async () => {
    setIsSlideshowActive(true);
    try {
      await fetch('/api/active-product', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action: 'resume' })
      });
      if (socket) socket.emit('resumeSlideshow'); // Specific resume signal for local server
    } catch (err) {
      console.error('Failed to resume slideshow:', err);
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
      ledPins2: p.ledPins2?.join(', ') || '',
      cropPins: p.cropPins && p.cropPins.length > 0 ? p.cropPins.map(cp => ({ cropName: cp.cropName, pin: cp.pin.toString() })) : [{ cropName: '', pin: '' }],
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

  const deleteUser = async (id: string) => {
    if (!confirm('Are you sure you want to delete this user?')) return;
    try {
      const res = await fetch('/api/admin/users', {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id })
      });
      if (res.ok) fetchUsers();
    } catch (err) { console.error(err); }
  };

  const toggleUserRole = async (id: string, currentRole: string) => {
    const newRole = currentRole === 'admin' ? 'user' : 'admin';
    try {
      const res = await fetch('/api/admin/users', {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id, role: newRole })
      });
      if (res.ok) fetchUsers();
    } catch (err) { console.error(err); }
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
        setIsSubmitting(false);
        return;
      }
    }

    const payload: any = {
      id: editingProductId || ('prod_' + Date.now()),
      name: formData.name,
      y25: Number(formData.y25),
      y26: Number(formData.y26),
      aspiration: Number(formData.aspiration),
      ledPin: isNaN(Number(formData.ledPin)) ? 0 : Number(formData.ledPin),
      ledPins2: formData.cropPins.map(cp => Number(cp.pin)).filter(n => !isNaN(n) && n > 0),
      cropPins: formData.cropPins.map(cp => ({ cropName: cp.cropName, pin: Number(cp.pin) })),
      crops: formData.cropPins.map(cp => cp.cropName).filter(n => n.trim() !== '').join(' / '),
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
      setFormData({ name: '', crops: '', y25: '', y26: '', aspiration: '', ledPin: '', ledPins2: '', cropPins: [{ cropName: '', pin: '' }], unit: 'Kg' });
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

  if (!mounted) return null;

  return (
    <div className="dashboard-container fade-in">
      <div className="header-actions">
        <h1 className="text-gradient">Master Remote Control</h1>
        <div style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
          <div style={{ color: 'var(--text-muted)', fontSize: '0.9rem' }}>
            Logged in as <span style={{ color: 'var(--accent)', fontWeight: 'bold' }}>{session?.user?.name}</span> ({isAdmin ? 'Admin' : 'User'})
          </div>
          <button className="btn" style={{ background: '#ef4444' }} onClick={() => signOut()}>Sign Out</button>
        </div>
      </div>
      
      <div style={{ display: 'flex', gap: '1rem', marginBottom: '2rem', alignItems: 'center', flexWrap: 'wrap' }}>
        <button className="btn" style={{ background: tab==='control' ? 'var(--accent)' : 'transparent', border: '1px solid var(--glass-border)' }} onClick={() => setTab('control')}>Control Screens</button>
        <button className="btn" style={{ background: tab==='admin' ? 'var(--accent)' : 'transparent', border: '1px solid var(--glass-border)' }} onClick={() => setTab('admin')}>Add Product</button>
        {isAdmin && (
          <button className="btn" style={{ background: tab==='admin_manage' ? 'var(--accent)' : 'transparent', border: '1px solid var(--glass-border)' }} onClick={() => setTab('admin_manage')}>Admin Manage</button>
        )}
        
        <button 
          className="btn" 
          style={{ 
            background: isSlideshowActive ? 'var(--success)' : 'transparent',
            border: isSlideshowActive ? 'none' : '1px solid var(--accent)',
            marginLeft: 'auto'
          }}
          onClick={resumeSlideshow}
        >
          {isSlideshowActive ? '● Slideshow: ON' : '▶ Resume Slideshow'}
        </button>
      </div>

      {tab === 'control' && (
        <>
          <div className="preview-container" style={{ display: 'flex', gap: '1.5rem', marginBottom: '3rem', flexWrap: 'wrap', justifyContent: 'center' }}>
             {/* Screen 1 Preview */}
             <div className="virtual-tft s1">
                <div className="scanline-overlay" style={{ background: 'rgba(0,0,0,0.05)' }}></div>
                <div style={{ position: 'absolute', top: 5, right: 10, fontSize: '0.6rem', color: '#94a3b8', fontWeight: 'bold', zIndex: 20 }}>SCREEN 1 - IMAGE</div>
                <div className="tft-content tft-animate" key={activeProduct?.id + '_s1'}>
                  {activeProduct?.imageUrl ? (
                    <img src={activeProduct.imageUrl} style={{ width: '100%', height: '100%', objectFit: 'contain', background: 'transparent' }} alt="preview" />
                  ) : (
                    <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#cbd5e1' }}>No Image</div>
                  )}
                </div>
             </div>
 
             {/* Screen 2 Preview */}
             <div className="virtual-tft" style={{ color: '#fff', fontFamily: 'monospace' }}>
                <div className="scanline-overlay"></div>
                <div style={{ position: 'absolute', bottom: 5, right: 10, fontSize: '0.6rem', color: '#64748b', fontWeight: 'bold', zIndex: 20 }}>SCREEN 2 - DETAILS</div>
                <div className="tft-content tft-animate" key={activeProduct?.id + '_s2'}>
                  <div style={{ background: '#182828', height: '50px', padding: '10px', display: 'flex', alignItems: 'center' }}>
                    <div style={{ 
                      fontSize: (activeProduct?.name?.length || 0) > 14 ? '1rem' : '1.5rem', 
                      fontWeight: 'bold', 
                      overflow: 'hidden', 
                      whiteSpace: 'nowrap', 
                      textOverflow: 'ellipsis' 
                    }}>
                      {activeProduct?.name || 'Waiting...'}
                    </div>
                  </div>
                  <div style={{ padding: '10px 15px 25px 15px', height: '190px', display: 'flex', flexDirection: 'column' }}>
                    <div style={{ color: '#00FF00', fontSize: (activeProduct?.crops?.length || 0) > 20 ? '0.8rem' : '1.2rem', marginBottom: '8px' }}>
                      Crops: <span style={{ color: '#fff' }}>{activeProduct?.crops || '-'}</span>
                    </div>
                    
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', flex: 1 }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', borderBottom: '1px solid #1e293b', paddingBottom: '4px' }}>
                          <div style={{ color: '#2563eb', fontWeight: 'bold', fontSize: '0.9rem' }}>2025-26 Sales:</div>
                          <div style={{ fontSize: '1.2rem' }}>{activeProduct?.y25?.toLocaleString() || 0} <span style={{fontSize: '0.7rem'}}>{activeProduct?.unit}</span></div>
                        </div>
                        
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', borderBottom: '1px solid #1e293b', paddingBottom: '4px' }}>
                          <div style={{ color: '#06b6d4', fontWeight: 'bold', fontSize: '0.9rem' }}>2026-2027 Sales:</div>
                          <div style={{ fontSize: '1.2rem' }}>{activeProduct?.y26?.toLocaleString() || 0} <span style={{fontSize: '0.7rem'}}>{activeProduct?.unit}</span></div>
                        </div>
  
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginTop: 'auto', paddingTop: '10px', borderTop: '1px solid #334155' }}>
                          <div style={{ color: '#22c55e', fontWeight: 'bold' }}>ASPIRATION TARGET:</div>
                          <div style={{ fontSize: '1.2rem', fontWeight: 'bold' }}>{activeProduct?.aspiration?.toLocaleString() || 0} <span style={{fontSize: '0.7rem'}}>{activeProduct?.unit}</span></div>
                        </div>
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
              onClick={() => {
                if (p.isActive) selectProduct(p.id);
                else alert("Product must be marked as 'In Display' to show on screen.");
              }}
              style={{ position: 'relative' }}
            >
              {p.isActive && (
                <div style={{ position: 'absolute', top: -5, left: -5, width: 12, height: 12, background: '#22c55e', borderRadius: '50%', border: '2px solid #fff', boxShadow: '0 0 8px #22c55e' }}></div>
              )}
              {p.imageUrl ? 
                <img src={p.imageUrl} alt="product" /> : 
                <div style={{ width: 60, height: 60, background: '#334155', borderRadius: 8 }}></div>
              }
              <div style={{ flex: 1 }}>
                <div style={{ fontWeight: 'bold' }}>{p.name}</div>
                <div style={{ fontSize: '0.8rem', color: 'var(--text-muted)' }}>
                  <span style={{ color: '#60a5fa' }}>5V: {p.ledPin}</span> | 
                  <span style={{ color: '#fbbf24' }}> 12V: [{p.ledPins2?.join(', ')}]</span> | 
                  {p.crops}
                </div>
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '0.5rem', minWidth: '100px' }}>
                <button 
                  className="btn" 
                  style={{ 
                    padding: '0.3rem 0.6rem', 
                    fontSize: '0.8rem', 
                    background: p.isActive ? '#22c55e' : '#475569',
                    border: p.isActive ? '1px solid #166534' : '1px solid var(--glass-border)',
                    color: p.isActive ? '#fff' : '#94a3b8'
                  }} 
                  onClick={(e) => { e.stopPropagation(); toggleActiveProduct(p); }}
                >
                  {p.isActive ? '✓ IN DISPLAY' : '✕ HIDDEN'}
                </button>
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
        <div className="glass-panel" style={{ maxWidth: '600px', margin: '0 auto' }}>
          <form onSubmit={submitAdminForm}>
            <div className="form-group">
              <label>Product Name</label>
              <input type="text" required value={formData.name} onChange={e => setFormData({...formData, name: e.target.value})} />
            </div>
            <div className="form-group">
              <label>Crop Mapping (Name & Pin)</label>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '0.8rem', background: 'rgba(255,255,255,0.03)', padding: '1rem', borderRadius: '8px', border: '1px solid var(--glass-border)' }}>
                {formData.cropPins.map((cp, idx) => (
                  <div key={idx} style={{ display: 'flex', gap: '0.5rem', alignItems: 'center' }}>
                    <input 
                      type="text" 
                      placeholder="Crop Name" 
                      style={{ flex: 2, marginBottom: 0 }} 
                      value={cp.cropName} 
                      onChange={e => {
                        const newCP = [...formData.cropPins];
                        newCP[idx].cropName = e.target.value;
                        setFormData({...formData, cropPins: newCP});
                      }}
                    />
                    <input 
                      type="number" 
                      placeholder="Pin" 
                      style={{ flex: 1, marginBottom: 0 }} 
                      value={cp.pin} 
                      onChange={e => {
                        const newCP = [...formData.cropPins];
                        newCP[idx].pin = e.target.value;
                        setFormData({...formData, cropPins: newCP});
                      }}
                    />
                    {formData.cropPins.length > 1 && (
                      <button 
                        type="button" 
                        style={{ background: '#ef4444', padding: '0.5rem', minWidth: '40px' }} 
                        className="btn"
                        onClick={() => {
                          const newCP = formData.cropPins.filter((_, i) => i !== idx);
                          setFormData({...formData, cropPins: newCP});
                        }}
                      >✕</button>
                    )}
                  </div>
                ))}
                <button 
                  type="button" 
                  className="btn" 
                  style={{ background: 'transparent', border: '1px dashed var(--accent)', color: 'var(--accent)', marginTop: '0.5rem' }}
                  onClick={() => setFormData({...formData, cropPins: [...formData.cropPins, { cropName: '', pin: '' }]})}
                >
                  + Add Another Crop
                </button>
              </div>
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
            <div style={{ display: 'flex', gap: '1rem' }}>
              <div className="form-group" style={{ flex: 1 }}>
                <label>ESP32 LED Pin (5V)</label>
                <input type="number" required value={formData.ledPin} onChange={e => setFormData({...formData, ledPin: e.target.value})} placeholder="e.g. 2" />
              </div>
            </div>
            <button type="submit" disabled={isSubmitting} className="btn">
                {isSubmitting ? 'Saving...' : (editingProductId ? 'Update Product' : 'Add Product')}
            </button>
            {editingProductId && (
              <button type="button" className="btn" style={{ marginLeft: '1rem', background: '#475569' }} onClick={() => {
                 setEditingProductId(null);
                 setFormData({ name: '', crops: '', y25: '', y26: '', aspiration: '', ledPin: '', ledPins2: '', cropPins: [{ cropName: '', pin: '' }], unit: 'Kg' });
              }}>Cancel Edit</button>
            )}
          </form>
        </div>
      )}

      {tab === 'admin_manage' && isAdmin && (
        <div className="glass-panel fade-in">
          <h2 style={{ marginBottom: '1.5rem', color: 'var(--accent)' }}>User Management ({users.length})</h2>
          <div style={{ overflowX: 'auto' }}>
            <table style={{ width: '100%', borderCollapse: 'collapse' }}>
              <thead>
                <tr style={{ borderBottom: '1px solid var(--glass-border)', textAlign: 'left' }}>
                  <th style={{ padding: '1rem' }}>Username</th>
                  <th style={{ padding: '1rem' }}>Role</th>
                  <th style={{ padding: '1rem' }}>Joined</th>
                  <th style={{ padding: '1rem' }}>Actions</th>
                </tr>
              </thead>
              <tbody>
                {users.map(u => (
                  <tr key={u._id} style={{ borderBottom: '1px solid rgba(255,255,255,0.05)' }}>
                    <td style={{ padding: '1rem' }}>{u.username}</td>
                    <td style={{ padding: '1rem' }}>
                      <span style={{ 
                        padding: '0.2rem 0.5rem', 
                        borderRadius: '4px', 
                        fontSize: '0.8rem',
                        background: u.role === 'admin' ? 'rgba(34, 197, 94, 0.2)' : 'rgba(148, 163, 184, 0.2)',
                        color: u.role === 'admin' ? '#4ade80' : '#94a3b8'
                      }}>
                        {u.role.toUpperCase()}
                      </span>
                    </td>
                    <td style={{ padding: '1rem', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
                      {new Date(u.createdAt).toLocaleDateString()}
                    </td>
                    <td style={{ padding: '1rem' }}>
                      <div style={{ display: 'flex', gap: '0.5rem' }}>
                        <button 
                          className="btn" 
                          style={{ padding: '0.3rem 0.6rem', fontSize: '0.8rem' }}
                          onClick={() => toggleUserRole(u._id, u.role)}
                        >
                          {u.role === 'admin' ? 'Demote' : 'Promote'}
                        </button>
                        {u.username !== session?.user?.name && (
                          <button 
                            className="btn" 
                            style={{ padding: '0.3rem 0.6rem', fontSize: '0.8rem', background: '#ef4444' }}
                            onClick={() => deleteUser(u._id)}
                          >
                            Delete
                          </button>
                        )}
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
}
