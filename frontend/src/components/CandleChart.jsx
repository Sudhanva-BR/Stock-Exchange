import React, { useMemo } from 'react'

const EMPTY_CHART_MSG = 'Waiting for trades — click Sim ON or submit orders'

/**
 * CandleChart — real-time OHLC candlestick chart built from tick data.
 * Renders as a pure SVG with:
 *   - Candle bodies + wicks (green = bullish, red = bearish)
 *   - Y-axis price grid with labels
 *   - X-axis time labels (every N candles)
 *   - Dashed current-price line with price tag
 *   - Optional VWAP line (amber dashed) with label
 */
function CandleChart({ candles, showVwap, vwap }) {
  const W = 720
  const H = 220
  const P = { top: 16, right: 52, bottom: 30, left: 58 }

  const visible = useMemo(() => (candles || []).slice(-60), [candles])

  if (!visible.length) {
    return (
      <div className="chart-empty">
        <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" opacity="0.4">
          <rect x="3" y="7" width="4" height="10" rx="1" />
          <line x1="5" y1="3" x2="5" y2="7" /><line x1="5" y1="17" x2="5" y2="21" />
          <rect x="10" y="4" width="4" height="13" rx="1" />
          <line x1="12" y1="1" x2="12" y2="4" /><line x1="12" y1="17" x2="12" y2="22" />
          <rect x="17" y="9" width="4" height="7" rx="1" />
          <line x1="19" y1="5" x2="19" y2="9" /><line x1="19" y1="16" x2="19" y2="20" />
        </svg>
        <span>{EMPTY_CHART_MSG}</span>
      </div>
    )
  }

  const pW = W - P.left - P.right
  const pH = H - P.top - P.bottom

  // Price range with padding
  const allPrices = visible.flatMap((c) => [c.high, c.low])
  const rawMin = Math.min(...allPrices)
  const rawMax = Math.max(...allPrices)
  const pad = (rawMax - rawMin) * 0.08 || rawMin * 0.005
  const minP = rawMin - pad
  const maxP = rawMax + pad
  const priceRange = maxP - minP || 1

  // If VWAP is outside current range, extend
  const effectiveMin = showVwap && vwap != null ? Math.min(minP, vwap - pad) : minP
  const effectiveMax = showVwap && vwap != null ? Math.max(maxP, vwap + pad) : maxP
  const effectiveRange = effectiveMax - effectiveMin || 1

  const toX = (i) => P.left + ((i + 0.5) / visible.length) * pW
  const toY = (p) => P.top + pH - ((p - effectiveMin) / effectiveRange) * pH

  const candleW = Math.max(2, Math.min(10, (pW / visible.length) * 0.65))

  // Y-axis ticks (5 levels)
  const yTicks = Array.from({ length: 5 }, (_, i) => {
    const val = effectiveMin + (effectiveRange * i) / 4
    return { val, y: toY(val) }
  })

  // X-axis labels (every ~8 candles, but not too many)
  const xStep = Math.max(1, Math.floor(visible.length / 7))
  const xLabels = visible
    .map((c, i) => ({ c, i }))
    .filter(({ i }) => i % xStep === 0 || i === visible.length - 1)

  const lastCandle = visible[visible.length - 1]

  return (
    <svg
      viewBox={`0 0 ${W} ${H}`}
      style={{ width: '100%', height: 'auto', display: 'block' }}
      role="img"
      aria-label={`Candlestick chart for ${lastCandle?.time}`}
    >
      {/* Chart background rect */}
      <rect
        x={P.left} y={P.top}
        width={pW} height={pH}
        fill="rgba(56,139,253,0.015)"
        rx="2"
      />

      {/* Y-axis grid lines + labels */}
      {yTicks.map(({ val, y }, i) => (
        <g key={i}>
          <line
            x1={P.left} y1={y} x2={W - P.right} y2={y}
            stroke="rgba(255,255,255,0.05)" strokeWidth="1"
          />
          <text
            x={P.left - 6} y={y + 4}
            fill="rgba(139,148,158,0.75)" fontSize="9"
            textAnchor="end"
            fontFamily="'JetBrains Mono', monospace"
          >
            {val.toFixed(2)}
          </text>
        </g>
      ))}

      {/* Candle bodies + wicks */}
      {visible.map((c, i) => {
        const x = toX(i)
        const isGreen = c.close >= c.open
        const color = isGreen ? '#23c55e' : '#ef4444'
        const bodyTop = toY(Math.max(c.open, c.close))
        const bodyBot = toY(Math.min(c.open, c.close))
        const bodyH = Math.max(1.5, bodyBot - bodyTop)
        return (
          <g key={`candle-${i}`}>
            {/* Wick */}
            <line
              x1={x} y1={toY(c.high)}
              x2={x} y2={toY(c.low)}
              stroke={color} strokeWidth="1.2"
              opacity="0.85"
            />
            {/* Body */}
            <rect
              x={x - candleW / 2} y={bodyTop}
              width={candleW} height={bodyH}
              fill={isGreen ? color : 'none'}
              stroke={color}
              strokeWidth="1"
              opacity="0.9"
              rx="0.5"
            />
          </g>
        )
      })}

      {/* VWAP line */}
      {showVwap && vwap != null && (() => {
        const vy = toY(vwap)
        if (vy < P.top || vy > H - P.bottom) return null
        return (
          <g>
            <line
              x1={P.left} y1={vy}
              x2={W - P.right} y2={vy}
              stroke="#f59e0b" strokeWidth="1.5"
              strokeDasharray="5 4" opacity="0.9"
            />
            {/* VWAP label tag */}
            <rect
              x={W - P.right + 2} y={vy - 9}
              width={46} height={16}
              rx="3"
              fill="rgba(245,158,11,0.15)"
              stroke="rgba(245,158,11,0.45)"
              strokeWidth="1"
            />
            <text
              x={W - P.right + 25} y={vy + 4}
              fill="#f59e0b" fontSize="9"
              textAnchor="middle"
              fontFamily="'JetBrains Mono', monospace"
            >
              VWAP
            </text>
          </g>
        )
      })()}

      {/* Current price dash + tag */}
      {lastCandle && (() => {
        const lp = lastCandle.close
        const ly = toY(lp)
        const isGreen = lastCandle.close >= lastCandle.open
        const color = isGreen ? '#23c55e' : '#ef4444'
        return (
          <g>
            <line
              x1={P.left} y1={ly}
              x2={W - P.right} y2={ly}
              stroke={color} strokeWidth="0.7"
              strokeDasharray="3 5" opacity="0.5"
            />
            <rect
              x={W - P.right + 2} y={ly - 9}
              width={46} height={16}
              rx="3"
              fill={`${color}22`}
              stroke={`${color}88`}
              strokeWidth="1"
            />
            <text
              x={W - P.right + 25} y={ly + 4}
              fill={color} fontSize="9"
              textAnchor="middle"
              fontFamily="'JetBrains Mono', monospace"
              fontWeight="600"
            >
              {lp.toFixed(2)}
            </text>
          </g>
        )
      })()}

      {/* X-axis baseline */}
      <line
        x1={P.left} y1={H - P.bottom}
        x2={W - P.right} y2={H - P.bottom}
        stroke="rgba(255,255,255,0.07)" strokeWidth="1"
      />

      {/* X-axis time labels */}
      {xLabels.map(({ c, i }) => (
        <text
          key={`xl-${i}`}
          x={toX(i)} y={H - P.bottom + 14}
          fill="rgba(139,148,158,0.6)" fontSize="9"
          textAnchor="middle"
          fontFamily="'JetBrains Mono', monospace"
        >
          {c.time}
        </text>
      ))}
    </svg>
  )
}

export default CandleChart
