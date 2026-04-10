import Link from 'next/link';

export default function Home() {
  return (
    <div className="screen-container fade-in">
      <h1 className="text-gradient" style={{ fontSize: '3rem', marginBottom: '2rem' }}>Agrarian Diorama Control</h1>
      <div style={{ display: 'flex', gap: '1rem' }}>
        <Link href="/dashboard" className="btn">Launch Remote Control</Link>
        <Link href="/screen1" className="btn" style={{ background: '#475569' }} target="_blank">Display Screen 1</Link>
        <Link href="/screen2" className="btn" style={{ background: '#475569' }} target="_blank">Display Screen 2</Link>
      </div>
    </div>
  );
}
