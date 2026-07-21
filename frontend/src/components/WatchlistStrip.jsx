import React, { useMemo } from 'react'

/**
 * MiniSparkline — tiny SVG line chart for price history.
 * Uses a unique per-symbol gradient to avoid SVG ID conflicts.
 */
function MiniSparkline({ prices, isUp }) {
  if (!prices || prices.length < 2) {
    return <div style={{ width: 48, height: 18 }} />
  }
  const W = 48, H = 18
  const min = Math.min(...prices)
  const max = Math.max(...prices)
  const range = max - min || 0.001
  const pts = prices
    .map((p, i) => {
      const x = (i / (prices.length - 1)) * W
      const y = H - 2 - ((p - min) / range) * (H - 4)
      return `${x.toFixed(1)},${y.toFixed(1)}`
    })
    .join(' ')

  const color = isUp ? '#23c55e' : '#ef4444'
  return (
    <svg
      width={W}
      height={H}
      viewBox={`0 0 ${W} ${H}`}
      style={{ display: 'block', flexShrink: 0 }}
      aria-hidden="true"
    >
      <polyline
        points={pts}
        fill="none"
        stroke={color}
        strokeWidth="1.5"
        strokeLinejoin="round"
        strokeLinecap="round"
        opacity="0.9"
      />
    </svg>
  )
}

/**
 * WatchlistStrip — Bloomberg-style horizontal ticker bar.
 * Shows: symbol, last price (with flash), % change from session open, mini sparkline.
 */
function WatchlistStrip({ symbols, selectedSymbol, onSymbolChange, symbolPriceData }) {
  return (
    <div className="watchlist-strip" role="tablist" aria-label="Symbol watchlist">
      <div className="watchlist-label">WATCHLIST</div>
      {symbols.map((sym) => {
        const data = symbolPriceData[sym] || {}
        const { lastPrice, changePct, prices = [], flashKey = 0, flashDir } = data
        const isUp = (changePct || 0) >= 0
        const isSelected = sym === selectedSymbol

        return (
          <button
            key={sym}
            id={`watchlist-${sym}`}
            role="tab"
            aria-selected={isSelected}
            className={`watchlist-ticker${isSelected ? ' wt-active' : ''}`}
            onClick={() => onSymbolChange(sym)}
            title={`Switch to ${sym}`}
          >
            {/* Symbol */}
            <div className="wt-symbol">{sym}</div>

            {/* Price — key-based remount triggers CSS flash animation */}
            <div
              key={flashKey}
              className={`wt-price${flashDir ? ` wt-flash-${flashDir}` : ''} ${isUp ? 'up' : 'down'}`}
            >
              {lastPrice != null ? lastPrice.toFixed(2) : '—'}
            </div>

            {/* Change % */}
            <div className={`wt-change ${isUp ? 'up' : 'down'}`}>
              {changePct != null
                ? `${isUp ? '+' : ''}${changePct.toFixed(2)}%`
                : '—'}
            </div>

            {/* Sparkline */}
            <MiniSparkline prices={prices} isUp={isUp} />
          </button>
        )
      })}
    </div>
  )
}

export default WatchlistStrip
