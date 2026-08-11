<?php
/**
 * @file
 * Fargo Rate API integration.
 *
 * API endpoint: https://dashboard.fargorate.com/api/indexsearch?q=<query>
 * Accepts player name ("Jeff Adrian") or member ID ("310807").
 */

/**
 * Looks up a player on fargorate.com by name or member ID.
 *
 * @param string $query
 *   Player full name ("First Last") or Fargo member ID.
 *
 * @return array|FALSE
 *   First matching Fargo player record, or FALSE on failure.
 */
function pool_tournament_fargo_lookup($query) {
  $url = 'https://dashboard.fargorate.com/api/indexsearch?q=' . rawurlencode($query);
  $response = backdrop_http_request($url, array(
    'headers' => array('Accept' => 'application/json'),
    'timeout' => 10,
  ));

  if ($response->code != 200) {
    watchdog('pool_tournament', 'Fargo lookup failed (HTTP @code) for query: @q',
      array('@code' => $response->code, '@q' => $query), WATCHDOG_WARNING);
    return FALSE;
  }

  $data = json_decode($response->data, TRUE);
  if (empty($data['value'])) {
    return FALSE;
  }

  return $data['value'][0];
}

/**
 * Looks up a player by first and last name.
 */
function pool_tournament_fargo_lookup_by_name($first_name, $last_name) {
  return pool_tournament_fargo_lookup(trim($first_name . ' ' . $last_name));
}

/**
 * Looks up a player by Fargo member ID.
 */
function pool_tournament_fargo_lookup_by_id($member_id) {
  return pool_tournament_fargo_lookup((string) $member_id);
}

/**
 * Maps a Fargo API response record to player entity field values.
 *
 * @param array $fargo_data
 *   A single record from $data['value'] returned by the Fargo API.
 *
 * @return array
 *   Associative array of player field values.
 */
function pool_tournament_fargo_map_to_player(array $fargo_data) {
  return array(
    'member_id'    => isset($fargo_data['readableId'])   ? $fargo_data['readableId']   : NULL,
    'fargo_rating' => isset($fargo_data['rating'])       ? $fargo_data['rating']       : NULL,
    'robustness'   => isset($fargo_data['robustness'])   ? $fargo_data['robustness']   : NULL,
    'location'     => isset($fargo_data['location'])     ? $fargo_data['location']     : NULL,
  );
}

/**
 * Updates a player entity with data from the Fargo API.
 *
 * @param int $player_id
 *   The player entity ID to update.
 * @param string $query
 *   Name or member ID to look up on Fargo.
 *
 * @return bool
 *   TRUE if the player was updated, FALSE otherwise.
 */
function pool_tournament_fargo_sync_player($player_id, $query) {
  $fargo = pool_tournament_fargo_lookup($query);
  if (!$fargo) {
    return FALSE;
  }

  $player = entity_load('player', $player_id);
  if (!$player) {
    return FALSE;
  }

  $mapped = pool_tournament_fargo_map_to_player($fargo);
  foreach ($mapped as $key => $value) {
    $player->$key = $value;
  }
  $player->save();
  return TRUE;
}
