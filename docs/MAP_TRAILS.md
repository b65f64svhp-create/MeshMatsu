# Map trails — v0.50.beta

Clover opens Maps. T cycles Selected → All → Off, starting with Selected.
Tab changes/recentres the selected node. Current markers remain above trail lines.
The header shows the trail mode; selected-node text explains empty/gapped trails.
Your node is blue, selected remote node green and other paths muted.

Trails link actual dated radio positions. They are approximate straight segments,
not turn-by-turn directions, roads, traceroutes or a reconstruction of every move.
No positions are requested, invented or made more precise. Original shared integer
coordinates and precision are retained. A new path needs two distinct dated fixes.
Repeated stationary fixes coalesce while preserving their actual first/last times.
There is no SD position archive or export in this version.

Retention: the current app session, latest 24 hours, 64 coordinate/precision fixes
per node and 2,048 shared. Oldest last-fix times are evicted first; incoming older
cached anchors cannot replace more recent retained points. Duplicated, conflicting
same-time or older per-node fixes are rejected. NodeInfo supplies at most the
reported dated anchor, without substituting last-heard/arrival time for fix time.
Known stale (>24h), invalid and far-future (>5m) fixes are rejected; small future
skew stays hidden until the clock catches up. Without a usable clock paths stay
hidden. The map uses the same radio-aware clock as recording.

Links require increasing times with <=1 hour from the previous point's last actual
fix to the next point's first actual fix and unchanged position precision. Longer
gaps and precision changes create separate sections. Repeated stationary reports
never create a moving line. Coordinates at zero are valid; absence is distinct.

Recording occurs in radio decoding, independent of map visibility/screen state.
Radio reconnect retains trails; app start/reset clears them. Ignored nodes are
excluded and their history purged on NodeInfo confirmation. Verified remove-node
also purges history. Position packets addressed to another node are excluded.
Current data is never drawn for ignored/unlocated nodes, even if a stored trail
remains temporarily after another radio database change.

Rendering uses the existing Mercator pan/zoom, shortest longitude wrap, viewport
clipping and existing map-worker/cache. It handles crossing segments whose endpoints
are offscreen, date-line paths and repeated low-zoom world tiles. Markers/labels
stay above lines. The map pack and copyright attribution are unchanged.

Checks cover model limits/coalescing/clock/gaps/precision, actual packet/snapshot
integration, selection/all/off and rendering guards/colors/order, safe clipping
and date-line/polar geometry, ignore/remove and unchanged RF/unread behavior.
Device testing still needed: observe moving-node reports, change trail modes,
pan/zoom, receive positions with the display off, reconnect, then restart the app.

## v0.53.beta map visibility filters

On the map press F for All nodes/Favorites, O for All ages/Recent (one hour),
and V for waypoint marker visibility. F uses the radio's existing favorite flag;
it does not modify that flag. Recent requires a valid current clock and a dated
position between now minus 3600 seconds and now, inclusive. Old/unknown/future
locations remain available when All ages is selected. These view choices are
session-only and combine; they do not remove nodes, history or waypoints.

The node counter, current markers, approximate trails, Tab cycling, C recenter and
Enter actions respect the same filters. Hidden selections clear without moving
the map. Opening a waypoint's map view explicitly enables waypoint markers.
The map crosshair remains visible when waypoint markers are hidden.
