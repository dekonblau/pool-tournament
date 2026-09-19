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
 *   First matching Fargo player record, or FALSE on failure. A query like
 *   a common last name can match more than one person (e.g. father/son
 *   sharing a name) -- use pool_tournament_fargo_lookup_all() to see every
 *   match instead of silently taking the first.
 */
function pool_tournament_fargo_lookup($query) {
  $results = _pool_tournament_fargo_request($query);
  return $results ? $results[0] : FALSE;
}

/**
 * Looks up every Fargo record matching a name or member ID.
 *
 * @param string $query
 *   Player full name ("First Last") or Fargo member ID.
 *
 * @return array
 *   All matching Fargo player records (possibly more than one -- e.g. two
 *   people sharing a name), or an empty array if none/on failure.
 */
function pool_tournament_fargo_lookup_all($query) {
  return _pool_tournament_fargo_request($query);
}

/**
 * Shared HTTP request/decode logic for the Fargo indexsearch endpoint.
 *
 * @return array
 *   All matching records ($data['value']), or an empty array on failure or
 *   no matches.
 */
function _pool_tournament_fargo_request($query) {
  $url = 'https://dashboard.fargorate.com/api/indexsearch?q=' . rawurlencode($query);
  $response = backdrop_http_request($url, array(
    'headers' => array('Accept' => 'application/json'),
    'timeout' => 10,
  ));

  if ($response->code != 200) {
    watchdog('pool_tournament', 'Fargo lookup failed (HTTP @code) for query: @q',
      array('@code' => $response->code, '@q' => $query), WATCHDOG_WARNING);
    return array();
  }

  $data = json_decode($response->data, TRUE);
  return empty($data['value']) ? array() : $data['value'];
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
    'member_id'     => isset($fargo_data['readableId'])      ? $fargo_data['readableId']      : NULL,
    // membershipId is a distinct identifier from readableId/member_id --
    // not the same value, both returned by the API.
    'membership_id' => isset($fargo_data['membershipId'])    ? $fargo_data['membershipId']     : NULL,
    // "rating" is not what fargorate.com's own UI (fairmatch.fargorate.com)
    // displays or what dashboard.fargorate.com's player page shows as the
    // player's rating -- that's "effectiveRating". "rating" and
    // "provisionalRating" are different, lower-level values.
    'fargo_rating'  => isset($fargo_data['effectiveRating']) ? $fargo_data['effectiveRating']  : NULL,
    'robustness'    => isset($fargo_data['robustness'])      ? $fargo_data['robustness']       : NULL,
    'location'      => isset($fargo_data['location'])        ? $fargo_data['location']         : NULL,
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
