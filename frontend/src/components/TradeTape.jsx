import React from 'react'

const MAX_TRADES = 50

/**
 * TradeTape — live scrolling list of recent executions.
 * Newest trades appear at the top with directional color + arrow.
 */
function TradeTape({ trades }) {
  const formatTime = (timestamp) => {
    try {
      const date = new Date(timestamp)
      return date.toLocaleTimeString('en-US', {
        hour12: false,
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
      })
    } catch {
      return '—'
    }
  }

  const getDirection = (index) => {
    if (index >= trades.length - 1) return 'flat'
    return trades[index].price > trades[index + 1].price ? 'up' : 
           trades[index].price < trades[index + 1].price ? 'down' : 'flat'
  }

  const fmtPrice = (n) => (typeof n === 'number' ? n.toFixed(2) : '—')
  const fmtQty   = (n) => (typeof n === 'number' ? n.toLocaleString() : '—')

  const ARROW = { up: '▲', down: '▼', flat: '—' }

  return (
    <div className="tape-list" role="log" aria-label="Trade tape" aria-live="polite">

      {trades.length === 0 ? (
        <div className="tape-empty">
          <div>No executions yet</div>
          <div style={{ marginTop: 4, fontSize: 11 }}>Submit an order to see trades here</div>
        </div>
      ) : (
        trades.slice(0, MAX_TRADES).map((trade, index) => {
          const dir = getDirection(index)
          return (
            <div key={trade.tradeId ?? `trade-${index}`} className="tape-row-card">
              <div className="tape-row-header">
                 <span className={`tape-badge ${dir}`}>FILL</span>
              </div>
              <div className="tape-row-body">
                 Matched {trade.buyOrderId} &amp; {trade.sellOrderId} | {fmtQty(trade.quantity)} shares @ ${fmtPrice(trade.price)}
              </div>
            </div>
          )
        })
      )}
    </div>
  )
}

export default TradeTape
