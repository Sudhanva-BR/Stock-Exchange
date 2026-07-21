import React from 'react'

/**
 * SymbolSelector — horizontal chip row for selecting a trading symbol.
 */
function SymbolSelector({ symbols, selectedSymbol, onSymbolChange }) {
  return (
    <div className="symbol-bar" role="tablist" aria-label="Symbol selector">
      <span className="symbol-bar-label">Market</span>
      {symbols.map(symbol => (
        <button
          key={symbol}
          id={`symbol-chip-${symbol}`}
          role="tab"
          aria-selected={symbol === selectedSymbol}
          className={`symbol-chip ${symbol === selectedSymbol ? 'active' : ''}`}
          onClick={() => onSymbolChange(symbol)}
        >
          <span>{symbol}</span>
        </button>
      ))}
    </div>
  )
}

export default SymbolSelector
