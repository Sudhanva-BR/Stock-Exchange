import React from 'react'

const IconLink = () => (
  <svg className="card-title-icon" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5">
    <path d="M7 9l2-2" />
    <path d="M5.5 11.5L4 13a2.828 2.828 0 1 1-4-4l1.5-1.5" />
    <path d="M10.5 4.5L12 3a2.828 2.828 0 1 1 4 4l-1.5 1.5" />
  </svg>
)

export default function NodeDataPanel({ debugData }) {
  const activeNodes = debugData?.activeNodes || []
  const priceLevels = debugData?.priceLevels || []

  const fmtPrice = (p) => typeof p === 'number' ? p.toFixed(2) : '—'
  const fmtQty = (q) => typeof q === 'number' ? q.toLocaleString() : '—'

  return (
    <div className="node-data-container">
      <div className="node-data-desc">
        <h2 className="section-title">Node Data — Raw OrderNode Structs</h2>
        <p>
          Every resting order is stored as an <code>OrderNode</code> in the pool arena. The <code>prev_idx</code> and <code>next_idx</code> fields form the doubly linked list that encodes time priority at each price level.
        </p>
      </div>

      <div className="node-data-layout">
        {/* Top: Active Nodes Table */}
        <div className="card node-table-card">
          <div className="card-header" style={{ borderBottom: '1px solid var(--border-default)', paddingBottom: '12px' }}>
             <div className="card-title"><IconLink /> Active Nodes (first 10)</div>
             <span className="card-badge live">LIVE</span>
          </div>
          <div className="card-body">
             <table className="node-table">
                <thead>
                  <tr>
                    <th>SLOT</th>
                    <th>ORDER ID</th>
                    <th>SIDE</th>
                    <th>PRICE</th>
                    <th>QTY</th>
                    <th>PREV SLOT</th>
                    <th>NEXT SLOT</th>
                  </tr>
                </thead>
                <tbody>
                  {activeNodes.length === 0 ? (
                    <tr>
                      <td colSpan="7" className="text-center">No active nodes</td>
                    </tr>
                  ) : (
                    activeNodes.slice(0, 10).map((node, i) => (
                      <tr key={i}>
                        <td className="slot-col">{node.slot}</td>
                        <td className="id-col">#{node.orderId || (node.slot + 1)}</td>
                        <td className={node.side === 'B' ? 'text-green' : 'text-red'}>{node.side}</td>
                        <td className="price-col">${fmtPrice(node.price)}</td>
                        <td>{fmtQty(node.qty)}</td>
                        <td>{node.prevSlot === -1 ? 'NONE' : node.prevSlot}</td>
                        <td>{node.nextSlot === -1 ? 'NONE' : node.nextSlot}</td>
                      </tr>
                    ))
                  )}
                </tbody>
             </table>
          </div>
        </div>

        {/* Bottom: Linked List Visual */}
        <div className="linked-list-visual">
           {priceLevels.map((lvl, i) => (
             <div key={i} className="price-level-row">
                <div className={`price-badge ${lvl.side === 'B' ? 'bid' : 'ask'}`}>
                  ${fmtPrice(lvl.price)}
                </div>
                
                {/* Node visualization per price level */}
                <div className="node-box">
                  <div className="node-box-slot">Slot [{lvl.slot}]</div>
                  <div className="node-box-info">#{lvl.slot + 1} | {lvl.qty}</div>
                </div>
                
                {lvl.nextSlot !== -1 && (
                  <div className="node-arrow">→</div>
                )}
             </div>
           ))}
        </div>
      </div>
    </div>
  )
}
