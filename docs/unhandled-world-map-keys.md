# World Map Keys Without Accessibility Handling

Game keys on the world map that currently have no screen reader announcements.

## Medium Priority

(none)

## Low Priority

(none)

## Resolved

* **H** — Hold Position: now announces "Holding." (direct key handler)
* **X** — Explore: now announces "Auto-exploring." (direct key handler, only when unit selected)
* **Shift+R** — Research Priorities: now announces "Research priorities." before opening dialog
* **] / [** — Raise/Lower terrain: already handled by terraform order polling (ORDER_TERRAFORM_UP/DOWN)
* **Z/X** — Zoom: purely visual, no SR needed (X only fires explore when unit selected)
* **+** — Create Group: announces "Group dialog." before opening (game handles dialog, text capture annotates)
* **Ctrl+G** — Grid toggle: announces "Grid on/off" after toggling
* **T** — Terrain toggle: announces "Flat terrain on/off" after toggling
* **C** — Center on unit: announces "Centered." after centering
* **F9** — View Monuments: announces "Monuments." before opening
* **F10** — Hall of Fame: announces "Hall of Fame." before opening
