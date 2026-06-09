<?php
/**
 * @file
 * Bracket Manager API wrapper.
 *
 * Mirrors all endpoints exposed by the bracket manager's server/index.ts.
 * GET requests require no key. POST/PATCH/DELETE require X-Api-Key header.
 *
 * Configuration (admin/config/pool-tournament/settings):
 *   pool_tournament.settings:bracket_api_base  — defaults to http://localhost:3001/api
 *   pool_tournament.settings:bracket_api_key   — the X-Api-Key secret
 */

// ---------------------------------------------------------------------------
// Core request helper
// ---------------------------------------------------------------------------

/**
 * Sends a request to the bracket manager API.
 *
 * @param string $method
 *   HTTP method: GET, POST, PATCH, DELETE.
 * @param string $path
 *   API path, e.g. '/tournaments' or '/stage/5/standings'.
 * @param array|NULL $data
 *   Data to JSON-encode as the request body (for POST/PATCH).
 *
 * @return array|FALSE
 *   Decoded JSON response, or FALSE on error.
 */
function pool_tournament_bracket_request($method, $path, $data = NULL) {
  $base = config_get('pool_tournament.settings', 'bracket_api_base');
  if (empty($base)) {
    $base = 'http://localhost:3001/api';
  }

  $url     = rtrim($base, '/') . $path;
  $options = array('method' => $method, 'headers' => array(), 'timeout' => 15);

  if ($method !== 'GET') {
    $api_key = config_get('pool_tournament.settings', 'bracket_api_key');
    if ($api_key) {
      $options['headers']['X-Api-Key'] = $api_key;
    }
  }

  if ($data !== NULL) {
    $options['headers']['Content-Type'] = 'application/json';
    $options['data'] = json_encode($data);
  }

  $response = backdrop_http_request($url, $options);

  if ($response->code >= 200 && $response->code < 300) {
    return json_decode($response->data, TRUE);
  }

  watchdog('pool_tournament', 'Bracket API @method @path returned @code: @body', array(
    '@method' => $method,
    '@path'   => $path,
    '@code'   => $response->code,
    '@body'   => isset($response->data) ? $response->data : '',
  ), WATCHDOG_ERROR);
  return FALSE;
}

// ---------------------------------------------------------------------------
// Export / data
// ---------------------------------------------------------------------------

/** Returns a full snapshot of all bracket data. */
function pool_tournament_bracket_export() {
  return pool_tournament_bracket_request('GET', '/export');
}

// ---------------------------------------------------------------------------
// Tournaments
// ---------------------------------------------------------------------------

/** Lists all tournaments. */
function pool_tournament_bracket_get_tournaments() {
  return pool_tournament_bracket_request('GET', '/tournaments');
}

/**
 * Creates a tournament in the bracket manager.
 *
 * @param string $name Tournament name.
 * @return array|FALSE  Created tournament record with 'id'.
 */
function pool_tournament_bracket_create_tournament($name) {
  return pool_tournament_bracket_request('POST', '/tournament', array('name' => $name));
}

/**
 * Renames a tournament.
 *
 * @param int    $id   Bracket manager tournament ID.
 * @param string $name New name.
 */
function pool_tournament_bracket_rename_tournament($id, $name) {
  return pool_tournament_bracket_request('PATCH', '/tournament/' . $id, array('name' => $name));
}

/**
 * Marks a tournament as started (TBD slots become BYEs).
 *
 * @param int $id Bracket manager tournament ID.
 */
function pool_tournament_bracket_start_tournament($id) {
  return pool_tournament_bracket_request('POST', '/tournament/' . $id . '/start');
}

/**
 * Deletes a tournament and all its stages/matches.
 *
 * @param int $id Bracket manager tournament ID.
 */
function pool_tournament_bracket_delete_tournament($id) {
  return pool_tournament_bracket_request('DELETE', '/tournament/' . $id);
}

/** Returns all stages for a tournament. */
function pool_tournament_bracket_get_stages($tournament_id) {
  return pool_tournament_bracket_request('GET', '/tournament/' . $tournament_id . '/stages');
}

// ---------------------------------------------------------------------------
// Stages
// ---------------------------------------------------------------------------

/**
 * Creates a bracket stage.
 *
 * @param int    $tournament_id Bracket manager tournament ID.
 * @param string $name          Stage name.
 * @param string $type          'single_elimination', 'double_elimination', or 'round_robin'.
 * @param array  $settings      Stage settings (grandFinal, size, seedOrdering, etc.).
 * @param array  $seeding       Optional array of participant name strings.
 */
