import React from 'react'

const IconDatabase = () => (
  <svg className="card-title-icon" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5">
    <ellipse cx="8" cy="4" rx="6" ry="2" />
    <path d="M2 4v8c0 1.1 2.7 2 6 2s6-.9 6-2V4" />
    <path d="M2 8c0 1.1 2.7 2 6 2s6-.9 6-2" />
  </svg>
)

const IconGrid = () => (
  <svg className="card-title-icon" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.5">
    <rect x="2" y="2" width="4" height="4" rx="1" />
    <rect x="10" y="2" width="4" height="4" rx="1" />
    <rect x="2" y="10" width="4" height="4" rx="1" />
    <rect x="10" y="10" width="4" height="4" rx="1" />
  </svg>
)

export default function MemoryPoolPanel({ debugData }) {
  const stats = debugData?.poolStats || { usedSlots: 0, freeSlots: 1000000, totalSlots: 1000000, nextFreeIdx: 0, usedPct: 0 }
  const occupiedSlots = new Set(debugData?.occupiedSlots || [])
  const freeListHead = debugData?.freeListHead || [0, 1, 2, 3, 4, 5, 6, 7]

  // Render a 4x10 grid (40 slots) for visual representation
  const TOTAL_GRID_SLOTS = 40
  const gridSlots = Array.from({ length: TOTAL_GRID_SLOTS }, (_, i) => i)

  return (
    <div className="memory-pool-container">
      <div className="memory-pool-desc">
        <h2 className="section-title">Memory Pool State</h2>
        <p>
          The engine pre-allocates one million <code>OrderNode</code> slots at startup. Every order allocation and deallocation is an O(1) array index operation — no OS heap calls on the hot path.
        </p>
      </div>

      <div className="memory-pool-layout">
        {/* Left Stats Panel */}
        <div className="pool-stats-card card">
          <div className="card-header" style={{ borderBottom: '1px solid var(--border-default)', paddingBottom: '12px' }}>
            <div className="card-title"><IconDatabase /> Pool Statistics</div>
          </div>
          <div className="card-body pool-stats-body">
            
            <div className="stat-group">
              <div className="stat-row">
                <span className="stat-label">Used Slots</span>
                <span className="stat-value">{(stats.usedPct || 0).toFixed(2)}%</span>
              </div>
              <div className="stat-progress-bar">
                <div className="stat-progress-fill" style={{ width: `${Math.max(stats.usedPct || 0, 1)}%` }}></div>
              </div>
            </div>

            <div className="stat-big">
              <div className="stat-big-val">{stats.usedSlots}</div>
              <div className="stat-big-lbl">ACTIVE ORDERS</div>
            </div>

            <div className="stat-big">
              <div className="stat-big-val">{stats.freeSlots}</div>
              <div className="stat-big-lbl">FREE SLOTS</div>
            </div>

            <div className="stat-big">
              <div className="stat-big-val">{stats.nextFreeIdx}</div>
              <div className="stat-big-lbl">NEXT FREE IDX</div>
            </div>

            <div className="free-list-group">
              <div className="stat-label">Free List Head Pointer</div>
              <div className="free-list-boxes">
                {freeListHead.slice(0, 7).map((idx, i) => (
                  <div key={i} className={`free-box ${i === 0 ? 'head' : ''}`}>{idx}</div>
                ))}
              </div>
            </div>

          </div>
        </div>

        {/* Right Grid Panel */}
        <div className="pool-grid-card card">
          <div className="card-header" style={{ borderBottom: '1px solid var(--border-default)', paddingBottom: '12px' }}>
             <div className="card-title"><IconGrid /> Occupied Slots</div>
          </div>
          <div className="card-body">
             <div className="slots-grid">
               {gridSlots.map(slotIdx => {
                  const isOccupied = occupiedSlots.has(slotIdx)
                  return (
                    <div key={slotIdx} className={`slot-box ${isOccupied ? 'occupied' : 'free'}`}>
                      [{slotIdx}]
                    </div>
                  )
               })}
             </div>
             <div className="slots-grid-desc">
               The physical indices shown are the actual array positions in the one-million-element arena that currently hold live orders. When an order is cancelled the slot returns to the free-list head and will be handed out first on the next allocation.
             </div>
          </div>
        </div>

      </div>
    </div>
  )
}
