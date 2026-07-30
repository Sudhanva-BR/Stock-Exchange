import { useState, useEffect, useRef, useCallback, useMemo } from 'react'
import WatchlistStrip from './components/WatchlistStrip'
import CandleChart from './components/CandleChart'
import OrderBookDepth from './components/OrderBookDepth'
import TradeTape from './components/TradeTape'
import OrderEntryForm from './components/OrderEntryForm'
import OBIPanel from './components/OBIPanel'
import LatencyDashboard from './components/LatencyDashboard'
import MemoryPoolPanel from './components/MemoryPoolPanel'
import NodeDataPanel from './components/NodeDataPanel'
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
const IconActivity = () => (
  <svg className="card-title-icon" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5">
    <polyline points="2,8 6,8 8,2 10,14 12,8 14,8" strokeLinecap="round" strokeLinejoin="round" />
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

// Strip any trailing slash so fetch(`${API_BASE}/api/...`) never becomes //api/...
const API_BASE = (import.meta.env.VITE_API_URL || '').replace(/\/+$/, '');
const WS_BASE  = (import.meta.env.VITE_WS_URL  ||
  `${window.location.protocol === 'https:' ? 'wss:' : 'ws:'}//${window.location.host}`
).replace(/\/+$/, '');

function App() {
  const [selectedSymbol, setSelectedSymbol] = useState('AAPL')
  const [symbols, setSymbols]     = useState(DEFAULT_SYMBOLS)
  const [wsConnected, setWsConnected] = useState(false)
  const [orderBook, setOrderBook] = useState({ bids: [], asks: [] })
  const [trades, setTrades]       = useState([])
  const [debugData, setDebugData] = useState({
     poolStats: { usedSlots: 0, freeSlots: 1000000, nextFreeIdx: 0, usedPct: 0 },
     occupiedSlots: [],
     activeNodes: [],
     priceLevels: [],
     freeListHead: [0, 1, 2, 3, 4, 5, 6, 7]
  })
  
  // HFT UI States
  const [symbolPriceData, setSymbolPriceData] = useState({}) // { AAPL: { lastPrice, changePct, prices: [], flashKey, flashDir } }
  const [candles, setCandles] = useState([]) // OHLC
  const [obiHistory, setObiHistory] = useState([]) // array of [-1, 1]
  const [latencyHistory, setLatencyHistory] = useState([]) // array of { p50, p99, ops, tps }
  const [showVwap, setShowVwap] = useState(false)
  const [simRunning, setSimRunning] = useState(false)
  const [latencyMs, setLatencyMs] = useState(null)
  const [currentView, setCurrentView] = useState('trading')
  
  const wsRef           = useRef(null)
  const simRef          = useRef(null)
  const simPriceRef     = useRef({})
  const latencyHist     = useRef([])
  const tradesBufferRef = useRef([])
  const ordersBufferRef = useRef([])
  const currentCandleRef = useRef(null)
  const clock           = useClock()

  // ── Derived market stats ──────────────────────────────────────
  const bids = orderBook?.bids || []
  const asks = orderBook?.asks || []
  const bestBid = bids[0]?.price
  const bestAsk = asks[0]?.price
  const totalBidQty = bids.reduce((s, l) => s + (l.quantity || 0), 0)
  const totalAskQty = asks.reduce((s, l) => s + (l.quantity || 0), 0)
  const imbalanceRaw = totalBidQty + totalAskQty > 0
    ? (totalBidQty - totalAskQty) / (totalBidQty + totalAskQty)
    : 0

  // Check OBI extreme
  const prevObiRef = useRef(0)
  useEffect(() => {
    prevObiRef.current = imbalanceRaw
  }, [imbalanceRaw])

  // Calculate VWAP for the selected symbol based on trades
  const vwap = useMemo(() => {
    if (trades.length === 0) return null
    let sumPV = 0
    let sumV = 0
    for (const t of trades) {
      sumPV += t.price * t.quantity
      sumV += t.quantity
    }
    return sumV > 0 ? sumPV / sumV : null
  }, [trades])
  
  // Calculate Avg Trade Size for Large Print Alert
  const avgTradeSize = useMemo(() => {
    if (trades.length === 0) return 0
    const sum = trades.reduce((acc, t) => acc + t.quantity, 0)
    return sum / trades.length
  }, [trades])

  // ── REST helpers ──────────────────────────────────────────────
  const fetchOrderBook = useCallback(async (symbol) => {
    try {
      const res = await fetch(`${API_BASE}/api/orderbook/${symbol}`)
      if (!res.ok) return
      const data = await res.json()
      console.log('Fetched OrderBook Data:', data)
      setOrderBook(data)
      
      // Update OBI history
      const obBids = data.bids || []
      const obAsks = data.asks || []
      const totalB = obBids.reduce((s, l) => s + (l.quantity || 0), 0)
      const totalA = obAsks.reduce((s, l) => s + (l.quantity || 0), 0)
      const obi = totalB + totalA > 0 ? (totalB - totalA) / (totalB + totalA) : 0
      setObiHistory(prev => [...prev, obi].slice(-60))
    } catch(err) { console.error('fetchOrderBook error:', err); }
  }, [])

  const fetchDebugData = useCallback(async (symbol) => {
    try {
      const res = await fetch(`${API_BASE}/api/debug/orderbook/${symbol}`)
      if (!res.ok) return
      const data = await res.json()
      setDebugData(data)
    } catch(err) { console.error('fetchDebugData error:', err); }
  }, [])

  const fetchTrades = useCallback(async (symbol) => {
    try {
      const res = await fetch(`${API_BASE}/api/trades/${symbol}?limit=200`) // fetch more for better VWAP/Chart
      if (!res.ok) return
      const data = await res.json()
      const fetchedTrades = Array.isArray(data) ? data : []
      setTrades(fetchedTrades)
      
      // Seed symbolPriceData for this symbol
      if (fetchedTrades.length > 0) {
          const prices = fetchedTrades.map(t => t.price).reverse()
          setSymbolPriceData(prev => ({
             ...prev,
             [symbol]: {
                lastPrice: prices[prices.length - 1],
                changePct: prices.length > 1 ? ((prices[prices.length - 1] - prices[0]) / prices[0]) * 100 : 0,
                prices: prices.slice(-20),
                flashKey: 0,
                flashDir: null
             }
          }))
          
          // Seed candles (simple mock)
          // Realistically, we'd group them by time. 
          // Here, we'll just let the websocket build the candles for now.
      }
    } catch(err) { console.error('fetchTrades error:', err); }
  }, [])

  const fetchSymbols = useCallback(async () => {
    try {
      const res = await fetch(`${API_BASE}/api/symbols`)
      if (!res.ok) return
      const data = await res.json()
      if (Array.isArray(data) && data.length > 0) {
         setSymbols(data)
         // Initialize symbol data
         const initialData = {}
         data.forEach(s => { initialData[s] = { lastPrice: null, changePct: null, prices: [], flashKey: 0 }})
         setSymbolPriceData(initialData)
      }
    } catch(err) { console.error('fetchSymbols error:', err); }
  }, [])

  // ── Metrics loop (1s interval) ────────────────────────────────
  useEffect(() => {
    const id = setInterval(() => {
       const now = Date.now()
       
       // Calculate TPS
       const recentTrades = tradesBufferRef.current.filter(t => now - t.time <= 1000)
       const tps = recentTrades.length
       tradesBufferRef.current = recentTrades
       
       // Calculate OPS
       const recentOrders = ordersBufferRef.current.filter(t => now - t <= 1000)
       const ops = recentOrders.length
       ordersBufferRef.current = recentOrders
       
       // Calc p50/p99 from latencies
       const latencies = [...latencyHist.current].sort((a,b) => a - b)
       const p50 = latencies.length > 0 ? latencies[Math.floor(latencies.length * 0.5)] : 0
       const p99 = latencies.length > 0 ? latencies[Math.floor(latencies.length * 0.99)] : 0
       
       setLatencyHistory(prev => [...prev, { p50, p99, ops, tps }].slice(-60))
       
       // Clear latencies slightly over time to reflect rolling
       if (latencies.length > 100) latencyHist.current = latencyHist.current.slice(-100)
       
       // Handle Candle closing
       if (currentCandleRef.current) {
           const c = currentCandleRef.current
           const timeStr = new Date().toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' })
           setCandles(prev => [...prev, { ...c, time: timeStr }].slice(-60))
           currentCandleRef.current = { open: c.close, high: c.close, low: c.close, close: c.close, time: timeStr }
       }
       
       fetchDebugData(selectedSymbol)
    }, 1000)
    return () => clearInterval(id)
  }, [selectedSymbol, fetchDebugData])

  // ── WebSocket ────
  useEffect(() => {
    let reconnectTimer

    const connect = () => {
      const ws = new WebSocket(`${WS_BASE}/ws`)
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
            setTrades(prev => [msg, ...prev].slice(0, 200)) // Keep 200 for good VWAP
            tradesBufferRef.current.push({ time: Date.now() })
            
            // Check large print
            if (avgTradeSize > 0 && msg.quantity > avgTradeSize * 3) {
               // large print — no alert, but could be logged
            }

            // Update Candle
            const p = msg.price
            const timeStr = new Date(msg.timestamp || Date.now()).toLocaleTimeString('en-US', { hour12: false, hour:'2-digit', minute:'2-digit', second:'2-digit'})
            if (!currentCandleRef.current) {
                currentCandleRef.current = { open: p, high: p, low: p, close: p, time: timeStr }
            } else {
                currentCandleRef.current.high = Math.max(currentCandleRef.current.high, p)
                currentCandleRef.current.low = Math.min(currentCandleRef.current.low, p)
                currentCandleRef.current.close = p
            }

          } else if (msg.type === 'orderbook_update' && msg.symbol === selectedSymbol) {
            fetchOrderBook(selectedSymbol)
            fetchDebugData(selectedSymbol)
          }
          
          // Always update symbolPriceData for Watchlist (even if not selected, if feed provided it)
          // But our feed might only send subscribed. Let's pretend it could send any trade.
          if (msg.type === 'trade') {
              setSymbolPriceData(prev => {
                  const sData = prev[msg.symbol] || { prices: [] }
                  const oldPrices = sData.prices || []
                  const p = msg.price
                  
                  const isUp = p >= (sData.lastPrice || p)
                  const newPrices = [...oldPrices, p].slice(-20)
                  const cp = newPrices.length > 1 ? ((newPrices[newPrices.length - 1] - newPrices[0]) / newPrices[0]) * 100 : 0
                  
                  return {
                      ...prev,
                      [msg.symbol]: {
                          lastPrice: p,
                          changePct: cp,
                          prices: newPrices,
                          flashKey: (sData.flashKey || 0) + 1,
                          flashDir: isUp ? 'up' : 'down'
                      }
                  }
              })
          }
          
        } catch { }
      }
    }

    connect()
    return () => {
      clearTimeout(reconnectTimer)
      wsRef.current?.close()
    }
  }, [selectedSymbol, fetchOrderBook, fetchDebugData, avgTradeSize])

  // ── Initial data ──────────────────────────────────────────────
  useEffect(() => {
    fetchSymbols()
    fetchOrderBook(selectedSymbol)
    fetchTrades(selectedSymbol)
    fetchDebugData(selectedSymbol)
  }, []) // eslint-disable-line

  // ── Symbol switch ─────────────────────────────────────────────
  const handleSymbolChange = useCallback((symbol) => {
    setSelectedSymbol(symbol)
    setOrderBook({ bids: [], asks: [] })
    setTrades([])
    setCandles([])
    currentCandleRef.current = null
    fetchOrderBook(symbol)
    fetchTrades(symbol)
    fetchDebugData(symbol)
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({ subscribe: symbol }))
    }
  }, [fetchOrderBook, fetchTrades, fetchDebugData])

  // ── Order submit with latency tracking ───────────────────────
  const handleOrderSubmit = useCallback(async (orderData) => {
    const t0 = performance.now()
    ordersBufferRef.current.push(Date.now())
    try {
      const res = await fetch(`${API_BASE}/api/orders`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ...orderData, symbol: selectedSymbol }),
      })
      const latency = performance.now() - t0
      latencyHist.current = [...latencyHist.current, latency]
      
      const sorted = [...latencyHist.current].sort((a, b) => a - b)
      const p50 = sorted[Math.floor(sorted.length * 0.5)]
      setLatencyMs(Math.round(p50 * 10) / 10)

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
      ordersBufferRef.current.push(Date.now())
      const t0 = performance.now()
      const body = { symbol: sym, side, type, quantity: qty }
      if (type === 'limit') body.price = price
      await fetch(`${API_BASE}/api/orders`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      })
      const latency = performance.now() - t0
      latencyHist.current = [...latencyHist.current, latency]
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
            <div className="navbar-subtitle">C++20 High-Performance Matching Engine · HFT Terminal View</div>
          </div>
        </div>

        <div className="navbar-meta">
          {latencyMs !== null && (
            <div className="latency-pill" title="p50 REST round-trip latency">
              <span className="latency-label">REST p50</span>
              <span className="latency-value">{latencyMs}ms</span>
            </div>
          )}
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
        {/* Top: Watchlist Strip */}
        <WatchlistStrip 
           symbols={symbols} 
           selectedSymbol={selectedSymbol} 
           onSymbolChange={handleSymbolChange} 
           symbolPriceData={symbolPriceData} 
        />

        {/* Dashboard grid */}
        
        <div className="tab-container" style={{ padding: '0 16px 12px 16px', display: 'flex', gap: '8px' }}>
           <button className={`tab-btn ${currentView === 'trading' ? 'active' : ''}`} onClick={() => setCurrentView('trading')} style={{ padding: '8px 16px', borderRadius: '4px', background: currentView === 'trading' ? 'var(--bg-accent)' : 'var(--bg-panel)', color: '#fff', border: 'none', cursor: 'pointer' }}>Trading Dashboard</button>
           <button className={`tab-btn ${currentView === 'debug' ? 'active' : ''}`} onClick={() => setCurrentView('debug')} style={{ padding: '8px 16px', borderRadius: '4px', background: currentView === 'debug' ? 'var(--bg-accent)' : 'var(--bg-panel)', color: '#fff', border: 'none', cursor: 'pointer' }}>Memory Pool &amp; Nodes</button>
        </div>

        {currentView === 'trading' ? (
        <div className="hft-dashboard-grid">
          
          {/* Middle-Left: Candlestick Chart & Tape */}
          <div className="left-column">
             <section className="card card-depth" aria-labelledby="depth-title">
               <div className="card-header">
                 <div className="card-title" id="depth-title"><IconChart /> {selectedSymbol} Candlestick Chart (1s)</div>
                 <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                     <button className={`vwap-btn ${showVwap ? 'active' : ''}`} onClick={() => setShowVwap(!showVwap)}>
                         VWAP {showVwap ? 'ON' : 'OFF'}
                     </button>
                     <span className="card-badge live">Live</span>
                 </div>
               </div>
               <div className="card-body">
                 <CandleChart candles={candles} showVwap={showVwap} vwap={vwap} />
               </div>
             </section>

             <section className="card card-tradetape" aria-labelledby="tape-title">
               <div className="card-header">
                 <div className="card-title" id="tape-title"><IconTape /> Trade Tape</div>
                 <span className="card-badge live">Live</span>
               </div>
               <div className="card-body">
                 <TradeTape trades={trades} avgTradeSize={avgTradeSize} />
               </div>
             </section>
          </div>

          {/* Middle-Center: Order Book + Latency */}
          <div className="center-column">
                <section className="card" aria-labelledby="ob-title">
                  <div className="card-header">
                    <div className="card-title" id="ob-title"><IconBook /> Order Book</div>
                    <span className="card-badge live">Live</span>
                  </div>
                  <div className="card-body">
                    <OrderBookDepth orderBook={orderBook} />
                  </div>
                </section>
              <section className="card card-latency-center" aria-labelledby="lat-title">
                 <div className="card-header">
                    <div className="card-title" id="lat-title"><IconActivity /> Performance Monitor</div>
                 </div>
                 <div className="card-body" style={{ padding: '12px' }}>
                    <LatencyDashboard 
                        latencyHistory={latencyHistory} 
                        ordersPerSec={latencyHistory[latencyHistory.length-1]?.ops || 0}
                        tradesPerSec={latencyHistory[latencyHistory.length-1]?.tps || 0}
                    />
                 </div>
              </section>
          </div>

          {/* Right Column: Order Entry + OBI */}
          <div className="right-column">
             <section className="card col-order-entry" aria-labelledby="oe-title">
               <div className="card-header">
                 <div className="card-title" id="oe-title"><IconOrder /> Order Entry</div>
               </div>
               <div className="card-body">
                 <OrderEntryForm onSubmit={handleOrderSubmit} selectedSymbol={selectedSymbol} />
               </div>
             </section>
             
             <section className="card card-obi" aria-labelledby="obi-title">
                <div className="card-header">
                   <div className="card-title" id="obi-title"><IconActivity /> Order Book Imbalance</div>
                </div>
                <div className="card-body">
                   <OBIPanel obi={imbalanceRaw} obiHistory={obiHistory} />
                </div>
             </section>
          </div>
        </div>
        ) : (
        <div className="debug-dashboard-grid" style={{ padding: '0 16px', display: 'flex', flexDirection: 'column', gap: '16px' }}>
           <MemoryPoolPanel debugData={debugData} />
           <NodeDataPanel debugData={debugData} />
        </div>
        )}
      </main>
    </div>
  )
}

export default App
