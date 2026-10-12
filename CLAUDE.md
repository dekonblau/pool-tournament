# pool_tournament — Backdrop CMS Module

## What this project is
Backdrop CMS 1.x module that manages pool tournaments. It connects to a separate
bracket manager app (React/Node.js) running on saturn (192.168.0.173) via REST API.

## Servers
- **uranus** (192.168.0.174) — this machine; Apache2, Backdrop CMS, webroot `/var/www/backdrop`, URL `kipspool.itadria.net`
- **saturn** (192.168.0.173) — bracket manager app; Node.js/Express on port 3001 (API), Vite on port 5173 (React frontend)
- SSH from uranus to saturn: `ssh dekonblau@192.168.0.173`
- SSH from saturn to uranus: `ssh dekonblau@192.168.0.174`

## Module location
`/var/www/backdrop/modules/custom/pool_tournament/`
Git repo initialized here (first commit ec318d8). Backdrop 1.x, no drush — use `bee` CLI instead.

## Bracket Manager API (saturn)
- Base URL (from uranus): `http://192.168.0.173:3001/api`
- Auth: `X-Api-Key` header on all POST/PATCH/DELETE; GETs are public
- API key stored in Backdrop config: `pool_tournament.settings:bracket_api_key`
- Configured at: `kipspool.itadria.net/admin/config/pool-tournament/settings`
- PHP wrapper: `includes/pool_tournament.bracket.inc` — covers all endpoints
- To inspect saturn's API source: `ssh dekonblau@192.168.0.173 "cat /var/www/bracket-manager/server/index.ts"`

## Bracket Manager API endpoints (all prefixed /api)
- GET    /tournaments
- POST   /tournament               { name }
- PATCH  /tournament/:id           { name }
- POST   /tournament/:id/start
- DELETE /tournament/:id
- GET    /tournament/:id/stages
- POST   /stage                    { tournamentId, name, type, settings, seeding? }
- DELETE /stage/:id
- GET    /stage/:id/participants
- GET    /stage/:id/standings
- GET    /stage/:id/current-matches
- PATCH  /seeding/:stageId         { seeding: string[] }
- POST   /stage/:id/reset-seeding
- POST   /participant              { tournamentId, names: string[] }
- PATCH  /participant/:id          { name }
- DELETE /participant/:id
- PATCH  /match                    { id, status, opponent1, opponent2 }
- POST   /match/:id/reset
- GET    /export

## Architecture

### Tournament = node content type (not custom entity)
Gets Views, layouts, path aliases for free. Fields on tournament node:
- `body` — description
- `field_bracket_manager_id` — int, links to bracket manager tournament ID
- `field_game_type_id` — int, references game_type entity
- `field_player_type_id` — int, references player_type entity
- `field_tournament_type_id` — int, references tournament_type entity
- `field_venue_id` — int, references venue entity
- `field_max_players` — int
- `field_registration_open` — boolean
- `field_tournament_date` — datetime

### Custom entities (5 total, all fieldable via entity_plus)
All follow the ed4 pattern: EntityPlusController, bundles stored as config files,
loaded dynamically in `hook_entity_info_alter()` via `config_get_names_with_prefix()`.

**Architecture note (IN PROGRESS):** Base table columns vs Field API fields is being
reconsidered. Currently some properties are base table columns, others are Field API.
The right split needs to be determined before further schema work.

1. **player** — pool player
   - Base table: `pool_tournament_player`
   - Base columns: player_id, bundle, label, uid, member_id, fargo_rating, robustness, location, status, created, changed
   - Bundles: `player.bundle.player` (one default bundle)
   - Fargo Rate API integration for lookup by name or member ID

2. **player_type** — type/format of play (OO analogy: subtype of player_type)
   - Base table: `pool_tournament_player_type`
   - Bundles: singles, partners, partners_general, partners_mixed, partners_scotch, partners_mixed_scotch, team
   - Each bundle has different fields (e.g. partners has players_per_side=2)

3. **game_type** — the pool game being played
   - Base table: `pool_tournament_game_type`
   - Bundles: 8ball, 8ball_bca, 8ball_3and1, 8ball_bank, 9ball, 9ball_rotating, 10ball, one_pocket, cribbage, hybrid, straight

4. **tournament_type** — format/handicap structure
   - Base table: `pool_tournament_tournament_type`
   - Bundles: open, handicapped, fargo_limit, fargo_limit_handicapped

5. **venue** — physical location
   - Base table: `pool_tournament_venue`
   - Base columns include: address, city, state, phone
   - Bundles: none predefined (admin creates as needed)

### Player preferences (M2M)
Table: `pool_tournament_player_preference` (player_id, preference_type, entity_id)
preference_type is one of: game_type, player_type, tournament_type, venue
Used by `hook_views_query_alter` to filter `/tournaments` listing for logged-in player.

## Key files
- `pool_tournament.module` — all hooks, menu, entity info, page callbacks
- `pool_tournament.install` — schema for all 6 tables + hook_install/uninstall
- `pool_tournament.info` — module metadata
- `entities/Player.php` — Player entity class
- `entities/PlayerType.php`, `GameType.php`, `TournamentType.php`, `Venue.php`
- `includes/pool_tournament.admin.inc` — settings form, bundle CRUD
- `includes/pool_tournament.bracket.inc` — bracket manager API wrapper
- `includes/pool_tournament.fargo.inc` — Fargo Rate API (fargorate.com)
- `includes/pool_tournament.forms.inc` — player form, preferences form, generic entity form
- `config/` — 24 bundle JSON files + pool_tournament.settings.json

## Fargo Rate API
URL: `https://dashboard.fargorate.com/api/indexsearch?q=<name_or_memberid>`
Returns JSON; `value[0]` has: readableId, rating, robustness, location, firstName, lastName.
PHP wrapper: `includes/pool_tournament.fargo.inc`

## Admin routes
- `admin/config/pool-tournament/settings` — API key + base URL + ping
- `admin/structure/player-bundle` — player entity bundle management
- `admin/structure/player-type-bundle` — player_type bundle management
- `admin/structure/game-type-bundle` — game_type bundle management
- `admin/structure/tournament-type-bundle` — tournament_type bundle management
- `admin/structure/venue-bundle` — venue bundle management

## Entity instance routes
- `player` / `player/add` / `player/%id` / `player/%id/edit`
- `player-type` / `player-type/add` / `player-type/%id` / `player-type/%id/edit`
- `game-type` / `game-type/add` / `game-type/%id`
- `tournament-type` / `tournament-type/add` / `tournament-type/%id`
- `venue` / `venue/add` / `venue/%id`
- `tournaments` — upcoming tournaments (filtered by player preferences if logged in)

## Cache clearing
```bash
cd /var/www/backdrop && bee cache-clear
# Select 0 for All
```

## Open items
1. Entity base table vs Field API field split — user is defining which fields belong where;
   schema refactor pending that decision.
2. Reference entity instances: player_type (7/7 bundles) and game_type (11/11 bundles) are
   fully seeded. tournament_type has all 4 bundles. venue has one placeholder row
   ("Kip's Pool Hall", fake address) awaiting real data.
3. Views config JSON (`pool_tournament_upcoming`) for `/tournaments` — currently falls back
   to a bare SQL query. Alternatively improve the fallback query directly.
4. ~~Ref entity add routes may have same shim bug as `player/add/%bundle`~~ — checked
   2026-09-28: already fixed generically. All 5 types share `pool_tournament_ref_entity_
   add_form_page()`, which always wraps the raw bundle string via `entity_create()` before
   handing off to the form. Verified live for all 5 entity types, no bug found.
