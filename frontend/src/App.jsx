import { useState, useEffect, useRef, useCallback } from 'react'
import SymbolSelector from './components/SymbolSelector'
import OrderBookDepth from './components/OrderBookDepth'
import TradeTape from './components/TradeTape'
import OrderEntryForm from './components/OrderEntryForm'
import './App.css'

const DEFAULT_SYMBOLS = ['AAPL', 'GOOGL', 'MSFT', 'AMZN', 'TSLA']
const WS_RECONNECT_DELAY_MS = 3000

// ─── SVG icons ────────────────────────────────────────────────────
const IconBook = () => (
  <svg className="card-title-icon" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5">
    <rect x="1" y="1" width="14" height="14" rx="2" />
    <line x1="5" y1="5" x2="11" y2="5" />
    <line x1="5" y1="8" x2="11" y2="8" />
    <line x1="5" y1="11" x2="9" y2="11" />
  </svg>
)
const IconTape = () => (
  <svg className="card-title-icon" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5">
    <path d="M2 4h12M2 8h8M2 12h10" strokeLinecap="round" />
  </svg>
)
const IconOrder = () => (
  <svg className="card-title-icon" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5">
    <path d="M8 1v14M3 6l5-5 5 5M3 10l5 5 5-5" strokeLinecap="round" strokeLinejoin="round" />
  </svg>
)
const IconChart = () => (
  <svg className="card-title-icon" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5">
    <polyline points="1,12 5,7 9,9 15,3" strokeLinecap="round" strokeLinejoin="round"/>
    <line x1="1" y1="15" x2="15" y2="15" strokeLinecap="round"/>
  </svg>
)

// ─── Clock hook ───────────────────────────────────────────────────
function useClock() {
  const [time, setTime] = useState(() => new Date().toLocaleTimeString())
  useEffect(() => {
    const id = setInterval(() => setTime(new Date().toLocaleTimeString()), 1000)
    return () => clearInterval(id)
  }, [])
  return time
}

