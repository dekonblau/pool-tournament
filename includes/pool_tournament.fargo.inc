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
 * @return array|FALSE
 *   All matching Fargo player records (possibly more than one -- e.g. two
 *   people sharing a name), an empty array if the lookup succeeded but
 *   found no matches, or FALSE if the Fargo service itself couldn't be
 *   reached. See _pool_tournament_fargo_request() -- callers that need to
 *   tell "service down" apart from "no matches" must check `=== FALSE`.
 */
function pool_tournament_fargo_lookup_all($query) {
  return _pool_tournament_fargo_request($query);
}

/**
 * Shared HTTP request/decode logic for the Fargo indexsearch endpoint.
 *
 * @return array|FALSE
 *   All matching records ($data['value']), possibly an empty array if the
 *   request succeeded but found nothing -- OR FALSE if the request itself
 *   failed (non-200 response, connection error, timeout). These are NOT
 *   the same thing: "the service is unreachable" and "the service
 *   responded and found no match" need different handling by callers that
 *   run an automated batch (e.g. a fargo-limit eligibility check across
 *   every player for a new tournament) -- a down service should stop the
 *   whole batch with a surfaced error, not get silently treated as
 *   "everyone is ineligible". Check `=== FALSE` explicitly; don't rely on
 *   emptiness/falsiness alone to tell the two apart.
 */
function _pool_tournament_fargo_request($query) {
  $url = 'https://dashboard.fargorate.com/api/indexsearch?q=' . rawurlencode($query);
  $response = backdrop_http_request($url, array(
    'headers' => array('Accept' => 'application/json'),
    'timeout' => 10,
  ));

  if ($response->code != 200) {
    watchdog('pool_tournament', 'Fargo lookup failed (HTTP @code) for query: @q',
      array('@code' => $response->code, '@q' => $query), WATCHDOG_ERROR);
    return FALSE;
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
    // readableId is the player's global FargoRate identifier -- usable as
    // the query itself against this same indexsearch endpoint.
    'member_id'     => isset($fargo_data['readableId'])      ? $fargo_data['readableId']      : NULL,
    // membershipId identifies a membership with a specific sanctioning body
    // (e.g. a particular pool league), not the player globally -- unlike
    // readableId, it is NOT a valid query against this endpoint (confirmed:
    // querying by membershipId returns no results, querying by readableId
    // does). Captured for record-keeping only.
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
