import React from 'react'

/**
 * OBIPanel — Order Book Imbalance visualization.
 *
 * Top: animated gauge bar (sell ←—— | ——→ buy)
 *      with a glowing needle at the current OBI position.
 * Middle: numeric readout + status label.
 * Bottom: 60-point OBI history as an SVG line chart.
 *
 * OBI is in range [-1, 1]:
 *   -1 = pure sell pressure
 *    0 = balanced
 *   +1 = pure buy pressure
 */
function OBIPanel({ obi = 0, obiHistory = [] }) {
  const val = isFinite(obi) ? obi : 0

  // Gauge fill: 50% = neutral, 0% = full sell, 100% = full buy
  const pct = ((val + 1) / 2) * 100

  const color =
    val > 0.4  ? '#23c55e' :
    val < -0.4 ? '#ef4444' :
    val > 0.15 ? '#4ade80' :
    val < -0.15 ? '#f87171' :
    '#8b949e'

  const statusLabel =
    val > 0.6  ? 'STRONG BUY PRESSURE'  :
    val > 0.25 ? 'BUY PRESSURE'         :
    val < -0.6 ? 'STRONG SELL PRESSURE' :
    val < -0.25 ? 'SELL PRESSURE'       :
    'BALANCED'

  // OBI history SVG chart
  const W = 260, H = 64
  const PX = 8, PY = 8
  const pW = W - PX * 2
  const pH = H - PY * 2

  const histPts = obiHistory.length >= 2
    ? obiHistory
        .map((v, i) => {
          const x = PX + (i / (obiHistory.length - 1)) * pW
          const y = PY + (1 - (v + 1) / 2) * pH
          return `${x.toFixed(1)},${y.toFixed(1)}`
        })
        .join(' ')
    : null

  return (
    <div className="obi-panel">
      {/* ── Gauge ── */}
      <div className="obi-gauge-wrapper">
        <span className="obi-side-label sell">SELL</span>

        <div className="obi-gauge-track" title={`OBI: ${val.toFixed(3)}`}>
          {/* Gradient fill bar */}
          <div
            className="obi-gauge-fill"
            style={{
              left:  val >= 0 ? '50%'       : `${pct}%`,
              width: `${Math.abs(val) * 50}%`,
              background: color,
            }}
          />
          {/* Center tick (neutral line) */}
          <div className="obi-gauge-mid-tick" />
          {/* Needle */}
          <div
            className="obi-gauge-needle"
            style={{
              left: `${pct}%`,
              background: color,
              boxShadow: `0 0 8px ${color}`,
            }}
          />
        </div>

        <span className="obi-side-label buy">BUY</span>
      </div>

      {/* ── Numeric readout ── */}
      <div className="obi-readout-row">
        <span className="obi-value" style={{ color }}>
          {val >= 0 ? '+' : ''}{val.toFixed(4)}
        </span>
        <span className="obi-status" style={{ color }}>
          {statusLabel}
        </span>
      </div>

      {/* ── History chart ── */}
      <div className="obi-history-wrap">
        <div className="obi-history-label">60-SECOND HISTORY</div>
        <svg
          viewBox={`0 0 ${W} ${H}`}
          style={{ width: '100%', height: 'auto', display: 'block' }}
          aria-label="OBI history chart"
        >
          {/* Zero / neutral line */}
          <line
            x1={PX} y1={H / 2}
            x2={W - PX} y2={H / 2}
            stroke="rgba(255,255,255,0.08)"
            strokeWidth="1"
            strokeDasharray="3 4"
          />
          {/* +0.5 and -0.5 reference lines */}
          {[0.5, -0.5].map((ref) => {
            const ry = PY + (1 - (ref + 1) / 2) * pH
            return (
              <line
                key={ref}
                x1={PX} y1={ry}
                x2={W - PX} y2={ry}
                stroke="rgba(255,255,255,0.04)"
                strokeWidth="1"
              />
            )
          })}

          {/* OBI line */}
          {histPts && (
            <polyline
              points={histPts}
              fill="none"
              stroke="#388bfd"
              strokeWidth="1.5"
              strokeLinejoin="round"
              strokeLinecap="round"
              opacity="0.9"
            />
          )}

          {/* Current value dot */}
          {obiHistory.length > 0 && (() => {
            const lv = obiHistory[obiHistory.length - 1]
            const lx = W - PX
            const ly = PY + (1 - (lv + 1) / 2) * pH
            return (
              <circle
                cx={lx} cy={ly} r="3"
                fill={color}
                stroke="rgba(0,0,0,0.5)"
                strokeWidth="1"
              />
            )
          })()}

          {/* Y axis labels */}
          {[{v: 1, l: '+1'}, {v: 0, l: '0'}, {v: -1, l: '-1'}].map(({v, l}) => (
            <text
              key={l}
              x={PX - 4} y={PY + (1 - (v + 1) / 2) * pH + 4}
              fill="rgba(139,148,158,0.5)" fontSize="8"
              textAnchor="end"
              fontFamily="'JetBrains Mono', monospace"
            >
              {l}
            </text>
          ))}
        </svg>
      </div>
    </div>
  )
}

export default OBIPanel