// ─── Main App ─────────────────────────────────────────────────────
function App() {
  const [selectedSymbol, setSelectedSymbol] = useState('AAPL')
  const [symbols, setSymbols]     = useState(DEFAULT_SYMBOLS)
  const [wsConnected, setWsConnected] = useState(false)
  const [orderBook, setOrderBook] = useState({ bids: [], asks: [] })
  const [trades, setTrades]       = useState([])
  const [simRunning, setSimRunning] = useState(false)
  const [latencyMs, setLatencyMs] = useState(null)
  const [orderCount, setOrderCount] = useState(0)
  const [tradeCount, setTradeCount] = useState(0)

  const wsRef         = useRef(null)
  const simRef        = useRef(null)
  const simPriceRef   = useRef({})
  const latencyHist   = useRef([])
  const clock         = useClock()

  // ── Derived market stats ──────────────────────────────────────
  const lastTrade = trades[0]
  const prevTrade = trades[1]
  const lastPrice = lastTrade?.price
  const priceDir  = lastPrice != null && prevTrade?.price != null
    ? lastPrice > prevTrade.price ? 'up' : lastPrice < prevTrade.price ? 'down' : 'flat'
    : 'flat'

  const bestBid = orderBook.bids[0]?.price
  const bestAsk = orderBook.asks[0]?.price
  const spread  = bestBid != null && bestAsk != null ? (bestAsk - bestBid).toFixed(2) : '—'
  const midPrice = bestBid != null && bestAsk != null ? ((bestBid + bestAsk) / 2).toFixed(2) : '—'
  const totalBidQty = orderBook.bids.reduce((s, l) => s + (l.quantity || 0), 0)
  const totalAskQty = orderBook.asks.reduce((s, l) => s + (l.quantity || 0), 0)
  const imbalance = totalBidQty + totalAskQty > 0
    ? ((totalBidQty - totalAskQty) / (totalBidQty + totalAskQty) * 100).toFixed(1)
    : '0.0'
  const imbalanceDir = parseFloat(imbalance) > 0 ? 'up' : parseFloat(imbalance) < 0 ? 'down' : 'flat'

  // ── REST helpers ──────────────────────────────────────────────
  const fetchOrderBook = useCallback(async (symbol) => {
    try {
      const res = await fetch(`/api/orderbook/${symbol}`)
      if (!res.ok) return
      const data = await res.json()
      setOrderBook(data)
    } catch { }
  }, [])

  const fetchTrades = useCallback(async (symbol) => {
    try {
      const res = await fetch(`/api/trades/${symbol}?limit=50`)
      if (!res.ok) return
      const data = await res.json()
      setTrades(Array.isArray(data) ? data : [])
    } catch { }
  }, [])

  const fetchSymbols = useCallback(async () => {
    try {
      const res = await fetch('/api/symbols')
      if (!res.ok) return
      const data = await res.json()
      if (Array.isArray(data) && data.length > 0) setSymbols(data)
    } catch { }
  }, [])

  // ── WebSocket — FIXED: use relative URL through Vite proxy ────
  useEffect(() => {
    let reconnectTimer

    const connect = () => {
      const wsProto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
      const ws = new WebSocket(`${wsProto}//${window.location.host}/ws`)
      wsRef.current = ws

      ws.onopen = () => {
        setWsConnected(true)
        ws.send(JSON.stringify({ subscribe: selectedSymbol }))
      }
      ws.onclose = () => {
        setWsConnected(false)
        reconnectTimer = setTimeout(connect, WS_RECONNECT_DELAY_MS)
      }
      ws.onerror = () => { }
      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data)
          if (msg.type === 'trade' && msg.symbol === selectedSymbol) {
            setTrades(prev => [msg, ...prev].slice(0, 50))
            setTradeCount(c => c + 1)
          } else if (msg.type === 'orderbook_update' && msg.symbol === selectedSymbol) {
            fetchOrderBook(selectedSymbol)
          }
        } catch { }
      }
    }

    connect()
    return () => {
      clearTimeout(reconnectTimer)
      wsRef.current?.close()
    }
  }, [selectedSymbol, fetchOrderBook])

  // ── Initial data ──────────────────────────────────────────────
  useEffect(() => {
    fetchSymbols()
    fetchOrderBook(selectedSymbol)
    fetchTrades(selectedSymbol)
  }, []) // eslint-disable-line

  // ── Symbol switch ─────────────────────────────────────────────
  const handleSymbolChange = useCallback((symbol) => {
    setSelectedSymbol(symbol)
    setOrderBook({ bids: [], asks: [] })
    setTrades([])
    fetchOrderBook(symbol)
    fetchTrades(symbol)
  }, [fetchOrderBook, fetchTrades])

  // ── Order submit with latency tracking ───────────────────────
  const handleOrderSubmit = useCallback(async (orderData) => {
    const t0 = performance.now()
    try {
      const res = await fetch('/api/orders', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ...orderData, symbol: selectedSymbol }),
      })
      const latency = performance.now() - t0
      latencyHist.current = [...latencyHist.current.slice(-99), latency]
      const sorted = [...latencyHist.current].sort((a, b) => a - b)
      const p50 = sorted[Math.floor(sorted.length * 0.5)]
      setLatencyMs(Math.round(p50 * 10) / 10)
      setOrderCount(c => c + 1)

      const data = await res.json()
      if (res.ok) {
        fetchOrderBook(selectedSymbol)
        fetchTrades(selectedSymbol)
        return { success: true, data }
      }
      return { success: false, error: data.error || 'Order rejected' }
    } catch (err) {
      return { success: false, error: err.message }
    }
  }, [selectedSymbol, fetchOrderBook, fetchTrades])

  // ── Market Simulator ──────────────────────────────────────────
  const submitSimOrder = useCallback(async (sym, side, type, price, qty) => {
    try {
      const body = { symbol: sym, side, type, quantity: qty }
      if (type === 'limit') body.price = price
      await fetch('/api/orders', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      })
    } catch { }
  }, [])

  const BASE_PRICES = { AAPL: 182, GOOGL: 142, MSFT: 378, AMZN: 185, TSLA: 248 }

  const runSimTick = useCallback(() => {
    for (const sym of DEFAULT_SYMBOLS) {
      if (!simPriceRef.current[sym]) {
        simPriceRef.current[sym] = BASE_PRICES[sym] || 100
      }
      simPriceRef.current[sym] = Math.max(1, simPriceRef.current[sym] + (Math.random() - 0.5) * 0.4)
      const mid = simPriceRef.current[sym]
      const spread = mid * 0.0005

      for (let i = 0; i < Math.floor(Math.random() * 3) + 2; i++) {
        const isBuy = Math.random() > 0.5
        const offset = Math.random() * spread * 3 + spread * 0.5
        const price  = parseFloat((isBuy ? mid - offset : mid + offset).toFixed(2))
        const qty    = Math.floor(Math.random() * 200) + 10
        if (price > 0) submitSimOrder(sym, isBuy ? 'buy' : 'sell', 'limit', price, qty)
      }

      if (Math.random() < 0.25) {
        const isBuy = Math.random() > 0.5
        submitSimOrder(sym, isBuy ? 'buy' : 'sell', 'market', null, Math.floor(Math.random() * 50) + 5)
      }
    }
  }, [submitSimOrder])

  const toggleSim = useCallback(() => {
    if (simRunning) {
      clearInterval(simRef.current)
      simRef.current = null
      setSimRunning(false)
    } else {
      runSimTick()
      simRef.current = setInterval(runSimTick, 700)
      setSimRunning(true)
    }
  }, [simRunning, runSimTick])

  useEffect(() => () => { if (simRef.current) clearInterval(simRef.current) }, [])

  // ── Render ────────────────────────────────────────────────────
  return (
    <div className="app">
      {/* ── Navbar ── */}
      <nav className="navbar" role="banner">
        <div className="navbar-brand">
          <div className="navbar-logo" aria-hidden="true">ME</div>
          <div>
            <div className="navbar-title">Mini Exchange</div>
            <div className="navbar-subtitle">C++20 High-Performance Matching Engine · ~1.2M orders/sec · p99 &lt;1.2µs</div>
          </div>
        </div>

        <div className="navbar-meta">
          {latencyMs !== null && (
            <div className="latency-pill" title="p50 REST round-trip latency">
              <span className="latency-label">REST p50</span>
              <span className="latency-value">{latencyMs}ms</span>
            </div>
          )}
          <div className="stats-pill">
            <span>{orderCount.toLocaleString()} orders</span>
            <span className="stats-sep">·</span>
            <span style={{ color: 'var(--green-base)' }}>{tradeCount} trades</span>
          </div>
          <button
            id="sim-toggle-btn"
            className={`sim-btn${simRunning ? ' running' : ''}`}
            onClick={toggleSim}
            title={simRunning ? 'Stop market simulator' : 'Start market simulator (auto-generates live orders)'}
          >
            <span className="sim-dot" />
            {simRunning ? 'Sim ON' : 'Sim OFF'}
          </button>
          <span className="navbar-time">{clock}</span>
          <div
            id="connection-status"
            className={`conn-pill ${wsConnected ? 'connected' : 'disconnected'}`}
            role="status"
            aria-live="polite"
          >
            <span className="conn-dot" />
            {wsConnected ? 'Live' : 'Offline'}
          </div>
        </div>
      </nav>

      {/* ── Main ── */}
      <main className="main-content">
        <SymbolSelector symbols={symbols} selectedSymbol={selectedSymbol} onSymbolChange={handleSymbolChange} />

        {/* Ticker row */}
        <div className="ticker-row" aria-label="Market stats">
          {[
            { label: 'Symbol',    value: selectedSymbol,           cls: '',       mono: true },
            { label: 'Last',      value: lastPrice?.toFixed(2) || '—', cls: priceDir },
            { label: 'Mid',       value: midPrice,                 cls: '' },
            { label: 'Best Bid',  value: bestBid?.toFixed(2) || '—', cls: 'up' },
            { label: 'Best Ask',  value: bestAsk?.toFixed(2) || '—', cls: 'down' },
            { label: 'Spread',    value: spread,                   cls: '',       gold: true },
            { label: 'OFI',       value: `${imbalance}%`,          cls: imbalanceDir },
            { label: 'Bid Qty',   value: orderBook.bids[0]?.quantity?.toLocaleString() || '—', cls: '' },
            { label: 'Ask Qty',   value: orderBook.asks[0]?.quantity?.toLocaleString() || '—', cls: '' },
            { label: 'Trades',    value: trades.length,            cls: '' },
          ].map(({ label, value, cls, mono, gold }) => (
            <div className="ticker-stat" key={label}>
              <span className="ticker-label">{label}</span>
              <span
                className={`ticker-value${cls ? ' ' + cls : ''}`}
                style={{
                  fontFamily: (mono || !cls) ? 'var(--font-mono)' : undefined,
                  color: gold ? 'var(--gold-base)' : undefined,
                }}
              >
                {value}
              </span>
            </div>
          ))}
        </div>

        {/* Dashboard grid */}
        <div className="dashboard-grid">
          {/* Order Book */}
          <section className="card card-orderbook" aria-labelledby="ob-title">
            <div className="card-header">
              <div className="card-title" id="ob-title"><IconBook /> Order Book</div>
              <span className="card-badge live">Live</span>
            </div>
            <div className="card-body">
              <OrderBookDepth orderBook={orderBook} />
            </div>
          </section>

          {/* Left Column */}
          <div className="left-column">
            {/* Depth Chart */}
            <section className="card card-depth" aria-labelledby="depth-title">
              <div className="card-header">
                <div className="card-title" id="depth-title"><IconChart /> Market Depth</div>
                <span className="card-badge live">Live</span>
              </div>
              <div className="card-body">
                <DepthChart orderBook={orderBook} />
              </div>
            </section>

            {/* Trade Tape */}
            <section className="card card-tradetape" aria-labelledby="tape-title">
              <div className="card-header">
                <div className="card-title" id="tape-title"><IconTape /> Trade Tape</div>
                <span className="card-badge live">Live</span>
              </div>
              <div className="card-body">
                <TradeTape trades={trades} />
              </div>
            </section>
          </div>

          {/* Order Entry */}
          <section className="card col-order-entry" aria-labelledby="oe-title">
            <div className="card-header">
              <div className="card-title" id="oe-title"><IconOrder /> Order Entry</div>
            </div>
            <div className="card-body">
              <OrderEntryForm onSubmit={handleOrderSubmit} selectedSymbol={selectedSymbol} />
            </div>
          </section>
        </div>
      </main>
    </div>
  )
}

