import Link from 'next/link';

export default function Home() {
  return (
    <div className="screen-container fade-in">
      <h1 className="text-gradient" style={{ fontSize: '3rem', marginBottom: '2rem' }}>Agrarian Diorama Control</h1>
      <div style={{ display: 'flex', justifyContent: 'center' }}>
        <Link href="/dashboard" className="btn" style={{ padding: '1rem 2rem', fontSize: '1.2rem' }}>Launch Remote Control</Link>
      </div>
    </div>
  );
}
