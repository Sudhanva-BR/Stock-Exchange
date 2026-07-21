import React from 'react'

/**
 * Sparkline — tiny filled SVG area chart.
 * `id` is used to make the gradient ID unique across multiple instances.
 */
function Sparkline({ data = [], color = '#388bfd', height = 36, id }) {
  const W = 130
  const H = height

  if (!data || data.length < 2) {
    return (
      <div
        style={{
          width: W, height: H,
          background: 'rgba(255,255,255,0.03)',
          borderRadius: 4,
          opacity: 0.5,
        }}
      />
    )
  }

  const min = Math.min(...data)
  const max = Math.max(...data)
  const range = max - min || 0.001

  const pts = data.map((v, i) => {
    const x = (i / (data.length - 1)) * W
    const y = H - 2 - ((v - min) / range) * (H - 6)
    return `${x.toFixed(1)},${y.toFixed(1)}`
  })

  const linePath = pts.join(' ')
  // Closed path for fill area
  const fillPath = `0,${H} ${linePath} ${W},${H}`
  const gradId = `sl-grad-${id}`

  return (
    <svg
      width={W}
      height={H}
      viewBox={`0 0 ${W} ${H}`}
      aria-hidden="true"
      style={{ display: 'block', flexShrink: 0 }}
    >
      <defs>
        <linearGradient id={gradId} x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stopColor={color} stopOpacity="0.25" />
          <stop offset="100%" stopColor={color} stopOpacity="0.02" />
        </linearGradient>
      </defs>
      <polygon points={fillPath} fill={`url(#${gradId})`} />
      <polyline
        points={linePath}
        fill="none"
        stroke={color}
        strokeWidth="1.5"
        strokeLinejoin="round"
        strokeLinecap="round"
      />
      {/* Last value dot */}
      <circle
        cx={(data.length - 1) / (data.length - 1) * W}
        cy={H - 2 - ((data[data.length - 1] - min) / range) * (H - 6)}
        r="2.5"
        fill={color}
      />
    </svg>
  )
}

/**
 * MetricCard — one metric tile: label, big value, sparkline.
 */
function MetricCard({ label, value, unit, sublabel, data, color, id }) {
  return (
    <div className="lat-metric-card">
      <div className="lat-metric-header">
        <span className="lat-metric-label">{label}</span>
        {sublabel && <span className="lat-metric-sublabel">{sublabel}</span>}
      </div>
      <div className="lat-metric-value-row">
        <span className="lat-metric-num" style={{ color }}>
          {value}
        </span>
        {unit && <span className="lat-metric-unit">{unit}</span>}
      </div>
      <Sparkline data={data} color={color} id={id} />
    </div>
  )
}

/**
 * LatencyDashboard — 2×2 grid of performance metrics.
 *
 *  REST p50  |  REST p99
 * -----------+-----------
 *  Orders/s  |  Trades/s
 */
function LatencyDashboard({ latencyHistory = [], ordersPerSec = 0, tradesPerSec = 0 }) {
  const p50Data = latencyHistory.map((l) => l.p50)
  const p99Data = latencyHistory.map((l) => l.p99)
  const opsData = latencyHistory.map((l) => l.ops || 0)
  const tpsData = latencyHistory.map((l) => l.tps || 0)

  const lastP50 = p50Data[p50Data.length - 1] ?? 0
  const lastP99 = p99Data[p99Data.length - 1] ?? 0

  const p50Color = lastP50 < 5 ? '#23c55e' : lastP50 < 20 ? '#f59e0b' : '#ef4444'
  const p99Color = lastP99 < 20 ? '#23c55e' : lastP99 < 100 ? '#f59e0b' : '#ef4444'

  if (latencyHistory.length === 0) {
    return (
      <div className="lat-empty">
        <div>Submit an order to see latency stats</div>
        <div style={{ marginTop: 4, fontSize: 11, opacity: 0.5 }}>
          p50 / p99 / orders/s / trades/s
        </div>
      </div>
    )
  }

  return (
    <div className="lat-dashboard">
      <div className="lat-grid">
        <MetricCard
          id="p50"
          label="REST p50"
          value={lastP50.toFixed(1)}
          unit="ms"
          sublabel="latency"
          data={p50Data}
          color={p50Color}
        />
        <MetricCard
          id="p99"
          label="REST p99"
          value={lastP99.toFixed(1)}
          unit="ms"
          sublabel="latency"
          data={p99Data}
          color={p99Color}
        />
        <MetricCard
          id="ops"
          label="Orders/s"
          value={ordersPerSec.toFixed(0)}
          unit=""
          sublabel="throughput"
          data={opsData}
          color="#388bfd"
        />
        <MetricCard
          id="tps"
          label="Trades/s"
          value={tradesPerSec.toFixed(0)}
          unit=""
          sublabel="fill rate"
          data={tpsData}
          color="#a78bfa"
        />
      </div>
    </div>
  )
}

export default LatencyDashboard
