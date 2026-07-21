import React, { useMemo } from 'react'

const MAX_ROWS = 14

/**
 * OrderBookDepth — renders BIDS (left) and ASKS (right) side-by-side.
 */
function OrderBookDepth({ orderBook }) {
  const { bids = [], asks = [] } = orderBook

  // Cumulative totals for depth bars
  const bidsWithTotal = useMemo(() => {
    let running = 0
    return bids.slice(0, MAX_ROWS).map(lvl => {
      running += lvl.quantity
      return { ...lvl, total: running }
    })
  }, [bids])

  const asksWithTotal = useMemo(() => {
    let running = 0
    return asks.slice(0, MAX_ROWS).map(lvl => {
      running += lvl.quantity
      return { ...lvl, total: running }
    })
  }, [asks])

  const maxBidTotal = bidsWithTotal.at(-1)?.total || 1
  const maxAskTotal = asksWithTotal.at(-1)?.total || 1

  // Spread calculation
  const bestBid = bids[0]?.price
  const bestAsk = asks[0]?.price
  const spread = bestBid != null && bestAsk != null
    ? (bestAsk - bestBid).toFixed(2)
    : '—'
  const spreadPct = bestBid != null && bestAsk != null
    ? (((bestAsk - bestBid) / bestBid) * 100).toFixed(3) + '%'
    : ''

  const fmt = (n) => typeof n === 'number' ? n.toFixed(2) : '—'
  const fmtQty = (n) => typeof n === 'number' ? n.toLocaleString() : '—'

  return (
    <div className="ob-wrapper">
      {/* Spread divider at the top */}
      <div className="ob-divider" style={{ borderTop: 'none' }}>
        <span className="ob-spread-label">Spread</span>
        <span className="ob-spread-value">{spread}</span>
        {spreadPct && (
          <span className="ob-spread-label" style={{ marginLeft: 4 }}>({spreadPct})</span>
        )}
      </div>

      <div className="ob-split-container">
        {/* BIDS (Left) */}
        <div className="ob-half">
          <div className="ob-col-headers">
            <span>Price</span>
            <span className="col-right">Qty</span>
            <span className="col-right">Total</span>
          </div>
          <div className="ob-section">
            {bidsWithTotal.length === 0 ? (
              <div className="ob-empty">No bids</div>
            ) : (
              bidsWithTotal.map((lvl, i) => (
                <div
                  key={`bid-${i}`}
                  className="ob-row bid"
                  style={{ '--depth': `${(lvl.total / maxBidTotal) * 100}%` }}
                  title={`Bid ${fmt(lvl.price)} × ${fmtQty(lvl.quantity)}`}
                >
                  <span className="ob-price">{fmt(lvl.price)}</span>
                  <span className="ob-qty">{fmtQty(lvl.quantity)}</span>
                  <span className="ob-total">{fmtQty(lvl.total)}</span>
                </div>
              ))
            )}
          </div>
        </div>

        {/* ASKS (Right) */}
        <div className="ob-half">
          <div className="ob-col-headers">
            <span>Price</span>
            <span className="col-right">Qty</span>
            <span className="col-right">Total</span>
          </div>
          <div className="ob-section">
            {asksWithTotal.length === 0 ? (
              <div className="ob-empty">No asks</div>
            ) : (
              asksWithTotal.map((lvl, i) => (
                <div
                  key={`ask-${i}`}
                  className="ob-row ask"
                  style={{ '--depth': `${(lvl.total / maxAskTotal) * 100}%` }}
                  title={`Ask ${fmt(lvl.price)} × ${fmtQty(lvl.quantity)}`}
                >
                  <span className="ob-price">{fmt(lvl.price)}</span>
                  <span className="ob-qty">{fmtQty(lvl.quantity)}</span>
                  <span className="ob-total">{fmtQty(lvl.total)}</span>
                </div>
              ))
            )}
          </div>
        </div>
      </div>
    </div>
  )
}

export default OrderBookDepth
