import React from 'react'

/**
 * AlertToasts — renders a stack of floating toast notifications.
 *
 * Types:
 * - 'SPREAD': yellow (Spread widened)
 * - 'LARGE_PRINT': blue (Large trade size)
 * - 'OBI': red/green (Order Book Imbalance extreme)
 */
function AlertToasts({ alerts }) {
  if (!alerts || alerts.length === 0) return null

  const getIcon = (type) => {
    switch (type) {
      case 'SPREAD': return '⚠️'
      case 'LARGE_PRINT': return '🔥'
      case 'OBI': return '⚖️'
      default: return '🔔'
    }
  }

  const getColorClass = (type, meta) => {
    switch (type) {
      case 'SPREAD': return 'toast-yellow'
      case 'LARGE_PRINT': return 'toast-blue'
      case 'OBI': return meta === 'buy' ? 'toast-green' : 'toast-red'
      default: return 'toast-gray'
    }
  }

  return (
    <div className="alert-toast-container" aria-live="assertive">
      {alerts.map((alert) => {
        const colorClass = getColorClass(alert.type, alert.meta)
        return (
          <div key={alert.id} className={`alert-toast ${colorClass}`}>
            <div className="toast-icon">{getIcon(alert.type)}</div>
            <div className="toast-content">
              <div className="toast-title">{alert.title}</div>
              <div className="toast-message">{alert.message}</div>
            </div>
            {/* Progress bar for auto-dismiss visualization */}
            <div className="toast-progress-bar" />
          </div>
        )
      })}
    </div>
  )
}

export default AlertToasts