// ─── Depth Chart (SVG, inline) ─────────────────────────────────────
function DepthChart({ orderBook }) {
  const { bids = [], asks = [] } = orderBook

  const W = 560, H = 140
  const P = { top: 20, right: 16, bottom: 28, left: 52 }

  const cumBids = []
  let c = 0
  for (const l of bids.slice(0, 20)) { c += l.quantity || 0; cumBids.push({ price: l.price, qty: c }) }

  const cumAsks = []
  c = 0
  for (const l of asks.slice(0, 20)) { c += l.quantity || 0; cumAsks.push({ price: l.price, qty: c }) }

  if (!cumBids.length && !cumAsks.length) {
    return (
      <div className="depth-empty">
        No orders yet — click &quot;Sim ON&quot; or submit orders to see depth
      </div>
    )
  }

  const prices = [...cumBids.map(b => b.price), ...cumAsks.map(a => a.price)].filter(Boolean)
  const qtys   = [...cumBids.map(b => b.qty),   ...cumAsks.map(a => a.qty)].filter(Boolean)
  if (!prices.length) return null

  const minP = Math.min(...prices), maxP = Math.max(...prices)
  const maxQ = Math.max(...qtys, 1)
  const pW   = W - P.left - P.right
  const pH   = H - P.top  - P.bottom

  const toX = p => P.left + ((p - minP) / (maxP - minP || 1)) * pW
  const toY = q => P.top  + pH - (q / maxQ) * pH

  const stepPath = (pts, isBid) => {
    if (!pts.length) return ''
    const s = isBid ? [...pts].sort((a,b) => b.price - a.price) : [...pts].sort((a,b) => a.price - b.price)
    let d = `M ${toX(s[0].price)} ${pH + P.top} L ${toX(s[0].price)} ${toY(s[0].qty)}`
    for (let i = 1; i < s.length; i++) {
      d += ` L ${toX(s[i].price)} ${toY(s[i-1].qty)} L ${toX(s[i].price)} ${toY(s[i].qty)}`
    }
    d += ` L ${toX(s.at(-1).price)} ${pH + P.top} Z`
    return d
  }

  const yTicks = [0, 0.5, 1].map(f => ({ q: Math.round(maxQ * f), y: toY(maxQ * f) }))

  return (
    <svg viewBox={`0 0 ${W} ${H + P.top + P.bottom}`} style={{ width: '100%', height: 'auto', display: 'block' }}
      role="img" aria-label="Bid/Ask cumulative depth chart">
      <defs>
        <linearGradient id="bidG" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stopColor="#23c55e" stopOpacity="0.35"/>
          <stop offset="100%" stopColor="#23c55e" stopOpacity="0.04"/>
        </linearGradient>
        <linearGradient id="askG" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stopColor="#ef4444" stopOpacity="0.35"/>
          <stop offset="100%" stopColor="#ef4444" stopOpacity="0.04"/>
        </linearGradient>
      </defs>

      {yTicks.map((t, i) => (
        <g key={i}>
          <line x1={P.left} y1={t.y} x2={W - P.right} y2={t.y} stroke="rgba(255,255,255,0.05)" strokeWidth="1"/>
          <text x={P.left - 6} y={t.y + 4} fill="rgba(139,148,158,0.7)" fontSize="9"
            textAnchor="end" fontFamily="'JetBrains Mono',monospace">{t.q.toLocaleString()}</text>
        </g>
      ))}

      {cumBids.length > 0 && <path d={stepPath(cumBids, true)} fill="url(#bidG)"/>}
      {cumAsks.length > 0 && <path d={stepPath(cumAsks, false)} fill="url(#askG)"/>}

      {cumBids.length > 0 && (() => {
        const s = [...cumBids].sort((a,b) => b.price - a.price)
        let d = `M ${toX(s[0].price)} ${toY(s[0].qty)}`
        for (let i = 1; i < s.length; i++) d += ` L ${toX(s[i].price)} ${toY(s[i-1].qty)} L ${toX(s[i].price)} ${toY(s[i].qty)}`
        return <path d={d} fill="none" stroke="#23c55e" strokeWidth="1.5"/>
      })()}
      {cumAsks.length > 0 && (() => {
        const s = [...cumAsks].sort((a,b) => a.price - b.price)
        let d = `M ${toX(s[0].price)} ${toY(s[0].qty)}`
        for (let i = 1; i < s.length; i++) d += ` L ${toX(s[i].price)} ${toY(s[i-1].qty)} L ${toX(s[i].price)} ${toY(s[i].qty)}`
        return <path d={d} fill="none" stroke="#ef4444" strokeWidth="1.5"/>
      })()}

      {cumBids.length > 0 && cumAsks.length > 0 && (() => {
        const mid = (cumBids[0].price + cumAsks[0].price) / 2
        const x = toX(mid)
        return <>
          <line x1={x} y1={P.top} x2={x} y2={H + P.top - P.bottom}
            stroke="rgba(245,158,11,0.55)" strokeWidth="1" strokeDasharray="3 2"/>
          <text x={x} y={P.top - 5} fill="#f59e0b" fontSize="9" textAnchor="middle"
            fontFamily="'JetBrains Mono',monospace">MID {mid.toFixed(2)}</text>
        </>
      })()}

      {[cumBids[0], cumAsks.at(-1)].filter(Boolean).map((pt, i) => (
        <text key={i} x={toX(pt.price)} y={H + P.top + P.bottom - 4}
          fill="rgba(139,148,158,0.65)" fontSize="9" textAnchor="middle"
          fontFamily="'JetBrains Mono',monospace">
          {pt.price?.toFixed(2)}
        </text>
      ))}

      <g>
        <rect x={P.left + 2} y={P.top + 4} width={8} height={8} rx="1.5" fill="#23c55e" opacity="0.7"/>
        <text x={P.left + 14} y={P.top + 12} fill="rgba(139,148,158,0.8)" fontSize="9"
          fontFamily="'JetBrains Mono',monospace">Bids</text>
        <rect x={P.left + 50} y={P.top + 4} width={8} height={8} rx="1.5" fill="#ef4444" opacity="0.7"/>
        <text x={P.left + 62} y={P.top + 12} fill="rgba(139,148,158,0.8)" fontSize="9"
          fontFamily="'JetBrains Mono',monospace">Asks</text>
      </g>
    </svg>
  )
}

export default App
