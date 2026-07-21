import React, { useState, useCallback } from 'react'

const ORDER_TYPES = ['limit', 'market']

/**
 * OrderEntryForm — compact, keyboard-friendly order ticket.
 * Supports BUY / SELL sides and LIMIT / MARKET types.
 */
function OrderEntryForm({ onSubmit, selectedSymbol }) {
  const [side, setSide] = useState('buy')
  const [type, setType] = useState('limit')
  const [price, setPrice] = useState('')
  const [quantity, setQuantity] = useState('')
  const [status, setStatus] = useState(null)
  const [loading, setLoading] = useState(false)

  const handleSubmit = useCallback(async (e) => {
    e.preventDefault()
    setStatus(null)
    setLoading(true)

    const orderData = {
      side,
      type,
      quantity: parseInt(quantity, 10),
    }

    if (type === 'limit') {
      const parsedPrice = parseFloat(price)
      if (isNaN(parsedPrice) || parsedPrice <= 0) {
        setStatus({ type: 'error', message: 'Enter a valid price > 0' })
        setLoading(false)
        return
      }
      orderData.price = parsedPrice
    }

    if (isNaN(orderData.quantity) || orderData.quantity < 1) {
      setStatus({ type: 'error', message: 'Enter a valid quantity ≥ 1' })
      setLoading(false)
      return
    }

    try {
      const result = await onSubmit(orderData)
      if (result.success) {
        setStatus({
          type: 'success',
          message: `✓ ${side.toUpperCase()} order placed — ID #${result.data.orderId}`,
        })
        setQuantity('')
        setPrice('')
      } else {
        setStatus({ type: 'error', message: result.error || 'Order rejected' })
      }
    } catch (err) {
      setStatus({ type: 'error', message: err.message })
    } finally {
      setLoading(false)
    }
  }, [side, type, price, quantity, onSubmit])

  const handleSide = (s) => {
    setSide(s)
    setStatus(null)
  }

  const handleType = (t) => {
    setType(t)
    setStatus(null)
    if (t === 'market') setPrice('')
  }

  return (
    <form className="order-form" onSubmit={handleSubmit} noValidate>
      {/* Side toggle */}
      <div>
        <div className="form-label" style={{ marginBottom: 8 }}>Direction</div>
        <div className="side-toggle" role="group" aria-label="Order side">
          <button
            type="button"
            id="order-side-buy"
            aria-pressed={side === 'buy'}
            className={`side-btn ${side === 'buy' ? 'active-buy' : ''}`}
            onClick={() => handleSide('buy')}
          >
            ▲ Buy
          </button>
          <button
            type="button"
            id="order-side-sell"
            aria-pressed={side === 'sell'}
            className={`side-btn ${side === 'sell' ? 'active-sell' : ''}`}
            onClick={() => handleSide('sell')}
          >
            ▼ Sell
          </button>
        </div>
      </div>

      {/* Order type */}
      <div>
        <div className="form-label" style={{ marginBottom: 8 }}>Order Type</div>
        <div className="type-pills" role="group" aria-label="Order type">
          {ORDER_TYPES.map(t => (
            <button
              key={t}
              type="button"
              id={`order-type-${t}`}
              aria-pressed={type === t}
              className={`type-pill ${type === t ? 'active' : ''}`}
              onClick={() => handleType(t)}
            >
              {t.charAt(0).toUpperCase() + t.slice(1)}
            </button>
          ))}
        </div>
      </div>

      <div className="form-divider" />

      {/* Price (limit only) */}
      {type === 'limit' && (
        <div className="form-field">
          <label htmlFor="order-price" className="form-label">Limit Price</label>
          <input
            id="order-price"
            type="number"
            step="0.01"
            min="0.01"
            className="form-input"
            value={price}
            onChange={(e) => setPrice(e.target.value)}
            placeholder="0.00"
            required
            autoComplete="off"
          />
        </div>
      )}

      {/* Quantity */}
      <div className="form-field">
        <label htmlFor="order-quantity" className="form-label">Quantity</label>
        <input
          id="order-quantity"
          type="number"
          min="1"
          step="1"
          className="form-input"
          value={quantity}
          onChange={(e) => setQuantity(e.target.value)}
          placeholder="0"
          required
          autoComplete="off"
        />
      </div>

      {/* Symbol hint */}
      {selectedSymbol && (
        <div style={{ fontSize: 11, color: 'var(--text-muted)', textAlign: 'center' }}>
          Placing order on <span style={{ color: 'var(--text-secondary)', fontFamily: 'var(--font-mono)', fontWeight: 600 }}>{selectedSymbol}</span>
        </div>
      )}

      {/* Submit */}
      <button
        id="order-submit-btn"
        type="submit"
        className={`submit-btn ${side}`}
        disabled={loading}
        aria-busy={loading}
      >
        {loading
          ? 'Submitting…'
          : `${side === 'buy' ? '▲ Buy' : '▼ Sell'} ${selectedSymbol || ''}`}
      </button>

      {/* Status */}
      {status && (
        <div
          role="alert"
          className={`status-alert ${status.type}`}
        >
          {status.message}
        </div>
      )}
    </form>
  )
}

export default OrderEntryForm