function pool_tournament_bracket_create_stage($tournament_id, $name, $type, $settings = array(), $seeding = array()) {
  $payload = array(
    'tournamentId' => (int) $tournament_id,
    'name'         => $name,
    'type'         => $type,
    'settings'     => $settings,
  );
  if (!empty($seeding)) {
    $payload['seeding'] = $seeding;
  }
  return pool_tournament_bracket_request('POST', '/stage', $payload);
}

/**
 * Deletes a stage.
 *
 * @param int $stage_id Bracket manager stage ID.
 */
function pool_tournament_bracket_delete_stage($stage_id) {
  return pool_tournament_bracket_request('DELETE', '/stage/' . $stage_id);
}

/** Returns participants registered for a stage. */
function pool_tournament_bracket_get_stage_participants($stage_id) {
  return pool_tournament_bracket_request('GET', '/stage/' . $stage_id . '/participants');
}

// ---------------------------------------------------------------------------
// Seeding
// ---------------------------------------------------------------------------

/**
 * Updates the seeding for a stage.
 *
 * @param int   $stage_id Stage ID.
 * @param array $seeding  Array of participant name strings or NULLs for TBD.
 */
function pool_tournament_bracket_update_seeding($stage_id, $seeding) {
  return pool_tournament_bracket_request('PATCH', '/seeding/' . $stage_id, array('seeding' => $seeding));
}

/**
 * Resets the seeding for a stage.
 *
 * @param int $stage_id Stage ID.
 */
function pool_tournament_bracket_reset_seeding($stage_id) {
  return pool_tournament_bracket_request('POST', '/stage/' . $stage_id . '/reset-seeding');
}

// ---------------------------------------------------------------------------
// Participants
// ---------------------------------------------------------------------------

/**
 * Adds participants to a tournament.
 *
 * @param int   $tournament_id Bracket manager tournament ID.
 * @param array $names         Array of participant name strings.
 */
function pool_tournament_bracket_add_participants($tournament_id, $names) {
  return pool_tournament_bracket_request('POST', '/participant', array(
    'tournamentId' => (int) $tournament_id,
    'names'        => $names,
  ));
}

/**
 * Renames a participant.
 *
 * @param int    $participant_id Bracket manager participant ID.
 * @param string $name           New name.
 */
function pool_tournament_bracket_rename_participant($participant_id, $name) {
  return pool_tournament_bracket_request('PATCH', '/participant/' . $participant_id, array('name' => $name));
}

/**
 * Removes a participant.
 *
 * @param int $participant_id Bracket manager participant ID.
 */
function pool_tournament_bracket_remove_participant($participant_id) {
  return pool_tournament_bracket_request('DELETE', '/participant/' . $participant_id);
}

// ---------------------------------------------------------------------------
// Matches
// ---------------------------------------------------------------------------

/**
 * Updates a match result.
 *
 * @param int   $match_id  Bracket manager match ID.
 * @param int   $status    Match status (2=Ready, 3=Running, 4=Completed, 5=Archived).
 * @param array $opponent1 Score/result for opponent 1: array('score'=>N,'forfeit'=>bool,'result'=>'win'|'loss').
 * @param array $opponent2 Score/result for opponent 2.
 */
function pool_tournament_bracket_update_match($match_id, $status, $opponent1, $opponent2) {
  return pool_tournament_bracket_request('PATCH', '/match', array(
    'id'        => (int) $match_id,
    'status'    => (int) $status,
    'opponent1' => $opponent1,
    'opponent2' => $opponent2,
  ));
}

/**
 * Resets a match result.
 *
 * @param int $match_id Bracket manager match ID.
 */
function pool_tournament_bracket_reset_match($match_id) {
  return pool_tournament_bracket_request('POST', '/match/' . $match_id . '/reset');
}

/** Returns currently active matches for a stage. */
function pool_tournament_bracket_get_current_matches($stage_id) {
  return pool_tournament_bracket_request('GET', '/stage/' . $stage_id . '/current-matches');
}

// ---------------------------------------------------------------------------
// Standings
// ---------------------------------------------------------------------------

/** Returns final standings for a stage. */
function pool_tournament_bracket_get_standings($stage_id) {
  return pool_tournament_bracket_request('GET', '/stage/' . $stage_id . '/standings');
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

/**
 * Returns TRUE if the bracket manager API is reachable.
 */
function pool_tournament_bracket_ping() {
  $result = pool_tournament_bracket_request('GET', '/tournaments');
  return $result !== FALSE;
}
